# Design — sb-usb-autoswap-host-evidence

## Diagnóstico (banco 2026-09-03, HW4 + motherboard V18)

Con la política de `sb-usb-pin-autoswap` (alternar cada `T`=2 s mientras no haya host activo) el SensorBoard alterna desde el arranque aunque no haya host. La pila host de la motherboard (ESP-IDF 4.4.6 en Arduino) reacciona así:

- Si al encender su puerto encuentra la ventana correcta: enumera en <1 s (verificado: `dispositivo enumerado VID 0x303A PID 0x4001 speed=1`).
- Si encuentra la ventana equivocada: falla la enumeración (`HUB: Bad transfer status 1: CHECK_SHORT_DEV_DESC`), deshabilita el puerto y **solo lo recupera con la siguiente desconexión** (el detach de 250 ms del siguiente intercambio). La recuperación + debounce + reset tarda más que la ventana buena (1,75 s) y menos que el ciclo (4 s): el reintento cae otra vez en la ventana mala. Enganche de fase: errores cada 4,01 s durante minutos, `devices=0`.

Con un PC como host (Windows) no ocurre: recupera el puerto en ~100 ms y enumera en la ventana buena. El problema es la alternancia a ciegas, no el intercambio en sí.

## Política v2: intercambiar solo con evidencia de host, y una sola vez por evidencia

```c
void sb_usb_orient_init(sb_usb_orient_t *st, uint32_t timeout_ms);
sb_orient_action_t sb_usb_orient_tick(sb_usb_orient_t *st, bool host_active,
                                      bool bus_reset_seen, uint32_t now_ms);
```

- `host_active` = `tud_mounted() || tud_connected()` (sin cambios).
- `bus_reset_seen` = TinyUSB entregó al menos un `DCD_EVENT_BUS_RESET` desde el último tick (flanco). Un bus reset (SE0 ≥10 ms) solo lo emite un host: es la evidencia de que hay alguien al otro lado. Es simétrico ante el cruce, así que llega en ambas orientaciones.
- Estado `armed`: se arma con el **primer** reset (`deadline = now + T`); resets posteriores **no extienden** el plazo (Windows hace 3 resets en <1 s antes de rendirse; si extendieran, nunca se intercambiaría).
- `host_active` → `armed = false` (el host nos habla: orientación correcta).
- `armed && deadline vencido && !host_active` → `swapped = !swapped`, `swap_count++`, `armed = false`, devuelve SWAP. **No se vuelve a intercambiar hasta ver otro reset**: la orientación nueva se mantiene indefinidamente mientras el host tarde en recuperarse.
- Sin ningún reset (sin host): la orientación no cambia jamás. El SensorBoard arranca en normal y se queda ahí, así que un host que arranque después con el cable correcto encuentra siempre la ventana buena.
- `timeout_ms == 0` desactiva la política.

Secuencia con cable invertido y la motherboard: SB normal → host enciende puerto → reset (evidencia) → LS, sin SETUP → a los 2 s SB intercambia (detach 250 ms + attach) → la desconexión recupera el puerto del host → reset → FS → SETUP → enlace. Un solo intercambio, sin enganche posible porque el SB ya no se mueve.

## Integración en `usb_comm`

- `tud_event_hook_cb(rhport, eventid, in_isr)` (weak en TinyUSB, se define fuerte en `sensorBoard_comm.c`): si `eventid == DCD_EVENT_BUS_RESET` incrementa un contador `volatile uint32_t s_bus_resets` (contexto de tarea TinyUSB, no ISR; el hook no debe bloquear).
- `usb_tx_task` → `orient_service()`: `bus_reset_seen = (s_bus_resets != s_bus_resets_seen)`; actualiza `s_bus_resets_seen`. Contador en vez de flag: el productor y el consumidor son tareas distintas y un flag "leer y borrar" perdería resets.
- El resto (detach 250 ms, `usb_wrap_ll_phy_enable_pin_exchg`, `s_cdc_ready=false`, `sensors.usb_swap`, `ESP_LOGW`) no cambia.
- `sb_usb_orient_init` ya no necesita `now_ms` (no arma nada al arrancar).

## Motherboard

Con la política v2 la pila host 4.4.6 no necesita recuperación adicional: el único intercambio provoca la desconexión que recupera el puerto. Se conserva como mejora permanente el log `SensorBoard enumerado VID/PID/velocidad` (`new_dev_cb`) a nivel INFO para diagnóstico en campo; el resto de la instrumentación temporal se retira.

## Tests (Unity, `comm_test`, sin host)

Un `TEST_CASE` por Scenario del delta: sin evidencia nunca intercambia; reset sin host → un intercambio a los `T` y quieto después; resets repetidos no extienden; host activo desarma; segundo reset → vuelve a intercambiar; desbordamiento; timeout 0.

## Verificación on-target (manual)

Con la motherboard como host (COM18):

1. Cable correcto, arranque conjunto: `SensorBoard conectado por USB` en <2 s tras arrancar el host, sin `swapped` en los `[SB]`.
2. Cable invertido, arranque conjunto: `[SB] ... D+/D- swapped (swap #1)` y `SensorBoard conectado por USB` en <5 s tras arrancar el host; sin `HUB: Bad transfer status` repetidos.
3. Con enlace activo, desenchufar y enchufar al revés: reenlaza solo (un intercambio).
4. Repetir 1 y 2 con el PC como host (`tools/monitor_sb.py`).
5. Sin host 5 min y después conectar: `usb_swap:false` (no ha alternado).
