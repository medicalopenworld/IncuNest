## Context

`SbSnapshot` (`sensorboard_comm.h`) ya guarda, con su marca de frescura, el enlace (`link_ok`), la puerta (`door_known/door_open/door_faulty`), la luz (`lux_valid/lux`) y el sonido (`dba_valid/dba`). `sb_build_telemetry()` los traduce a claves `sb_*` de ThingsBoard y **omite** la clave de lo que no se sabe. El HMI no recibe nada de eso.

El enlace MB→HMI es CSV ASCII por UART a 115200. `CTRL,STATE` y `CTRL,TEL` salen a 1 Hz; `CTRL,PPG` a 25 Hz; `CTRL,TIME` cada 10 s (precedente directo de un mensaje lento y propio). El parser del HMI (`parse_message`) es una cadena de `else if (strncmp(...))` sin rama final: una línea desconocida no hace nada.

## Goals / Non-Goals

**Goals**
- Que el operador vea de un vistazo "puerta abierta" y un nivel cualitativo de ruido, en Main y en bloqueo.
- Lógica pura (formato, pacing, parser, clasificación, decisión de qué pintar) probada en host.
- No tocar el lazo de control ni las alarmas.

**Non-Goals**
- Mostrar dB o lux como medida. No hay calibración.
- Señalizar el fallo del hall: lo hace `ALARM_SENSORBOARD_DOOR_FAULT`.
- Cambiar la cadencia o el contenido de `CTRL,STATE`/`CTRL,TEL`.
- Calibrar el micrófono o el ALS (deuda de `SensorBoard_v2`).

## Decisions

### D1. Mensaje propio `CTRL,ENV`, no campos nuevos en `CTRL,STATE`

`CTRL,STATE` formatea en `msg[192]` y ya lleva 23 campos; otra rama reserva el 24. Además `PROTOCOL.md` fija que **todo campo de STATE debe poder asumir 1 Hz**, y el entorno no lo necesita: la puerta cambia a ritmo humano y el ruido se publica en origen cada 5 s. Un mensaje aparte permite una cadencia propia y no compite por bytes.

Formato: `CTRL,ENV,link,door,lux,dbA\n`

| campo | valores | notas |
|---|---|---|
| `link` | `1` / `0` | enlace USB con la SensorBoard vivo (heartbeat < 90 s) |
| `door` | `0` cerrada, `1` abierta, `2` fallo, `-1` desconocida | `2` = hall sospechoso (`door_faulty`), la misma condición que `ALARM_SENSORBOARD_DOOR_FAULT` |
| `lux` | entero ≥ 0, o `-1` | **sin calibrar**, reservado |
| `dbA` | entero ≥ 0, o `-1` | **sin calibrar**, ver D3 |

Los enteros van redondeados: la UART no necesita decimales de una magnitud que se va a clasificar en tres cubos.

### D2. Centinelas: `-1`, el criterio de `CTRL,TEL`

`CTRL,TEL` manda un centinela fuera de rango físico en lugar de `0`, porque `0` es un valor plausible que se lee como medida. Aquí lo mismo: `-1` no es ni un lux ni un dBA posibles, y para `door` `-1` es el único valor que no es ni cerrada ni abierta. Con `link=0` los tres valores viajan como `-1` (no se manda la última lectura buena: sería un dato congelado, y es lo que `sb_build_telemetry()` ya evita omitiendo la clave).

La frescura reutiliza las constantes del módulo (`SB_ENV_STALE_MS`, `SB_SOUND_STALE_MS`, `SB_DOOR_STALE_MS`) para que ThingsBoard y el HMI coincidan sobre qué es "rancio".

Constantes en `shared/include/protocol.h` (`PROTO_ENV_UNAVAILABLE`, `PROTO_ENV_DOOR_*`, `PROTO_ENV_PERIOD_MS`): ambos lados las incluyen, para que el centinela no sea un número mágico duplicado.

### D3. El ruido se muestra como nivel cualitativo, con umbrales provisionales

Medido en banco: el ICS-41350 da **~60 dB en reposo**, "deberían ser 35-45", sin ponderación A y sin sonómetro de referencia (`SensorBoard_v2/ESTADO.md`). Es un desfase de ~+20 dB (no necesariamente constante: sin ponderación A cambia con el espectro). Pintar "60 dB" en una incubadora neonatal sería afirmar una medida que no se tiene, y quien lo lea la compara con el límite recomendado de 45 dBA para una UCIN.

Se clasifica sobre la escala **cruda** del sensor, y el HMI pinta solo la clase:

| clase | dBA crudos | texto (ES / EN / FR / PT) | razonamiento |
|---|---|---|---|
| Bajo | `< 65` | Bajo / Low / Bas / Baixo | el reposo medido (~60) más un margen de 5 dB |
| Medio | `65 .. 74` | Medio / Mid / Moyen / Medio | hasta 15 dB sobre el reposo |
| Alto | `>= 75` | Alto / High / Eleve / Alto | ≥ 15 dB sobre el reposo; con el desfase estimado equivale a ≳ 55 dBA reales, por encima del objetivo de 45 de la AAP |

Se evita "Normal" y "Silencio" a propósito: afirmarían que el ambiente es aceptable o que está en silencio, y sin calibración ninguna de las dos cosas se sabe. "Bajo/Medio/Alto" describe la posición relativa a una referencia de banco, no una conformidad.

