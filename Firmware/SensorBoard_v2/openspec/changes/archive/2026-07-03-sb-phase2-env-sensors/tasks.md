# Tasks — sb-phase2-env-sensors

## 1. Red

- [x] 1.1 Registro `sensorBoard_status_set_sensor` (stub) + tests de builder `status` con `sensors{}` en `comm_test`
- [x] 1.2 Componente `env_sensors` con headers y stubs puros (`sht4x_crc8`, `sht4x_convert_temp/rh`, `sb_env_build_event`, `sb_als_mv_to_lux`) + test app `env_sensors_test` con los TEST_CASE de los Scenarios — fallan sobre stubs

## 2. Green

- [x] 2.1 Implementar registro de sensores en `usb_comm` (tabla estática) + volcado en `sb_cmd_build_status`
- [x] 2.2 Implementar puros: CRC-8 Sensirion, conversiones SHT4x, builder de evento (null posicional), conversión ALS
- [x] 2.3 Driver I2C `i2c_master` (2 buses) + lectura SHT40 (cmd 0xFD, delay 10 ms, CRC por palabra)
- [x] 2.4 ADC oneshot para ALS (IO1) + Kconfig (`SB_ENV_POLL_PERIOD_S`, `SB_ALS_UV_PER_LUX`)
- [x] 2.5 `sensor_task` (prio 4): polling, publicación del evento, actualización de disponibilidad
- [x] 2.6 `main.c`: init de `env_sensors`

## 3. Verify

- [x] 3.1 `idf.py build` app principal y ambos test apps sin errores
- [x] 3.2 Cada Scenario con TEST_CASE o cobertura manual documentada

## 4. Review

- [x] 4.1 code-reviewer y security-reviewer en paralelo; hallazgos resueltos

## 5. Docs / cierre

- [x] 5.1 README + CHANGELOG + architecture.md
- [ ] 5.2 Archivar change, retro y checkbox de EPIC-001
