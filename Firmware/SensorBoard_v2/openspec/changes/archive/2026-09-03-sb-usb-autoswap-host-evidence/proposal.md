# Proposal — sb-usb-autoswap-host-evidence

## Why

En banco (2026-09-03, SensorBoard HW4 + motherboard V18) el autoswap de `sb-usb-pin-autoswap` funciona con un PC como host en ambas orientaciones, pero con la motherboard falla de forma intermitente: su pila host (ESP-IDF 4.4.6 dentro de Arduino) **no se recupera** si lo primero que ve tras encender el puerto es un dispositivo low-speed que no responde (el SensorBoard en la ventana de orientación equivocada). Como la política actual alterna a ciegas cada 2 s desde el arranque, aunque no haya host, el estado en el que la motherboard encuentra al SensorBoard es una moneda al aire: en la orientación correcta del cable, la mitad de los arranques dejan al host muerto (150 s sin enumerar, `SENSORBOARD LINK LOST`), y con el cable invertido nunca enumera.

## What Changes

- **Política de orientación con evidencia de host.** `sb_usb_orient_tick()` recibe además `host_reset_seen` (flanco: TinyUSB entregó `DCD_EVENT_BUS_RESET` desde el último tick). Solo arma el plazo cuando ha visto un bus reset; si el host no llega a hablarnos (ni SETUP ni `SET_CONFIGURATION`) en `T` tras ese reset, intercambia **una vez** y vuelve a esperar evidencia. Sin bus reset (sin host) la orientación no cambia jamás: el SensorBoard arranca y permanece en orientación normal, así que un host que arranque después encuentra siempre la ventana correcta si el cable está bien.
- **Hook de TinyUSB** `tud_event_hook_cb()` en `usb_comm` para capturar `DCD_EVENT_BUS_RESET` (contexto de tarea TinyUSB) en un flag consumido por `usb_tx_task`.
- **Motherboard (fuera de este componente, misma rama):** recuperación del host cuando lleva >8 s sin ningún dispositivo enumerado — reinstalación de `cdc_acm_host` y `usb_host` — porque con el cable invertido la primera ventana es low-speed por definición y la pila 4.4.6 no sale de ese estado por sí sola.

## Impact

- Affected specs: `usb-transport` (MODIFIED "Tolerancia a la inversión de D+/D-").
- Affected code: `components/usb_comm/sensorBoard_usb_orient.{c,h}`, `sensorBoard_comm.c`, `test_apps/comm_test/main/test_usb_orient.c`; motherboard `src/modules/sensorboard_comm/sensorboard_comm.cpp`.
- Sin cambio de protocolo.

## Non-goals

- Cambiar la versión de Arduino/ESP-IDF de la motherboard.
- Persistir la orientación en NVS (deja de ser necesario: sin host no se alterna).
