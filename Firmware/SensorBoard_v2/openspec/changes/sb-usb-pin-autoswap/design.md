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

## Señal de "host activo" (`link_up` de la política)

`link_up = tud_mounted() || tud_connected()`:

- `tud_mounted()`: SET_CONFIGURATION recibido. Se mantiene en true durante suspend (`DCD_EVENT_SUSPEND` solo pone `suspended`), así que un host dormido no dispara intercambios.
- `tud_connected()`: TinyUSB lo pone al recibir el **primer paquete SETUP** (`usbd.c`: "Mark as connected after receiving 1st setup packet") y lo borra con bus reset o unplug. Un SETUP válido (NRZI + SYNC + CRC) es imposible con D+/D- cruzados, así que es prueba de orientación correcta. Esta guarda hace la política conservadora: un host lento en llegar a SET_CONFIGURATION (reintentos, pila arrancando, hub) nunca provoca un intercambio, con lo que no puede darse un batido entre el reloj de alternancia y los reintentos del host. Por eso no hace falta backoff ni jitter.

> Corrección respecto al diseño inicial: se había descartado `tud_connected()` asumiendo que se ponía con el bus reset (SE0, simétrico ante el cruce). La revisión de seguridad lo contrastó con el fuente de TinyUSB y era falso; la premisa corregida queda en ADR-0003.

Sin VBUS sensing no hay `DCD_EVENT_UNPLUGGED`: si el host desaparece sin bus reset, `tud_mounted()` se queda en true y la política no actúa (conservador por diseño). El siguiente bus reset del host (al reconectar, en cualquier orientación) borra `cfg_num`/`connected` y rearranca el ciclo.

## Integración en `usb_comm`

- `sensorBoard_comm_init()`: justo antes de crear `usb_tx_task` inicializa la política con `CONFIG_SB_USB_AUTOSWAP_TIMEOUT_MS` (0 si `SB_USB_AUTOSWAP` está desactivado) y publica `sensors.usb_swap=false` en el registro de `status`.
- `usb_tx_task`: al inicio de cada iteración (cadencia ≤ `SB_TX_IDLE_POLL_MS` = 20 ms en reposo) llama a `tick(link_up, now_ms)`. Si devuelve SWAP y el host sigue inactivo: `s_cdc_ready=false` (re-arma la retención: DTR es pegajoso porque TinyUSB no invoca `line_state_cb` en reset/unplug) → `tud_disconnect()` → `usb_wrap_ll_phy_enable_pin_exchg(&USB_WRAP, swapped)` → `vTaskDelay(250 ms)` (detach visible incluso a través de hubs/pilas host que sondean el puerto) → `tud_connect()` → `sensors.usb_swap=swapped` → `ESP_LOGW`. Contexto de tarea, nunca ISR; sin host activo la tarea TinyUSB no tiene actividad de endpoints, así que los read-modify-write de `otg_conf`/`dctl` no compiten con nadie.
- `cdc_writable() = s_cdc_ready && tud_cdc_n_connected()`: todas las escrituras al CDC (frames, retención, binarios) usan esta conjunción para que un host que desaparece sin cerrar el puerto no deje al firmware escribiendo en un FIFO muerto — los frames vuelven a la retención de arranque.
- `dcd_connect/dcd_disconnect` del port DWC2 de TinyUSB hacen read-modify-write de `USB_WRAP.otg_conf` (solo los campos de pull), así que preservan `exchg_pins`/`exchg_pins_override` (verificado en `dcd_dwc2.c`).
- Dependencias: `PRIV_REQUIRES esp_hal_usb` (`hal/usb_wrap_ll.h`); el registro `USB_WRAP` viene de `soc` (requerimiento común).

## Tests (Unity, `comm_test`, sin host)

Un `TEST_CASE` por Scenario del delta de `usb-transport`: no intercambia antes del plazo; intercambia al vencer y rearma; alterna; con enlace no intercambia y rearma; robusto al wrap de `now_ms`; timeout 0 desactiva.

## Verificación on-target (manual)

Hardware: SensorBoard **HW_NUM 4** (conector con el cruce); en la V5 el caso 2 no es reproducible salvo con un cable/adaptador cruzado a propósito.

1. Cable en orientación correcta: enumera en <1 s, sin `ESP_LOGW` de intercambio; `status` → `sensors.usb_swap:false`.
2. Cable invertido: enumera en ~2–2.5 s; al conectar el monitor aparece el `ESP_LOGW` de intercambio retenido; `status` → `sensors.usb_swap:true`.
3. Desenchufar y volver a enchufar en la otra orientación con el SB alimentado: vuelve a enumerar.
4. Sin host durante minutos: al conectar en cualquier orientación enumera.
5. Host lento: a través de un hub USB 2.0 barato y con la motherboard real (no solo un PC de desarrollo), la orientación correcta enumera sin intercambios (`swap_count` = 0 en el log).
6. Cada intercambio produce un detach/attach visible en el host (eventos de puerto, p. ej. `dmesg`/`usbipd`), no solo "acaba enumerando".
7. Enlace montado → apagar el host sin cerrar el puerto (DTR queda pegado) → volver a encender: los frames del intervalo aparecen retenidos al reconectar, no se pierden.
