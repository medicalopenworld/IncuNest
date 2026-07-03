# Proposal — sb-phase4-door

## Why

La apertura de la puerta de la incubadora es un evento clínicamente relevante (pérdida térmica, intervención): la motherboard necesita saberlo en tiempo real, no por polling. Se adelanta la Fase 4 sobre la 3 por menor complejidad (GPIO+ISR vs pipeline I2S/dBA), según el orden acordado 1→2→4→3→5.

## What Changes

- Componente nuevo `door_sensor`:
  - Hall **DRV5032FB** en IO47 (ver `docs/hardware.md`). Variante de bajo consumo con muestreo interno de ~5 Hz: el propio sensor ya filtra rebotes mecánicos rápidos; el debounce de firmware (Kconfig, por defecto 250 ms ≥ periodo de muestreo del sensor) absorbe el resto. GPIO con pull-up interno (cubre salida open-drain) y polaridad configurable (`SB_DOOR_ACTIVE_LOW`, por defecto activo-bajo = imán presente = puerta cerrada).
  - **ISR de flanco (ANYEDGE) que solo hace hand-off** (`vTaskNotifyGiveFromISR` → `door_task`, prio 4), conforme a `.claude/rules/embedded.md`: nada de lógica, logs ni JSON en la ISR.
  - `door_task`: tras la señal espera la ventana de debounce, lee el nivel estable y, solo si difiere del último reportado, publica `{"type":"event","cmd":"door_open"|"door_closed","ts":<ms>}`.
  - **Estado inicial publicado al arrancar** (una vez), para que la motherboard no arranque a ciegas.
  - Lógica de decisión pura (`sb_door_logic`) y builder de evento separados del driver — testables en Unity sin hardware.
  - Registra `door` en el status (`sensorBoard_status_set_sensor`).

## Impact

- Affected specs: `door-sensor` (nueva capability).
- Affected code: `components/door_sensor/` (nuevo), `main/main.c` (init), `test_apps/door_test/` (nuevo). No toca `usb_comm` ni `env_sensors`.
