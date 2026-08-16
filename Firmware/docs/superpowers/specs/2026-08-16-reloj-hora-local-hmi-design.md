# Reloj de hora local en la cabecera del HMI

Fecha: 2026-08-16
Estado: propuesto
Placas afectadas: `motherBoard`, `Display_HMI`, `PROTOCOL.md`

## Problema

El HMI no muestra fecha ni hora. La petición original era acotada — poner el
reloj a la derecha de "IncuNest" en la cabecera — pero al investigarla apareció
que **el sistema entero no conoce su hora local**:

- El NTP por WiFi se configura con offset cero (`Wifi_OTA.cpp:1090`,
  `DriveUpload.cpp:261`), así que el reloj de sistema de la motherBoard es UTC.
- El HMI formatea todos sus epochs con `gmtime_r` (`AlarmCenter.cpp:103`,
  `BabyHistory.cpp:70`), o sea también UTC.
- La única fuente de zona horaria que el equipo recibe hoy —el offset NITZ que
  el operador móvil entrega junto a la hora— se usa para normalizar a UTC y
  acto seguido se descarta (`GPRS.cpp:354-358`).

La hora local no es deducible por el equipo: es una convención política, no una
magnitud observable. NTP no transporta zona horaria, y la longitud daría hora
solar, no civil. Alguien externo tiene que comunicarla.

Además hay un fallo latente en la adquisición: `GPRSEnsureTimeSynced()`
(`GPRS.cpp:331`) sale por la puerta de atrás en cuanto el reloj ya está en hora:

```c
if (time(nullptr) >= (time_t)1609459200L) {
  s_synced = true;   // latch permanente
  return;            // nunca se llega a modem.getNetworkTime()
}
```

En cualquier unidad donde el NTP por WiFi gane la carrera, al módem no se le
llega a preguntar por la zona, y `s_synced` impide una segunda oportunidad. La
causa raíz es que dos responsabilidades distintas viven en la misma función: el
reloj lo puede poner el WiFi, pero **la zona solo la sabe el módem**.

## Alcance

Dentro:

- La motherBoard adquiere y difunde el offset de zona horaria.
- El HMI muestra un reloj de fecha y hora local en la cabecera de `ScreenMain`.
- Los tres puntos del HMI que pintan fechas pasan a hora local por un helper
  común.

Fuera (decisiones explícitas, no olvidos):

- **Sin ajuste manual de zona.** Las dos vías son automáticas. Una unidad sin
  SIM, sin cobertura y sin WiFi se queda sin hora local, y es aceptable.
- **Sin reglas de horario de verano (DST).** Se maneja un offset fijo. Togo, el
  destino de despliegue, no aplica DST. Una base de zonas IANA en el firmware es
  desproporcionada para una etiqueta de cabecera.
- **Sin persistencia del offset.** Vive solo en RAM; un reinicio sin
  conectividad vuelve a "Sin hora" hasta que alguna vía conteste.

## Principio rector

> Todo lo que se almacena o se transmite es UTC. El offset se aplica
> únicamente al formatear para una persona.

Historial de alarmas, perfiles de bebé, subidas a Drive y telemetría a
ThingsBoard no cambian. Un cambio de offset no reinterpreta ningún registro ya
escrito — propiedad necesaria para la trazabilidad exigida por IEC 60601.

## Arquitectura

### `motherBoard/src/modules/util/tz_source.{h,cpp}` (nuevo)

Lógica pura, sin Arduino ni red, para que entre en el entorno `native` y sea
testeable con Unity — igual que su vecino `civil_time.cpp`, ya presente en
`build_src_filter` (`platformio.ini:132`).

Responsabilidades:

- Guardar el offset vigente en cuartos de hora y su procedencia.
- Aplicar la política de prioridad entre fuentes.
- Validar rangos (offsets civiles reales: `[-48, +56]` cuartos de hora).
- Parsear el offset de la respuesta del servicio de IP.

Estado expuesto:

| Campo | Tipo | Significado |
|---|---|---|
| `quarterHours` | `int8_t` | Offset respecto a UTC, en cuartos de hora (`+8` == UTC+2) |
| `source` | enum | `NONE` (0), `NITZ` (1), `IP` (2) |

La unidad es cuartos de hora a propósito: es la que ya usa
`civil_to_unix_utc(..., int tzQuarterHours, ...)` (`civil_time.h:14`), y
cubre husos no enteros como UTC+5:45. No se introduce una segunda convención.

### Política de prioridad

**NITZ gana a IP, siempre.** La antena está físicamente donde está el equipo;
una dirección IP puede ser de una VPN, un enlace satelital o la sede del
operador en otro país. Si NITZ llega después de que la IP ya hubiera fijado un
offset, NITZ lo sustituye. El caso inverso no ocurre: una respuesta por IP nunca
degrada un offset que ya vino de NITZ.

Con `source == NONE` no hay offset conocido y el HMI no debe pintar hora.

### Adquisición por GPRS

Se extrae la lectura de zona de `GPRSEnsureTimeSynced()` a una función hermana:

```c
void GPRSEnsureTimeSynced();      // pone el reloj; conserva su latch actual
void GPRSEnsureTimeZoneSynced();  // lee el tz del módem; latch independiente
```

`GPRSEnsureTimeZoneSynced()` se ejecuta **aunque el reloj ya esté sincronizado**,
que es exactamente el caso que hoy se pierde. Reutiliza la llamada
`modem.getNetworkTime(..., &tz)` que ya existe, con el mismo criterio de rechazo
del reloj por defecto de 2004 del SIM800.

La lógica de sincronización de reloj no se modifica: es zona sensible y con
historia. El cambio es de estructura, no de comportamiento del reloj.

### Adquisición por WiFi

