## Context

Display_HMI, LVGL 8.3, ESP32-S3 (CrowPanel 7", 800x480, 16 MB flash, 8 MB PSRAM
OPI). Estado de partida, verificado en el código:

- `TelemetryHistory` (`src/ui/TelemetryHistory.cpp`) es un overlay sobre
  `lv_layer_top()` con tres gráficos (aire, piel, humedad) alimentados desde
  tres anillos `float` de 1440 ranuras de 10 s (4 h) en PSRAM. Solo guarda
  medidas. `TelemetryHistory_Open()` **no tiene ningún llamador**.
- **No hay sistema de ficheros montado** en el HMI. La partición `spiffs`
  (0xA10000, 0x5E0000, ~5,9 MB) existe pero nadie la usa en producción
  (`AudioManager.cpp`, el único `SPIFFS.begin`, está excluido del build).
- La SRAM interna es el recurso escaso: el panel RGB necesita dos bounce
  buffers de 38,4 KB contiguos y DMA-capaces que se piden en `UI_Task` tras
  arrancar WiFi. `LV_MEM_CUSTOM` ya reserva todo el heap de LVGL en PSRAM
  (`lv_conf.h:62-66`), así que los `y_points` de los gráficos tampoco gastan
  SRAM.
- Regla **ARQ-LOCK-001**: LittleFS/NVS/I2C fuera de `LVGL_Lock`; mutaciones
  de widgets dentro. `UI_Task` ya decide dentro del lock y escribe fuera
  (`doNVSWrite`, `UITask.cpp:4991-5019`).
- La hora (`HMI_GetEpochNow()`, UTC) es 0 hasta el primer `CTRL,TIME` de la
  motherBoard, que es la única autoridad de hora.

## Goals / Non-Goals

**Goals**

- Marcadores de alarma (alta/baja) y línea de consigna histórica en los tres
  gráficos.
- Ventanas 4 h / 12 h / 24 h con un único buffer de 24 h.
- La tendencia sobrevive a un reinicio y a un corte de red eléctrica
  (pérdida máxima acotada y documentada).
- Lógica pura testeada en host; el modelo no usa SRAM interna.

**Non-Goals**

- Cambios de protocolo, de `shared/` o de la motherBoard.
- Repoblar marcas desde `CTRL,ALM_HISTORY` tras un reinicio (los eventos se
  persisten en el propio fichero; ver §Eventos). Queda como deuda anotada.
- Restaurar el botón TREND de la pantalla de bloqueo (se retiró a petición).
- Retirar `ui_ScreenCharts` (la pantalla antigua queda inalcanzable, como ya
  lo estaba; limpiarla es otro cambio).
- Reparar el hueco de datos de un corte *largo* de enlace: sigue siendo
  "sin dato" (NaN), como hoy.

## Decisions

### Punto de entrada: botón "TENDENCIA" en el centro de alarmas

