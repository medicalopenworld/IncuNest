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
- La resp de `status` incluye ahora `"sensors":{"sht0":…,"sht1":…,"sht2":…,"als":…}` con la disponibilidad real.

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
| `test_apps/comm_test/` | Test app Unity del transporte |
| `docs/` | Hardware, ADRs, arquitectura, retros, épicas |
| `openspec/` | Specs (fuente de los tests) y changes en curso |
| `.claude/` | Framework agéntico (agentes, rules, hooks, skills) |

El proceso de desarrollo (loop engineering, TDD, gitflow con gate de merge humano) está descrito en [`CLAUDE.md`](CLAUDE.md) y `docs/blueprint/`.
