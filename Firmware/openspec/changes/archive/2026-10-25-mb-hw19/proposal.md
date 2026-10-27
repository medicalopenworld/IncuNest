## Why

Placas afectadas: **motherBoard** únicamente. `shared/` y `Display_HMI` no cambian: el protocolo con el display no se toca (la V19 sigue hablando por UART1 en IO15/IO16).

La motherBoard **V19 (HW_NUM=19)** aún no existe físicamente, pero el firmware tiene que compilar para ella y dejar la preparación hecha para el primer banco. La V19 cambia el módulo a ESP32-S3-WROOM-1-**N8R2** (8 MB flash + 2 MB PSRAM quad) y gobierna por primera vez, desde el micro, cosas que hasta la V18 estaban siempre alimentadas:

- el raíl de **5 V** (humidificador, LEDs del AFE, SensorBoard por USB, display HMI) tras un buck con EN (`PWR_5V_EN`, IO40);
- la alimentación del **SIM800C** (`GSM_PWR_EN`, IO39, buck propio; PWRKEY a GND, auto-encendido al aplicar VBAT);
- el **PDN del AFE4490** (`AFE_PDN`, IO7), que en la V18 caía en `FAKE_PIN`;
- un **watchdog externo** TPS3431 (`EXT_WDT_WDI`, IO41, montaje por confirmar);
- la señal **CHRG_OK** del BQ25730 (`MAINS_OK`, IO42): fuente de entrada válida;
- una UART de depuración en el conector J1 (IO43/IO44).

Esto abre dos mejoras de robustez que en la V18 son imposibles porque no hay pin ni potencia que conmutar: un **power-cycle real del módem** (hoy `GPRSForceReset()` solo manda `AT+CPOWD=1` y el módem se queda colgado cuando el AT ya no contesta) y un **reset físico del AFE4490** con watchdog de muestras y diagnóstico publicado.

Área con known issues: arranque y comunicación serie (`Firmware/docs/known_issues.md`). El orden de arranque que ya estaba validado (SPO2 antes del host USB, etc.) se conserva; solo se antepone el encendido de los raíles.

**Restricción dura: la V18 (unidad de banco) no cambia de comportamiento.** Todo lo nuevo va tras `HW_NUM >= 19` o con no-op en HW<19.

## What Changes

- Nuevo entorno `[env:IncuNest_V19]` (clon de V18, `-DHW_NUM=19`, `-DBOARD_HAS_PSRAM`, PSRAM quad). El log de arranque imprime `psramFound()` y `ESP.getPsramSize()`.
- `board.h`: bloque `HW_NUM >= 19` con los pines nuevos; la V19 hereda de la V18 shunts y referencia de corriente del calefactor (`>= 18`). `GPRS_PWRKEY` pasa a 0 en V19 para que no se ejecute el bloque `#if (GPRS_PWRKEY)` de `GPRS.cpp` (PWRKEY va a GND).
- HAL: nueva tabla `hal_hw19.cpp`.
- Módulo `src/system/power_rails.{h,cpp}`: `pwr5vSet()`, `gsmPowerSet()`, `mainsPresentPin()`, el toggle del watchdog externo y el power-cycle del módem; no-op en HW<19.
- Secuencia de arranque HW19: `PWR_5V_EN` lo primero, `GSM_PWR_EN` antes de la tarea GPRS.
- **Power-cycle real del módem** (HW19) en `GPRSForceReset()` y en el camino `mon_kill` del `GPRSMonitorTask`, con contador `GSM_power_cycles` en el grupo DIAG de telemetría. Lógica de temporización pura en `modules/control/modem_power_cycle.*` con tests Unity.
- **AFE4490**: `AFE_diag` (`diag_code` de la librería) en `/debug/state` y en telemetría DIAG; watchdog de muestras (>10 s sin muestra → `stop()`, pulso de PDN, `begin()`, máximo una vez por minuto, contador `AFE_resets`) solo en HW19. Lógica pura en `modules/control/afe_watchdog.*` con tests Unity.
- **Watchdog externo**: toggle de `EXT_WDT_WDI` junto a `watchdogReload()`, desactivado por defecto (`EXT_WDT_ENABLED 0`).
- **CHRG_OK**: `mainsPresentPin()` y su publicación en `/debug/state`. No se implementa la alarma de corte de red (rama `feat/mains-detector`).
- **UART de depuración**: no se añade ninguna `HardwareSerial` nueva; ver `design.md` (el log ya sale por UART0 = IO43/IO44).
- Doc `Firmware/docs/hw19.md`.

## Capabilities

### New Capabilities

- `hw19-board-support`: soporte de la motherBoard V19: entorno de compilación y PSRAM, pinout, secuencia de alimentación de los raíles controlados, power-cycle del módem, diagnóstico y reset del AFE4490, watchdog externo y detección de red por CHRG_OK, con la garantía de que la V18 no cambia.

## Impact

- **Código — motherBoard**: `platformio.ini`, `include/config/board.h`, `include/config/telemetry_keys.h`, `src/hal/hal.h` y `hal_hw1{6,7,9}.cpp`, `src/main.cpp` (`setup()`, `GPRSMonitorTask`, log de arranque), `src/system/initHardware.cpp`, `src/system/ESP32_config.cpp`, `src/system/power_rails.*` (nuevo), `src/tasks/GPRS.cpp`, `src/tasks/Wifi_OTA.cpp`, `src/drivers/SPO2.cpp`, `src/modules/debug/debug_mode.cpp`, `src/modules/control/{modem_power_cycle,afe_watchdog}.*` (nuevos).
- **Código — shared/Display_HMI**: ninguno.
- **Docs**: `Firmware/docs/hw19.md` (nuevo), `Firmware/docs/partitions.md` (nota del módulo).
- **Testing**: la temporización del power-cycle y el watchdog del AFE son lógica pura en `modules/control/` y entran en `[env:native]` con tests Unity (commit `test(...)` antes del `feat`). Todo lo que toca pines, buck, PSRAM, PDN, TPS3431 o CHRG_OK es **verificación manual en banco V19**: no existe entorno de test para ello y no se afirma cobertura que no hay.
- **Compatibilidad**: la V18 y la V17 no cambian de comportamiento; se comprueba comparando que `pio run -e IncuNest_V18` compila y que ningún camino nuevo se compila en ella.
- **Out of scope (Non-goals)**:
  - La alarma de corte de red (rama `feat/mains-detector`, que usará `mainsPresentPin()`).
  - Mover buffers a `ps_malloc` o medir el heap con PSRAM (pendiente de banco).
  - Fijar la ventana del TPS3431 y activar `EXT_WDT_ENABLED`.
  - Cambiar versiones de firmware o de librerías.
