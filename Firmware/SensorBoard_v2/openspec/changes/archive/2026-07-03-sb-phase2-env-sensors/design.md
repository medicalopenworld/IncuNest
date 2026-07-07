# Design — sb-phase2-env-sensors

## Decisiones

1. **Redundancia expuesta, no fusionada.** Los 3 SHT40 se publican como arrays posicionales (`temp:[t0,t1,t2]`) con `null` por sensor caído. La fusión (mediana/votación) es decisión clínica de la motherboard, no de la placa de sensores. ADR-0002.
2. **Dos buses `i2c_master`** (API nueva de IDF v6): `I2C_NUM_0` = IO41/IO42 (sht0=0x44, sht1=0x46), `I2C_NUM_1` = IO4/IO5 (sht2=0x44). El bus 1 lo compartirá la cámara (SCCB) en Fase 5: el handle del bus se expone internamente para reutilizarlo, pero no en el header público.
3. **Separación driver/conversión** (regla `embedded.md`): `sht4x_convert_*()` y `sb_env_build_event()` son funciones puras (sin I2C/RTOS) expuestas en `include/` para tests Unity; el acceso I2C/ADC vive aparte.
4. **Extensión de `status` por registro agnóstico** en `usb_comm` (`sensorBoard_status_set_sensor`): tabla estática nombre→bool, volcada por el builder. `usb_comm` no sabe qué es un SHT40; las fases 3-5 registran `mic`/`door`/`cam` igual.
5. **`sensor_task` prioridad 4**, stack 4096: por debajo del transporte (5), conforme al presupuesto de prioridades comentado en `sensorBoard_comm.c`.
6. **ALS sin calibrar**: `CONFIG_SB_ALS_UV_PER_LUX` (µV por lux, por defecto 1000) documentado como placeholder hasta calibrar contra un luxómetro; el evento publica el valor derivado y el README lo advierte.

## Riesgos

- El SHT40 necesita ~8-10 ms entre comando y lectura (high precision): se resuelve con `vTaskDelay`, no bloqueando el bus.
- Payload del evento con 3 sensores: ~120 B < 256 B — cabe con margen incluso con valores extremos.
