# Proposal — sb-phase2-env-sensors

## Why

Con el transporte (Fase 1) operativo, la incubadora necesita su telemetría ambiental básica: temperatura y humedad relativa (control térmico del habitáculo — 3 sensores SHT40 para redundancia en zonas distintas) y luz ambiental (ciclo día/noche del neonato). Es la Fase 2 del roadmap y la primera que publica datos reales por `usb_comm`.

## What Changes

- Componente nuevo `env_sensors`:
  - **SHT40 ×3** (datasheet Sensirion SHT4x): dos en el bus I2C de temperatura IO41/IO42 (`SHT40-AD1B` 0x44 + `SHT40-CD1B` 0x46) y uno en el bus principal IO4/IO5 (`SHT40-AD1B` 0x44). Driver `i2c_master` (API nueva de IDF), medición de alta precisión (cmd 0xFD), validación CRC-8 Sensirion por palabra, conversión pura ticks→°C/%RH testeable.
  - **ALS-PT19** (fototransistor analógico) por ADC oneshot en IO1 (ADC1_CH0), conversión mV→lux con factor de escala en Kconfig **documentado como no calibrado** (depende de la resistencia de carga del esquema).
  - **`sensor_task`** (prioridad 4, < transporte): polling periódico (Kconfig, por defecto 5 s), publica `{"type":"event","cmd":"sensor_data","data":{"temp":[t0,t1,t2],"hum":[h0,h1,h2],"lux":L},"ts":…}` — posición = sensor, `null` si ese sensor falló (NACK/timeout/CRC): la motherboard decide la fusión/redundancia.
  - Fallo de sensor no bloquea ni crashea: se marca no-disponible y se sigue.
- Extensión **acotada y agnóstica** de `usb_comm`: registro genérico de disponibilidad `sensorBoard_status_set_sensor(name, bool)` (tabla estática de 8 entradas, nombres opacos) que `sb_cmd_build_status` vuelca como `"sensors":{...}` en la resp de `status`. No toca framing/CRC/tareas RX-TX; es el punto de extensión que el roadmap preveía desde la Fase 1 y que Fases 3-5 reutilizarán.

## Impact

- Affected specs: `env-sensors` (nueva), `command-dispatch` (campo `sensors` en `status`).
- Affected code: `components/env_sensors/` (nuevo), `components/usb_comm/` (solo `sensorBoard_cmd_builder.*` + tabla de registro nueva), `main/main.c` (init), `test_apps/comm_test` (tests de builder) + `test_apps/env_sensors_test` (nuevo).
- Nombres registrados: `sht0`, `sht1`, `sht2`, `als`.
