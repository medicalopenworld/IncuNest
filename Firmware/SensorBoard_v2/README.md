# SensorBoard v2 — Firmware

Firmware de la placa de sensores periférica de la incubadora **IncuNest**. Publica telemetría ambiental (y en fases futuras audio, puerta y cámara) hacia la motherboard a través de USB-CDC con un protocolo enmarcado con CRC.

- **MCU:** ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB PSRAM octal)
- **Framework:** ESP-IDF v6 nativo (CMake + `idf.py`), C, FreeRTOS
- **Hardware/pinout:** [`docs/hardware.md`](docs/hardware.md) (fuente de verdad)
- **Roadmap de fases:** `../docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md`

## Protocolo (Fase 1)

Frame binario sobre USB CDC (ADR-0001):

```
Magic(0xAB 0xCD) + Type(1B) + Length(4B LE) + Payload(N) + CRC16(2B BE)
```

- CRC16-CCITT FALSE (poly `0x1021`, init `0xFFFF`) sobre Type+Length+Payload.
- `Type 0x00` = payload JSON (≤256 B). `Type 0x01` = reservado para JPEG (Fase 5).
- Todo `ESP_LOG` sale como frames `{"type":"log","ts":…,"msg":"…"}` — no hay consola de texto.
- Comandos entrantes: `{"type":"cmd","cmd":"status","id":N}` → resp con `device`/`fw`/`uptime`. Comando desconocido → `status:"error"`.
- Heartbeat `{"type":"event","cmd":"heartbeat","uptime":…}` cada 30 s. **Contrato de fail-safe:** si la motherboard deja de recibir heartbeat durante >90 s (3 periodos), debe tratar al SensorBoard como no disponible y sus lecturas como inválidas — un fallo de arranque aquí produce reboot (sin canal de diagnóstico) y la única señal externa es ese silencio. `uptime`/`ts` (uint32, ms) dan la vuelta a los ~49.7 días: no son monótonos indefinidamente.

La API para las fases de sensores es `sensorBoard_comm_send_json()` (y `send_binary()` a partir de la Fase 5); ninguna fase reabre el framing.

## Telemetría (Fase 2)

Cada `CONFIG_SB_ENV_POLL_PERIOD_S` (5 s por defecto) se publica:

```json
{"type":"event","cmd":"sensor_data","data":{"temp":[36.5,37.0,36.8],"hum":[55.0,54.5,60.1],"lux":320.5},"ts":5200}
```

- Posición i = sensor físico (0 = bus temp 0x44, 1 = bus temp 0x46, 2 = bus principal 0x44). Un sensor caído aparece como `null` en su posición — la **fusión/votación es responsabilidad de la motherboard** (ADR-0002).
- `lux` proviene del ALS-PT19 por ADC; la conversión usa `CONFIG_SB_ALS_UV_PER_LUX`, **sin calibrar** contra luxómetro todavía.
- La resp de `status` incluye ahora `"sensors":{"sht0":…,"sht1":…,"sht2":…,"als":…,"door":…}` con la disponibilidad real.

## Puerta (Fase 4)

Eventos en tiempo real por interrupción (hall DRV5032 en IO47, debounce `CONFIG_SB_DOOR_DEBOUNCE_MS`): `{"type":"event","cmd":"door_open","ts":…}` / `door_closed`, solo en cambios estables. Al arrancar se publica el estado actual una vez y, sin cambios, se **re-afirma cada `CONFIG_SB_DOOR_REASSERT_S` (30 s)** — un evento perdido por backpressure se autocorrige. Polaridad con `CONFIG_SB_DOOR_ACTIVE_LOW` (por defecto: imán presente = cerrada).

**Contrato de fail-safe (motherboard):** con una sola línea digital el firmware **no puede distinguir** "puerta abierta" de "hall desconectado/averiado" (pull-up ⇒ nivel alto en ambos). La motherboard debe tratar un `door_open` sostenido implausible o un flapping open/closed como posible fallo de sensor (alarma), nunca como entrada directa del control térmico.

