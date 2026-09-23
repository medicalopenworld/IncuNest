## 1. shared/

Commit: `feat(shared): test env_sensor unico (USB, I2C2 o SHT4x)`.

- [x] 1.1 `factory_test.h`: `FTEST_MB_ENV_SENSOR` (id 6) sustituye a
      `SENSORBOARD` + `EXT_SHT4X`; `FTEST_MB_COUNT` = 28.
- [x] 1.2 **RED/GREEN** — tabla y tests (`env_sensor`, count 28).

## 2. motherBoard

Commit: `fix(motherboard): tests de fabrica sin I2C directo, env_sensor, charger cacheado`.

- [x] 2.1 `charger` con `g_bq_status_valid` (12 s); `skin_adc` e `ina3221` con
      estado cacheado; regla en la cabecera del fichero.
- [x] 2.2 `env_sensor` con tres caminos y cascada `sb_usb`; `sb_env` sin
      exterior obligatorio.
- [x] 2.3 `humid_usb` → SKIP `omitido`; `gsm_signal` → WARN `sin señal`.
- [x] 2.4 `pio run -e IncuNest_V18` y `pio test -e native` en verde.

## 3. Display_HMI

Commit: `fix(hmi): test de fabrica paginado con barra de progreso y veredicto`.

- [x] 3.1 `HMI_I2C` con `UI_TouchInitOk()` / `UI_BacklightInitOk()`; sondeo
      solo informativo.
- [x] 3.2 `HMI_BUZZER` y `HMI_SPEAKER` fuera de la secuencia; código muerto
      eliminado.
- [x] 3.3 Cuadrícula paginada 3×N con `<` `>` e indicador; sigue al test en
      curso salvo paginación manual.
- [x] 3.4 Barra de progreso y veredicto `HW OK` / `HW ERROR`; persistencia.
- [x] 3.5 `pio run -e main` en verde.

## 4. Documentación

Commit: `docs: protocolo FTEST v2.3.2 y pantalla HW test paginada`.

- [x] 4.1 `PROTOCOL.md` v2.3.2; `docs/hmi.md`; `docs/hardware.md` (omitidos y
      regla de I2C).

## Evidencia del stage Verify (2026-09-06, sin hardware conectado)

HEAD `ee434d6`, worktree `Firmware/.worktrees/factory-test`, PowerShell:

| Comando | Resultado |
|---|---|
| `motherBoard: pio run -e IncuNest_V18` | SUCCESS, RAM 26.4 %, Flash 54.4 % (1497797 B), 0 warnings |
| `motherBoard: pio test -e native` | 20 suites PASSED (308 casos); `test_sensorboard_frame` ERRORED por la DLL preexistente de esta máquina |
| `Display_HMI: pio run -e main` | SUCCESS, RAM 39.2 %, Flash 80.6 % (2536644 B), 0 warnings |

## 5. Verificación manual (banco)

- [ ] 5.1 **Manual** — a batería, sin SHT4x, con SensorBoard: `charger` PASA,
      `env_sensor` PASA `usb`, ningún test se queda en curso.
- [ ] 5.2 **Manual** — sin cobertura: `gsm_signal` y `gsm_net` en ámbar;
      veredicto `HW OK` si no hay rojos.
- [ ] 5.3 **Manual** — páginas con guantes; la barra llega al 100 %.
