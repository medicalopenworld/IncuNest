## 1. Lógica pura con test (`[env:native]`)

- [ ] 1.1 `modules/control/modem_power_cycle.{h,cpp}` y `test/test_modem_power_cycle`, alta en `[env:native]` `build_src_filter` (test antes del feat)
- [ ] 1.2 `modules/control/afe_watchdog.{h,cpp}` y `test/test_afe_watchdog`, alta en `[env:native]`

## 2. motherBoard — entorno y pinout

- [ ] 2.1 `[env:IncuNest_V19]` en `platformio.ini` (HW_NUM=19, PSRAM quad); excluir `hal_hw19.cpp` de V17/V18 y `hal_hw17.cpp` de V19
- [ ] 2.2 `board.h`: `#error` con 19, bloque `HW_NUM >= 19`, `>= 18` en shunts y `HEATER_POWER_REFERENCE_IS_SYSTEM_CURRENT`, `GPRS_PWRKEY 0`, `EXT_WDT_ENABLED 0`
- [ ] 2.3 `hal.h` y `hal_hw{16,17,19}.cpp`
- [ ] 2.4 Log de arranque: `psramFound()` y `ESP.getPsramSize()`
- [ ] 2.5 UART de depuración: `debugSerial.begin` explícito en V19 (sin `HardwareSerial` nueva, ver design D8)

## 3. motherBoard — raíles, módem, CHRG_OK, watchdog externo

- [ ] 3.1 `src/system/power_rails.{h,cpp}` (`pwr5vSet`, `gsmPowerSet`, `mainsPresentPin`, power-cycle, toggle WDI), no-op en HW<19
- [ ] 3.2 Secuencia de arranque en `setup()` (5 V primero, GSM antes de la tarea GPRS)
- [ ] 3.3 Power-cycle en `GPRSForceReset()`/`GPRSPowerUp()` y en `mon_kill` de `GPRSMonitorTask`
- [ ] 3.4 `GSM_power_cycles` en DIAG (GPRS y WiFi) y claves en `telemetry_keys.h`
- [ ] 3.5 Toggle de `EXT_WDT_WDI` en `watchdogReload()` bajo `EXT_WDT_ENABLED`
- [ ] 3.6 `mains_ok` en `/debug/state`

## 4. motherBoard — AFE4490

- [ ] 4.1 `afePdnPulse()` y reutilización en `initSPO2()`
- [ ] 4.2 `AFE_diag` en `/debug/state`, DIAG y línea de log de SPO2
- [ ] 4.3 Watchdog de muestras con `stop()`/PDN/`begin()` (solo V19) y `AFE_resets` en DIAG

## 5. Verificación

- [ ] 5.1 `pio run -e IncuNest_V19` y `pio run -e IncuNest_V18` en verde; anotar RAM/Flash de ambos
- [ ] 5.2 `pio test -e native` en `Firmware/motherBoard`
- [ ] 5.3 `python tools/check_transport_matrix.py`: el peor caso de claves sigue dentro de `THINGSBOARD_FIELDS_AMOUNT`
- [ ] 5.4 **Manual, banco V19**: arranque con `PWR_5V_EN`/`GSM_PWR_EN` a los niveles esperados y orden correcto
- [ ] 5.5 **Manual, banco V19**: `psramFound()=1` y `getPsramSize()` ≈ 2 MB en el log de arranque
- [ ] 5.6 **Manual, banco V19**: power-cycle del módem provocado (apagar la red/forzar `GPRSForceReset`) con `GSM_PWR_EN` bajo ≥ 2 s medido con osciloscopio y `GSM_power_cycles` incrementado
- [ ] 5.7 **Manual, banco V19**: `AFE_diag` visible con sonda ausente; reset del AFE forzado (cortar DRDY) y recuperación con `AFE_resets` incrementado
- [ ] 5.8 **Manual, banco V19**: `/debug/state` refleja `mains_ok` al conectar y desconectar la fuente
- [ ] 5.9 **Manual, banco V19**: log legible por J1 (IO43/IO44) a 115200
- [ ] 5.10 **Manual, banco V18**: la unidad de banco arranca y publica igual que antes (sin claves `GSM_power_cycles`/`AFE_resets`)

## 6. Docs

- [ ] 6.1 `Firmware/docs/hw19.md`, enlace desde `docs/partitions.md` y desde el índice de docs si existe