## Nivel sonoro (Fase 3)

Cada `CONFIG_SB_MIC_PUBLISH_PERIOD_S` (5 s): `{"type":"event","cmd":"sound_level","data":{"dba":42.3},"ts":…}` — RMS de una ventana de `CONFIG_SB_MIC_WINDOW_MS` (1 s) del ICS-41350 (PDM, 16 kHz). **Advertencia:** el campo se llama `dba` por el protocolo del roadmap, pero el valor es SPL estimado **sin ponderación A** y con offset de sensibilidad de datasheet (`CONFIG_SB_MIC_DB_OFFSET_TENTHS`) **sin calibrar contra sonómetro**. No usar para decisiones clínicas hasta calibrar.

## Cámara (Fase 5)

`{"type":"cmd","cmd":"capture","id":N}` → resp `{"type":"resp","cmd":"capture","id":N,"status":"ok","size":<bytes>,"ts":…}` seguida de un frame binario `TYPE=0x01` con el JPEG (QVGA, `CONFIG_SB_CAM_JPEG_QUALITY`). El sensor se autodetecta por SCCB — **OV2640 (0x30) u OV5640 (0x3C)** — y el modelo detectado se loguea al arrancar. Solo bajo demanda — no hay captura continua. Un `capture` con otro en vuelo responde `error`/`busy`; una captura colgada >10 s responde `camera stalled` y baja `sensors.cam`. El SCCB de la OV2640 comparte el bus I2C principal con el SHT40 (requiere que `env_sensors` inicie primero); si ese bus no está disponible, `sensors.cam:false`.

**Prioridad de TX:** el JSON (telemetría/heartbeat/resp) siempre drena antes que los binarios; hay como máximo **un JPEG en vuelo** (PSRAM acotada) y, por el framing contiguo, un JSON urgente puede esperar como mucho la transmisión de ese único frame (~600 ms en el peor caso con host atascado, típicamente <100 ms).

**Modelo de amenaza del enlace (decisión registrada):** el USB SensorBoard↔motherboard se trata como **canal de confianza intra-dispositivo** (conector interno del equipo). No hay autenticación en el protocolo: cualquier host físico en ese USB puede pedir `capture` (imagen del interior de la incubadora). Riesgo aceptado mientras el conector no sea accesible externamente; re-evaluar si el enlace sale del chasis.

## Compilar y flashear

```bash
idf.py build                      # compila (gate de verificación automatizado)
idf.py -p COMx flash monitor      # flasheo y monitor — SIEMPRE manual
```

## Tests (Unity, on-target)

```bash
idf.py -C test_apps/comm_test build                       # compila los tests
idf.py -C test_apps/comm_test -p COMx flash monitor       # los ejecuta en hardware real
```

Los `TEST_CASE` derivan de los escenarios OpenSpec (`openspec/specs/`). La ejecución en placa es manual; el gate automatizado es solo compilación.

## Estructura

| Ruta | Qué es |
|---|---|
| `main/` | `app_main`: init + heartbeat |
| `components/usb_comm/` | Transporte: framing, CRC16, tareas RX/TX, dispatcher de comandos |
| `components/{env_sensors,door_sensor,mic_sensor,camera_sensor}/` | Un componente por fase de sensor |
| `test_apps/<comp>_test/` | Test apps Unity por componente |
| `tools/` | `monitor_sb.py`: monitor/decodificador host del enlace USB |
| `docs/` | Hardware, ADRs, arquitectura, retros, épicas |
| `openspec/` | Specs (fuente de los tests) y changes en curso |
| `.claude/` | Framework agéntico (agentes, rules, hooks, skills) |

El proceso de desarrollo (loop engineering, TDD, gitflow con gate de merge humano) está descrito en [`CLAUDE.md`](CLAUDE.md) y `docs/blueprint/`.