**Los umbrales son provisionales y viven en un único sitio** (`SBENV_NOISE_MID_DBA` / `SBENV_NOISE_HIGH_DBA` en `sb_env.h`, con el test que los ancla). Cuando el micrófono se calibre contra sonómetro hay que **reemplazarlos** por umbrales reales (p. ej. 45 / 55 dBA) y revisar los textos; queda anotado como deuda.

Sin histéresis: la fuente publica cada 5 s y el cambio de clase en el borde es, como mucho, un parpadeo del texto cada 5 s. Se añadirá si en banco molesta.

### D4. La luz viaja pero no se pinta

El ALS-PT19 es un fototransistor por ADC cuya conversión a lux "depende de la resistencia de carga del esquema — calibración pendiente" (`hardware.md`). Además el HMI no tiene ninguna decisión que tomar con la luz. Pintarla sería exactamente el número sin significado que D3 evita. Se mantiene el campo en el mensaje (parseado y probado) para no tocar el protocolo cuando exista calibración.

### D5. La puerta: solo se pinta "abierta"

- `0` cerrada: no se pinta nada (estado normal; menos ruido en un heading ya lleno).
- `1` abierta: icono de puerta + texto "Abierta".
- `2` fallo: **no se pinta**. El estado abierta/cerrada de un hall sospechoso no es fiable, y la alarma `ALARM_SENSORBOARD_DOOR_FAULT` ya lo señaliza con su banner y su sonido; un segundo indicador sería duplicar la señal y, peor, pintar un "abierta" que el propio sistema desconfía.
- `-1` desconocida: no se pinta.

La puerta **no** entra en el lazo térmico ni en ninguna alarma nueva.

### D6. Cadencia: inmediato en cambio, 5 s en reposo

`sb_env_pacer` decide `due` si: no se ha enviado nunca, o cambió `link` o `door` respecto a lo último enviado, o pasaron `PROTO_ENV_PERIOD_MS` (5000 ms) desde el último envío. `lux`/`dbA` **no** disparan envío inmediato: son lentos y se clasifican en cubos. 5 s coincide con `SB_SOUND_PUBLISH_PERIOD_MS`: enviar más rápido no traería dato nuevo.

`Communication_Task` itera cada 1 ms (`COMMUNICATION_TASK_PERIOD_MS`), así que el snapshot (copia bajo mutex) se toma cada 200 ms, no en cada pasada: la latencia de la puerta queda ≤ 200 ms y no se pelea con la tarea del SensorBoard por el mutex.

### D7. Dónde se pinta

El heading de Main está lleno: los huecos entre widgets son de 19 px, y entre Alarmas y Ajustes quedan ~74 px que las zonas táctiles ampliadas (`TOUCH_EXT_MEDIUM`=30) se comen. Otras ramas abiertas meten identidad del paciente en el botón Bebés y un indicador de batería: esos slots están reservados.

El **botón del reloj** (`ui_ClockButton`, 180×55) tiene la hora en `montserrat_26` (~80 px) y la fecha `DD/MM/AAAA` en 14 (~90 px), ambas pegadas a la izquierda: queda libre una franja de ~70 px a la derecha, **dentro** de un widget que ya existe, sin mover ningún slot ni tocar zonas táctiles. Dos filas de 22 px: arriba la puerta (solo si abierta), abajo el ruido. Los hijos no son clicables, así que el toque sigue llegando al botón del reloj.

Caso límite: con "Sin hora" (`STR_NO_TIME`, 26 pt, ~140 px) el texto invade la franja; en ese estado el indicador se oculta en Main. Es un estado transitorio (la MB aún no ha sincronizado la hora) y el indicador es informativo.

En **bloqueo** el heading tiene el reloj como etiquetas sueltas y su lado derecho está libre (sin Bebés/Alarmas/Ajustes): se replica a la derecha del candado, en el mismo y de la fila.

El refresco sigue el patrón de `clock_update()`: se calcula una firma y solo se escribe LVGL si cambia.

### D8. Qué hace el HMI con el dato rancio

El HMI guarda el último `CTRL,ENV` con su `millis()`. `sbenv_view()` (pura) devuelve "no pintar" si nunca llegó, si pasaron más de `SBENV_STALE_MS` (15 s = 3 periodos) o si `Display_IsBoardLinkLost()`. Así, una MB que se reinicia o que pierde la SensorBoard no deja la puerta "abierta" congelada en pantalla.

## Risks / Trade-offs

- **UART**: 25 B/5 s; sin riesgo de la inundación de `known_issues` #2. La línea se prueba contra un buffer de 40 B.
- **Contrato sin test de integración**: el cableado de `CommTask.cpp` (toma de snapshot, `hmiSerial.print`) y todo LVGL no tienen entorno de test; la lógica está en módulos puros para que lo que se pueda probar se pruebe. El resto va a la checklist de banco.
- **Desfase del micrófono no constante**: si el ruido ambiente real de una sala cambia el desfase, los umbrales crudos pueden ser inadecuados. Mitigado: solo hay tres cubos y se rotula como relativo; se rehacen al calibrar.
- **Tema oscuro**: `UI_ApplyTheme()` barre todas las etiquetas y les pone un color; el indicador reasigna sus colores tras el barrido (invalidando su caché), igual que `wifi_link_status_invalidate()`.

## Migration Plan

Sin migración: protocolo aditivo. Orden de flasheo indiferente.

## Open Questions

- ¿Quiere producto que el ruido "Alto" dispare algún aviso audible? Fuera de alcance: hoy es informativo.
- Umbrales definitivos tras calibrar con sonómetro.