Consulta al servicio de geolocalización por IP `ip-api.com`, que devuelve el
offset sin requerir clave de API y con una respuesta mínima. **El endpoint y el
formato exactos se verifican contra la documentación del servicio en el momento
de implementar**, no se dan por conocidos de memoria.

Comportamiento:

- Se intenta una vez tras conectar el WiFi, con reintentos espaciados si falla.
- No bloquea el arranque ni ninguna tarea de control.
- Si el parseo falla o el offset está fuera de rango, se descarta en silencio y
  se reintenta más tarde. El estado previo no se degrada.

La motherBoard ya dispone de los dos patrones de cliente necesarios
(`WiFiClient` en `Wifi_OTA.cpp:46`, `WiFiClientSecure` en `DriveUpload.cpp:117`);
no se añade ninguna librería.

### Protocolo

Se extiende la trama existente en lugar de añadir una nueva, porque
`known_issues.md` #2 desaconseja tráfico UART periódico evitable:

```
CTRL,TIME,epoch,tzq,tzsrc
```

| Campo | Rango | Significado |
|---|---|---|
| `epoch` | `uint32` | Hora Unix **UTC**, o `0` si no hay hora. Sin cambios. |
| `tzq` | `-48..+56` | Offset en cuartos de hora. |
| `tzsrc` | `0..2` | `0`=desconocido, `1`=NITZ, `2`=IP. |

`tzsrc` no es redundante: sin él, **"offset 0 porque estamos en Togo (UTC+0)"** y
**"offset 0 porque no sabemos la zona"** son indistinguibles, y el HMI no puede
decidir si pintar la hora o el aviso.

Compatibilidad hacia atrás, en ambos sentidos:

- HMI antiguo + MB nueva: `sscanf(line, "CTRL,TIME,%lu", &epoch)`
  (`CommTask.cpp:520`) ignora los campos sobrantes. Sigue funcionando en UTC.
- HMI nuevo + MB antigua: los campos no llegan; el HMI asume `tzsrc=0`.

### Lado HMI

Se amplía la API de reloj que ya existe en `CommTask.h:186-191`:

```c
uint32_t HMI_GetEpochNow();          // UTC, sin cambios
int8_t   HMI_GetTzQuarterHours();    // offset vigente
bool     HMI_HasLocalTime();         // false si tzsrc==0 o epoch==0
```

Un único helper de formateo aplica el offset, y **los tres puntos que pintan
fechas pasan por él**:

1. El reloj de cabecera (nuevo).
2. `AlarmCenter.cpp:103`.
3. `BabyHistory.cpp:70`.

Es deliberado incluir los dos existentes: una cabecera marcando las 14:32 junto
a un historial que fecha esa misma alarma a las 12:32 es peor que no tener
reloj, y en registros clínicos es una ambigüedad inaceptable.

### El reloj en pantalla

Dos labels en la cabecera de `ui_ScreenMain`, en el hueco libre entre el final
de "IncuNest" (x≈140) y el inicio del botón "Bebes" (x≈500):

- Hora en cuerpo grande.
- Fecha debajo en cuerpo pequeño, formato `DD/MM/AAAA`.

Son dos labels porque LVGL no admite dos tamaños de fuente en uno solo.

Un `lv_timer` de 1 s recalcula la cadena y **solo escribe el label si cambió**,
para no forzar repintados cada segundo.

Sin hora disponible (`HMI_HasLocalTime() == false`): se muestra `"Sin hora"`.

## Seguridad y modos de fallo

- Nada de esto alimenta el PID, las alarmas ni ningún actuador. Es presentación.
  Un fallo en cualquier vía degrada a "Sin hora", nunca a un valor inventado.
- El JSON del servicio y la trama de protocolo se parsean a la defensiva:
  validar rango antes de aceptar, descarte silencioso de lo malformado, según
  `.claude/rules/security.md`.
- **Riesgo aceptado**: el nivel gratuito de `ip-api.com` es HTTP en claro, así
  que alguien en la misma red podría alterar el offset *mostrado*. No afecta a
  registros almacenados (siguen en UTC), ni a la telemetría, ni al control. Se
  documenta como riesgo asumido, no como descuido. Si en el futuro se exige
  integridad aquí, la vía es NITZ como fuente única, no un parche sobre HTTP.
- El offset nunca modifica epochs ya almacenados.

## Verificación

**Automatizable — Unity en `[env:native]` de motherBoard** (TDD estricto, test
en rojo primero), sobre `modules/util/tz_source.cpp`:

- El parseo del offset de una respuesta válida del servicio de IP.
- Rechazo de respuestas malformadas, truncadas o con offset fuera de rango.
- NITZ sustituye a un offset previo de IP.
- Una respuesta por IP no degrada un offset ya obtenido de NITZ.
- El estado inicial es `NONE` y no aparenta ser UTC+0.

**Manual — documentar qué se probó**, según `.claude/rules/testing.md`:

- `pio run -e main` en ambas placas.
- Unidad con SIM: el offset llega por NITZ y el reloj marca hora local.
- Unidad solo WiFi: el offset llega por la consulta de IP.
- Unidad sin conectividad: la cabecera muestra "Sin hora" y nada se cuelga.
- Coherencia: la hora de la cabecera y la del historial de alarmas coinciden.

## Consecuencias

- `PROTOCOL.md` debe documentar los dos campos nuevos de `CTRL,TIME`.
- El HMI mostrará hora local mientras ThingsBoard y Drive siguen en UTC. Es
  intencionado, y es el comportamiento habitual en equipo médico, pero conviene
  que quien lea ambos lo sepa.
- Sin persistencia, un reinicio sin conectividad pierde el offset aprendido.
  Añadir caché en NVS es un cambio pequeño y aislado si algún día molesta.
