## Context

El controlador de retroiluminación del CrowPanel 7" es un **STC8H1K28** en
I2C `0x30`, el mismo bus (IO15/IO16) que el táctil GT911 y el PCF8563, y el
mismo chip que da el zumbador y el altavoz del display. Protocolo: **un byte
por transacción, sin registro**. Según el comentario "doc v1.3" de
`UITask.cpp`:

| Byte | Significado |
|---|---|
| 0 | brillo máximo |
| 1..245 | niveles (245 = apagado) |
| 246 / 247 | zumbador ON / OFF |
| 248 / 249 | altavoz ON / OFF |

El firmware solo ha escrito 10 (`DISPLAY_BL_ON_VALUE`, el "encendido" de
producción) y 245 (`DISPLAY_BL_OFF_VALUE`). **Nunca se ha probado un valor
intermedio**; esa es la incertidumbre central y se trata como riesgo, no como
hecho.

Restricciones de la placa:

- **ARQ-LOCK-001**: ninguna transacción I2C dentro de `LVGL_Lock`. El sitio
  natural es tras `LVGL_Unlock()` en `UI_Task`, junto a `link_audio_service()`
  y `AlarmSound_Update()`.
- Las escrituras NVS van fuera del bloqueo, en el bloque diferido.
- La inactividad la mide LVGL (`lv_disp_get_inactive_time()`): cualquier toque
  la pone a 0, esté la pantalla bloqueada o no.

## Goals / Non-Goals

**Goals**

- Nivel de usuario 20-100 % aplicado en las pantallas normales.
- Modo noche: atenuar a un nivel fijo tras N s **bloqueada y sin toques**.
- Toque (bloqueo o desbloqueo) y alarma devuelven el brillo.
- Lógica de decisión pura y probada en host.
- Degradar a on/off sin romper nada si el STC8 no acepta niveles.

**Non-Goals**

- Sensor de luz ambiente / brillo automático (no hay sensor en el HMI).
- Horario de noche por reloj. El modo noche se activa por inactividad en
  bloqueo, no por hora.
- Cambiar nada del protocolo con la motherBoard.

## Decisions

### D1. Máquina de estados pura: `brightness_policy`

`modules/brightness/brightness_policy.{h,cpp}`, sin Arduino ni LVGL (compila
en `[env:native]`). Una función total:

```
uint8_t Brightness_Target(const BrightnessInputs&)
  BrightnessInputs { userPct, nightMode, locked, idleSec, alarmActive }
```

Tabla de estados a nivel (la primera fila que aplique gana):

| # | alarmActive | nightMode | locked | idleSec | Nivel objetivo |
|---|---|---|---|---|---|
| 1 | si | cualquiera | cualquiera | cualquiera | **100 %** |
| 2 | no | si | si | >= `BRIGHTNESS_NIGHT_DELAY_S` (30) | `min(userPct, BRIGHTNESS_NIGHT_PCT)` (20 %) |
| 3 | no | cualquier otra combinación (modo noche apagado, o no bloqueada, o con toque reciente) | | | `userPct` |

`userPct` se sanea siempre a `[20, 100]` (un valor de NVS corrupto o 0 nunca
da una pantalla negra). Notas:

- La alarma pesa más que el modo noche y que el nivel de usuario: **nunca**
  hay pantalla atenuada con alarma activa.
- "Alarma" = alguna alarma de `alarmList` activa (silenciada o no: el
  silencio apaga el sonido, no la condición) **o** pérdida del enlace con la
  motherBoard (`Display_IsBoardLinkLost()`, que ya hace sonar su propia señal).
- `idleSec` es la inactividad de LVGL en segundos: un toque la pone a 0, así
  que un toque en bloqueo o desbloqueo (que lo hay, en `LockScreenAnyTouch_cb`,
  en el popup de desbloqueo, en cualquier pantalla) sube a `userPct` en la
  siguiente pasada (<= `LOOP_DELAY_MS`). No hace falta enganchar callbacks de
  toque: la inactividad de LVGL es la fuente única.
- Autobloqueo a los 20 s (`INACTIVITY_TIMEOUT_MS`) y atenuación a los 30 s sin
  toques: el modo noche atenúa unos 10 s después del bloqueo automático. En un
  bloqueo manual, `enter_lock_screen()` llama a `lv_disp_trig_activity()`, y la
  atenuación llega 30 s después del bloqueo.
- Con `userPct == 20` el nivel noche (20 %) no atenúa nada más: el suelo del
  slider coincide con el nivel noche. Es deliberado y se anota como deuda: el
  nivel noche real (si hace falta más oscuro) es una decisión de banco.
- El apagado `pwroff` no pasa por la política: sigue escribiendo
  `Backlight_Off()` y deteniéndose.

### D2. Mapa porcentaje a byte

Función pura `Brightness_PercentToByte(pct, levelsSupported)` en el mismo
módulo (testeable). El byte es **inverso** (0 = máx). Se modela el nivel como
ciclo de trabajo con una curva cuadrática (la percepción de brillo es
aproximadamente logarítmica; una curva lineal dejaría todo el recorrido útil
en la mitad alta):

```
duty   = pct * pct                      (0..10000)
byte   = 245 - (duty * (245 - 10) + 5000) / 10000
byte   = clamp(byte, 10, 240)           (nunca >= 241 salvo Backlight_Off)
```

