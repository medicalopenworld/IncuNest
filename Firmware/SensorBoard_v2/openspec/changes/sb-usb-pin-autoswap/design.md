# Design — sb-usb-pin-autoswap

## Política de orientación (`sensorBoard_usb_orient.{h,c}`)

Módulo puro, sin includes de TinyUSB ni de registros, con estado explícito:

```c
typedef enum { SB_ORIENT_NONE = 0, SB_ORIENT_SWAP } sb_orient_action_t;
typedef struct { uint32_t timeout_ms, deadline_ms, swap_count; bool swapped; } sb_usb_orient_t;

void sb_usb_orient_init(sb_usb_orient_t *st, uint32_t timeout_ms, uint32_t now_ms);
sb_orient_action_t sb_usb_orient_tick(sb_usb_orient_t *st, bool link_up, uint32_t now_ms);
bool sb_usb_orient_is_swapped(const sb_usb_orient_t *st);
uint32_t sb_usb_orient_swap_count(const sb_usb_orient_t *st);
```

- `init`: orientación normal, `deadline = now + timeout`. `timeout_ms == 0` ⇒ política desactivada (nunca devuelve SWAP).
- `tick(link_up=true)`: rearma `deadline = now + timeout`, devuelve NONE (con enlace jamás se intercambia; una desconexión posterior cuenta desde su caída, no desde el arranque).
- `tick(link_up=false)`: si `(int32_t)(now - deadline) >= 0` ⇒ conmuta `swapped`, `swap_count++`, rearma y devuelve SWAP. Comparación con resta con signo ⇒ robusta al desbordamiento de `now_ms` (uint32, ~49.7 días).
- Alterna indefinidamente sin host: el host puede aparecer más tarde en cualquiera de las dos orientaciones. Coste nulo (sin host, el bus está inerte).

## Señal de "enlace montado"

`tud_mounted()` (SET_CONFIGURATION recibido). **No** `tud_connected()`: con D+/D- cruzados el host ve un dispositivo low-speed y aun así emite bus reset (SE0, simétrico ante el cruce), lo que pondría `connected=true` sin que la enumeración pueda prosperar.

## Integración en `usb_comm`

- `sensorBoard_comm_init()`: tras `tinyusb_driver_install()` inicializa la política con `CONFIG_SB_USB_AUTOSWAP_TIMEOUT_MS` (0 si `SB_USB_AUTOSWAP` está desactivado).
- `usb_tx_task`: al inicio de cada iteración (cadencia ≤ `SB_TX_IDLE_POLL_MS` = 20 ms en reposo) llama a `tick(tud_mounted(), now_ms)`. Si devuelve SWAP: `tud_disconnect()` → `usb_wrap_ll_phy_enable_pin_exchg(&USB_WRAP, swapped)` → `vTaskDelay(50 ms)` (el host ve un detach limpio) → `tud_connect()` → `ESP_LOGW`. Contexto de tarea, nunca ISR.
- `dcd_connect/dcd_disconnect` del port DWC2 de TinyUSB hacen read-modify-write de `USB_WRAP.otg_conf` (solo los campos de pull), así que preservan `exchg_pins`/`exchg_pins_override` (verificado en `dcd_dwc2.c`).
- Dependencias: `PRIV_REQUIRES esp_hal_usb` (`hal/usb_wrap_ll.h`); el registro `USB_WRAP` viene de `soc` (requerimiento común).

## Tests (Unity, `comm_test`, sin host)

Un `TEST_CASE` por Scenario del delta de `usb-transport`: no intercambia antes del plazo; intercambia al vencer y rearma; alterna; con enlace no intercambia y rearma; robusto al wrap de `now_ms`; timeout 0 desactiva.

## Verificación on-target (manual)

1. Cable en orientación correcta: enumera en <1 s, sin `ESP_LOGW` de intercambio.
2. Cable invertido: enumera en ~2–2.5 s; al conectar el monitor aparece el `ESP_LOGW` de intercambio retenido.
3. Desenchufar y volver a enchufar en la otra orientación con el SB alimentado: vuelve a enumerar.
4. Sin host durante minutos: al conectar en cualquier orientación enumera.