`docs/hmi.md` no dice desde dónde se abre `TelemetryHistory`; solo el
comentario de `TelemetryHistory.h` ("accesible desde `ui_ScreenLock` sin
desbloquear") y la lección E9 de formación, ambos del botón de bloqueo que se
retiró a petición para la tanda del 2026-09-17 (`8b62b23b`, que deja dicho
"si se recupera el botón, hay que devolver también esa lección"). El
criterio por defecto era hacer que el botón de gráficos de Main abra la
tendencia. **No es viable**, por dos hechos verificados:

1. `ui_ChartButton` está oculto desde siempre (`ElementsCreation.cpp:1999`;
   la línea que lo ocultaba en `UITask.cpp:4513` está comentada, pero la
   de la creación se mantiene) y su posición (x=138, y=-214 desde el centro)
   **pisa a `ui_BabiesButton`** (x=123, 110 px de ancho).
2. El heading no tiene hueco: el lado derecho está ya al paso mínimo que
   permite la zona táctil ampliada del candado
   (`ElementsCreation.cpp:920-949`, y el mismo argumento está en el
   `design.md` archivado de `hmi-boton-ayuda`). Meter un botón obliga a
   redistribuir un heading que no se puede ver ni probar en este entorno.

Decisión: **botón "TENDENCIA" en la cabecera de `AlarmCenter`**, a la derecha
del botón TEST (el hueco izquierdo ya es el de TEST; el título está
centrado y la X a la derecha, queda sitio de sobra). Razones:

- Es el sitio donde ya se piensa en alarmas, y el propósito de los
  marcadores es justamente correlacionar alarmas con medidas.
- Se alcanza en **un toque** desde Main (icono de alarmas o check de "todo
  OK"), y también desde el icono de alarma de la pantalla de bloqueo sin
  desbloquear.
- Cero cambios de layout en Main o en el heading.
- `AlarmCenter` se queda abierto *debajo* de la tendencia: al cerrarla, o
  cuando la tendencia cede la pantalla por una alarma, el operador aterriza
  en el centro de alarmas, que es donde se atiende.

La política de cesión de `TelemetryHistory` (`mustYield()`: se cierra con
**cualquier** alarma activa o enlace caído, para no tapar el banner) **no se
toca**: la tendencia es una herramienta retrospectiva y de consulta. Con una
alarma activa el botón no hace nada visible (`Open()` ya rechaza abrir), lo
que se documenta en el spec.

`ui_ChartButton` y `ui_ScreenCharts` quedan como están (inalcanzables).
Deuda anotada para el `scribe`/retro.

### Modelo de datos: un bloque PSRAM, 6 series `int16` en décimas

```
bloque único  = heap_caps_malloc(6 * 8640 * 2 B, MALLOC_CAP_SPIRAM)  = 103 680 B
series (planar, cada una un anillo de 8640):
  0 aire    1 piel    2 humedad        (medida, décimas: 36.7 C -> 367)
  3 cons.aire  4 cons.piel  5 cons.hum (consigna, décimas)
sin dato     = INT16_MIN (reemplaza al NaN de los float)
```

Se pasa de `float` a `int16` en décimas porque el gráfico ya redondeaba a
décimas (`lroundf(x * Y_SCALE)`, `Y_SCALE = 10`): no se pierde nada visible,
las seis series de 24 h caben en la mitad (104 KB en vez de los 207 KB de
`float`) y el fichero y el anillo comparten representación (sin conversión al
volcar ni al restaurar). Una única reserva: un fallo parcial dejaría el
histórico a medias (mismo razonamiento que `ensureBuffers()` actual). Si no
hay PSRAM la tendencia se queda vacía, el equipo monitoriza igual.

Los índices del anillo siguen siendo el tiempo: una ranura por 10 s
absolutos; una ranura sin telemetría se rellena con "sin dato" en las seis
series (también la consigna: sin telemetría tampoco hay consigna conocida).

Memoria resumida:

| Qué | Tamaño | Dónde | Vida |
|---|---|---|---|
| Modelo: anillo (6 x 8640 x int16) | 103 680 B | PSRAM | permanente |
| Modelo: eventos (64 x 16 B) + reloj + cabecera | ~1,1 KB | PSRAM (mismo bloque) | permanente |
| Cola de flancos (16 x 16 B) | 256 B | `.bss` | permanente |
| Staging de volcado/restauración | <= 104 224 B | PSRAM | durante el volcado |
| `y_points` LVGL (6 series x 1442 x 2 B) | 17 304 B | PSRAM (`LV_MEM_CUSTOM`) | permanente |

El modelo entero (estructura + anillo) es **un único `heap_caps_malloc`** en
`MALLOC_CAP_SPIRAM`, reservado en `TelemetryHistory_Init()`. La única
estructura nueva en SRAM interna es la cola de flancos de 256 B: la escribe
`CommTask` bajo una sección crítica de unos pocos ciclos y no puede apuntar a
un modelo que pertenece a `UI_Task`. Además el código nuevo (estado de
`trend_persist`, `hmi_fs`, estáticos de la librería LittleFS) añade `.bss`; el
delta total de RAM del build se mide y se reporta en `tasks.md`.

### Reloj: de `esp_timer` a epoch con transición limpia

`nowSlot()` era `esp_timer_get_time()/1000/SLOT_MS`: reinicia en 0 en cada
arranque, así que un fichero no se puede alinear con él. Nuevo reloj
(`trend_clock`): `slot = (monoMs / SLOT_MS) + offsetSlots`.

- **Sin hora** (`HMI_GetEpochNow() == 0`): `offsetSlots = 0`, es decir, el
  comportamiento actual. Los datos de antes de tener hora se guardan con
  ranuras "monótonas" pequeñas.
- **Primera hora válida**: `offsetSlots = epoch/10 - monoSlot`. El reloj
  devuelve el desplazamiento y el modelo hace `lastSlot += desplazamiento`
  (y lo mismo con los eventos): los datos de antes de la hora **conservan su
  separación relativa**, solo se re-etiquetan con su hora real. No se
  pierde ni se duplica ninguna muestra.
- **Correcciones posteriores de hora** (NTP, ajuste manual): si la diferencia
  respecto al offset vigente es mayor de 1 ranura, se aplica el mismo
  desplazamiento (positivo o negativo). Una diferencia <= 1 ranura es
  redondeo de `millis()` frente a `esp_timer` y se ignora (histéresis, sin
  parpadeo del eje).

La zona horaria no interviene: el epoch es UTC y la local solo se aplica al
formatear las etiquetas (como ya hace `xLabelText`).

### Persistencia: formato de fichero `/trend.bin`

Little-endian explícito (se serializa byte a byte, sin `struct` empaquetado,
para que host y ESP32 coincidan sin suposiciones):

```
off  tam  campo
  0    4  magic "ITRN" (49 54 52 4E)
  4    2  version = 1
  6    2  headerSize = 32
  8    4  slotSeconds = 10
 12    4  count          (ranuras guardadas, 1..8640)
 16    4  lastEpoch      (UTC s del inicio de la ultima ranura = lastSlot * 10)
 20    2  series = 6
 22    2  eventCount
 24    4  payloadCrc32   (CRC-32/IEEE de todo lo que sigue a la cabecera)
 28    4  headerCrc32    (CRC-32/IEEE de los bytes 0..27)
 32  6*count*2  series planar int16 LE, de la mas antigua a la mas reciente
  .  eventCount*8  { u32 epoch, u8 id, u8 prioridad, u8 alta(1)/baja(0), u8 0 }
```

Tamaño máximo 32 + 103 680 + 512 = 104 224 B. Una versión o `slotSeconds`
desconocidos, una longitud que no cuadra o un CRC malo hacen que el fichero
se **ignore entero** (nunca se restaura a medias): la tendencia arranca
vacía y el equipo sigue. El fichero guarda siempre epoch; no se vuelca nada
mientras el reloj no esté sincronizado.

### Política de volcado

- **Cuándo**: cada 5 min (`TREND_DUMP_PERIOD_MS`) si ha entrado alguna
  muestra desde el último volcado, y al **entrar en bloqueo** (aviso desde
  `enter_lock_screen` y la ruta de auto-bloqueo), con una separación mínima
  de 2 min respecto al volcado anterior: el auto-bloqueo se dispara tras cada
  interacción (20 s), y sin tope cada toque del operador sería un volcado.
- **Cómo**: en `UI_Task`, **fuera de `LVGL_Lock`**, en tres pasos:
  1. se serializa una instantánea consistente al buffer de staging (PSRAM;
     un `memcpy`-like de ~100 KB, orden de milisegundos);
  2. se abre `/trend.bin.tmp` y se escribe **un trozo de 4 KB como máximo
     por pasada del bucle y no más de uno cada 50 ms** (un bloque de
     LittleFS = 4 KB: cada trozo cuesta un borrado de ~45 ms con la caché
     de flash parada, el mismo orden que una escritura NVS; troceado, el
     refresco respira entre trozos y **nunca hay un bloqueo de 100 KB**,
     tampoco con una alarma activa);
  3. al terminar, `rename(tmp, /trend.bin)`, atómico en LittleFS.
  Un corte de corriente en mitad deja el `trend.bin` anterior intacto y un
  `.tmp` huérfano que el siguiente volcado reescribe.
- **Pérdida máxima ante un corte**: 5 min de datos y los eventos de esos 5
  min. Es el trade-off elegido frente al desgaste: 288 volcados/día x 27
  bloques ≈ 7,8 k borrados/día repartidos por LittleFS sobre ~1450 bloques
  ≈ 5 ciclos/bloque/día frente a ~100 k ciclos de vida.
- **Fallo**: si falla abrir, escribir o renombrar, se aborta, se libera el
  staging, se registra y se reintenta en el siguiente periodo (nunca en
  bucle). La tendencia en RAM no se ve afectada.
- En modo formación (`Training_IsActive()`) se sigue volcando: la
  telemetría que se registra es la real (la restricción de ADR-0002 es sobre
  lo que *cambia el usuario*, no sobre medidas).

### Política de restauración

Una sola vez por arranque, **en cuanto el reloj se sincroniza** (puede ser
varios segundos o minutos después del arranque: la hora la fija la
motherBoard por `CTRL,TIME`, o el RTC del HMI vía semilla):

1. se lee `/trend.bin` al staging, se valida (magic, versión, longitudes, dos
   CRC);
2. se rechaza si `lastEpoch/10 > ahora + 1` (fichero "del futuro": la hora
   estuvo mal); el fichero se deja en disco, el siguiente volcado lo
   sustituye;
3. las muestras del fichero se **anteponen** a lo ya registrado en esta
   sesión (`trend_store_prepend`): el hueco entre la última muestra del
   fichero y la primera viva se rellena con "sin dato", las muestras vivas
   mandan sobre las del fichero si se solaparan, y lo que no cabe en 24 h se
   descarta por el lado antiguo;
4. los eventos del fichero con ranura dentro del rango se anteponen a los
   vivos, sin duplicar.

Si no hay hora nunca (placa sin sincronizar), no se restaura ni se vuelca:
la tendencia se comporta como hoy, solo en RAM.

### Ventanas y decimación

Ventanas `{4 h, 12 h, 24 h}` = `{1440, 4320, 8640}` ranuras, por defecto 4 h.
LVGL no puede pintar 8640 puntos útiles en ~670 px; el gráfico tiene **siempre
la misma geometría de 1442 puntos** (721 cubos x 2: mínimo y máximo de
cada cubo, en el orden en que ocurrieron para conservar la forma de la
traza), con 2/6/12 ranuras por cubo. Así:

- cambiar de ventana no reasigna las series (`lv_chart_set_point_count` solo
  se llama una vez);
- los picos de una ventana de 24 h se ven (no se promedia);
- los **límites de cubo se alinean a múltiplos absolutos de ranura**, no a
  "ahora": si no, cada ranura nueva desplazaría los límites y el min/max de
  cada cubo parpadearía al repintar. El precio es un cubo parcial a la
  derecha (de ahí 721 y no 720), <= 12 ranuras de desfase del borde derecho
  (2 min en 24 h, ~1 px).
- Un cubo entero "sin dato" corta la línea (`LV_CHART_POINT_NONE`); huecos
  parciales dentro de un cubo no la cortan (con 12 ranuras por cubo, 2 min).

Rejilla: 4 h cada 15 min (17 líneas, mayor cada hora); 12 h cada hora (13
líneas, mayor cada 3 h); 24 h cada 2 h (13 líneas, mayor cada 6 h); siempre 5
etiquetas de hora en el eje del gráfico de humedad.

### Marcadores

- **Consigna**: una segunda serie por gráfico (naranja), dibujada *debajo*
  de la medida. Se registra solo si esa magnitud está bajo control en el eco
  de la placa (`ctrl_state_msg`: `actuation` y `controlMode`): consigna de
  aire con control de temperatura en modo aire; piel en modo piel; humedad
  con control de humedad. En cualquier otro caso, "sin dato" (no se pinta
  una línea de una consigna que no gobierna nada). La decisión vive en una
  función pura (`trend_setpoint_validity`) con test.
- **Alarmas**: `processReceivedAlarm()` (CommTask) detecta el flanco
  (`alarm.state != wasActive`) y encola `{id, prioridad, alta/baja,
  esp_timer}` en una cola de 16 entradas protegida por sección crítica de
  unos pocos ciclos (el hilo de `CommTask` no toca el modelo). `UI_Task` la
  drena y convierte cada flanco a ranura con **el instante del flanco**, no
  el de drenaje. Anillo de 64 eventos; al llenarse se descarta el más
  antiguo.
- **Dibujo**: en `LV_EVENT_DRAW_POST_END` de cada gráfico (después de series y
  rejilla), una línea vertical por evento visible en la ventana: **alta** =
  línea continua del color de la prioridad (alta rojo, media naranja, baja
  amarillo oscuro); **baja** (resuelta) = línea discontinua verde. Una
  leyenda de tres swatches en la cabecera de la vista (ASCII, ES/EN/FR/PT).
- Una alarma que ya estaba activa cuando el HMI arranca se sella en el
  instante en que el HMI la conoce (la placa reenvía `CTRL,ALM`), no en el
  que empezó realmente: limitación conocida y documentada.

### Módulos

```
src/modules/trend/
  trend_types.h      constantes (ranura, capacidad, series, sin dato)
  trend_store.*      anillo de 6 series int16: record (rellena huecos), get,
                     shift, prepend
  trend_events.*     anillo de eventos: add, ventana, shift, prepend
  trend_clock.*      mono -> ranura con offset epoch e histéresis
  trend_decimate.*   cubos min/max alineados + plan de ventana + slot->px
  trend_file.*       CRC-32, serializar, validar, restaurar
  trend_model.*      store + events + clock; muestra, flanco, restauración
  trend_policy.*     cuándo toca volcar; validez de consigna
  trend_persist.*    pegamento con E/S real (hmi_fs + PSRAM): restauración y
                     volcado troceado (solo firmware, no se compila en host)
src/modules/storage/
  hmi_fs.*           LittleFS de la partición "spiffs" (solo Arduino)
```

Todo `trend_*` es C++17 sin LVGL ni Arduino y se compila en `[env:native]`.
`hmi_fs` y la parte de `TelemetryHistory` que toca LVGL/LittleFS solo se
verifican compilando y en banco (no se finge cobertura de test).

### API de `hmi_fs` (la reutilizará el diario de eventos)

```cpp
bool   HmiFs_Begin(void);       // idempotente; monta por etiqueta "spiffs"; si
                                // el montaje falla, formatea UNA vez y reintenta
bool   HmiFs_IsMounted(void);
bool   HmiFs_Info(size_t *total, size_t *used);
// Lectura entera a un buffer del llamador (false si no existe / no cabe).
bool   HmiFs_ReadAll(const char *path, uint8_t *buf, size_t cap, size_t *outLen);
// Escritura atomica incremental: tmp + rename.
class HmiFsAtomicWriter {
  bool   begin(const char *path);              // abre path+".tmp"
  size_t write(const uint8_t *p, size_t n);    // 0 = fallo
  bool   commit(void);                         // cierra y rename(tmp, path)
  void   abort(void);                          // cierra y borra el tmp
};
bool   HmiFs_WriteAtomic(const char *path, const uint8_t *buf, size_t len);
```

Se monta **en `UI_Task`, fuera de `LVGL_Lock` y después de que el panel RGB
haya reservado sus bounce buffers** (la primera vez que el servicio de
volcado la necesita), no en `setup()`: el montaje asigna heap interno y no
debe competir con esa reserva. El tamaño de heap interno consumido por el
montaje se mide en banco y se anota.

### Tests de host

`[env:native]` compilará los `trend_*` (no `hmi_fs` ni `TelemetryHistory`):
anillo (huecos, duplicados, wrap, shift, prepend, límites), eventos (wrap,
ventana, prepend), reloj (primera sincronía, histéresis, salto atrás),
decimación (cubos alineados, orden min/max, vacíos, 24 h), formato (ida y
vuelta, cada corrupción detectada, fichero del futuro, fichero antiguo,
solape con datos vivos), y políticas.

## Risks / Trade-offs

- **Parpadeo benigno del panel durante un borrado de flash** (documentado en
  `platformio.ini`: la caché de PSRAM se suspende durante la operación de
  flash). Mitigado troceando (un borrado cada >= 50 ms, nunca un bloqueo
  largo), pero no eliminado. Es lo mismo que ya ocurre con las escrituras
  NVS. Se anota para la verificación en banco.
- **Heap interno del montaje de LittleFS**: pequeño pero no nulo; se monta
  tarde y se mide.
- **El fichero "futuro" o con hora errónea**: se descarta entero (no se
  intenta reparar).
- **Pérdida ante un corte**: <= 5 min más el hueco; es el precio acordado.
- **Una alarma a mitad de volcado**: el volcado continúa en trozos; nada
  bloquea el refresco ni el banner.