| pct | byte |
|---|---|
| 100 | 10 (idéntico a hoy: sin cambio de comportamiento por defecto) |
| 80 | 95 |
| 60 | 160 |
| 40 | 207 |
| 20 | 236 |

Esos valores son una **hipótesis a calibrar en banco**. El techo 240 deja un
margen de 5 con el 245 (apagado) y 6 con el primer opcode de audio (246): una
conversión nunca puede apagar la pantalla ni disparar el zumbador, ni siquiera
con un `pct` fuera de rango (se sanea antes).

### D3. Degradación: `DISPLAY_BL_LEVELS_SUPPORTED`

Constante de compilación en `display_config.h` (por defecto 1). Con 0,
`Brightness_PercentToByte()` devuelve siempre `DISPLAY_BL_ON_VALUE` para
cualquier `pct > 0`: el slider y el modo noche siguen funcionando en la UI y
en NVS, pero el hardware queda a pleno brillo, que es el comportamiento
actual. Es el interruptor de banco: si el STC8 no acepta niveles (se apaga, se
cuelga, ignora el byte), se pone a 0 y el firmware sigue siendo seguro. No hay
detección en caliente: la escritura I2C se ACKea igual aunque el chip
descarte el byte, así que no se puede saber por software.

Defensa adicional: el servicio nunca "se acuerda" de un nivel que no pudo
escribir (D4), y la fila 1 de la tabla escribe siempre el byte 10, el único
probado en producción. Una alarma siempre restituye un estado conocido.

### D4. Driver `drivers/backlight` y servicio en `UI_Task`

- `Backlight_SetPercent(pct, attempts)`: sanea, convierte, escribe, reintenta
  `attempts` veces con 50 ms entre intentos (solo el arranque pide 5; el
  servicio periódico pide 1 para no bloquear la UI). Devuelve el resultado de
  `endTransmission() == 0`. Recuerda el último byte escrito con éxito.
- `Backlight_Off()`: 245, hasta 3 intentos.
- El servicio (`brightness_service`) vive en `UITask.cpp`: dentro del bloqueo
  de LVGL solo **calcula** el objetivo (lee pantalla activa, inactividad,
  alarmas); tras `LVGL_Unlock()` compara con `Backlight_LastByte()`/el
  porcentaje aplicado y escribe solo si difiere. Si la escritura falla, el
  porcentaje aplicado no se actualiza y se reintenta en la siguiente pasada.
- Las escrituras `Wire` en línea del arranque y de `pwroff_update()` pasan a
  llamar al driver. La prueba I2C de fábrica (`FactoryTest.cpp`) y el
  zumbador (`buzzer.cpp`, `AudioManager.cpp`) **no se tocan**: usan el mismo
  bus pero no son brillo.
- Pantallas que no deben atenuarse: no hay excepciones por pantalla. Si el
  usuario bloquea con modo noche, el test de fábrica, la ayuda, etc. no están
  bloqueados (`locked` es falso fuera de la pantalla de bloqueo), por lo que
  nunca se atenúan.

`locked` para la política = `lv_scr_act() == ui_ScreenLock`, no la variable
`locked` (que se pone a false al tocar para desbloquear mientras la pantalla de
bloqueo sigue cargada): la política necesita "estoy mostrando la pantalla
de bloqueo", y el toque ya lo cubre la inactividad.

### D5. Ajustes y persistencia

Dos filas nuevas en `ui_ModesConfigCont` (las filas existentes se reparten
cada 55 px para que quepan cinco):

- slider "BRILLO" 20-100, paso 1. Al **soltar** (`LV_EVENT_RELEASED`) se
  persiste; mientras se arrastra solo se actualiza la variable (el servicio
  aplica el nivel en vivo, así el usuario ve el efecto sin escribir NVS en
  cada paso).
- switch "MODO NOCHE", por defecto OFF.

NVS `hmi_cfg`: `bl_pct` (uchar, 20-100, defecto 100) y `bl_night` (uchar 0/1,
defecto 0). Se escriben en el bloque diferido de `UI_Task` (fuera de
`LVGL_Lock`, patrón de `doNVSWrite`), no inline como hace hoy el handler de
modo oscuro. En modo formación (`Training_IsActive()`) no se persiste, igual
que el resto de ajustes (ADR-0002).

## Risks / Trade-offs

- **Niveles intermedios sin verificar (alto)**: ver D3. Mitigación: el
  interruptor de compilación, el byte 10 en toda alarma y el techo de 240.
- **Parpadeo de I2C al atenuar/restaurar**: dos transacciones de 1 byte por
  cambio; despreciable frente al tráfico de táctil.
- **Compartir bus con zumbador**: las escrituras de brillo y de zumbador son
  transacciones independientes de un byte sin estado compartido en el chip;
  ya conviven hoy.
- **Lectura de `alarmList` desde la tarea UI mientras la escribe Comm**: es el
  patrón ya existente de `UI_IsAnyAlarmActive()`; el peor caso es leer un
  estado con un ciclo de retraso, y la política se reevalúa cada pasada.

## Migration Plan

Sin migración: las claves NVS nuevas no existen en unidades ya desplegadas y
se leen con su valor por defecto (100 %, noche OFF), lo que reproduce el
comportamiento actual byte a byte (10). Rollback: reflashear la versión
anterior ignora las claves.

## Open Questions

- ¿Acepta el STC8H1K28 niveles intermedios y con qué curva? (banco)
- ¿El 20 % (byte 236) es suficientemente visible, y el nivel noche, suficientemente
  oscuro? (banco)
