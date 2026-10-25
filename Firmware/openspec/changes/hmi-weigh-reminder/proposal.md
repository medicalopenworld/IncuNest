## Why

Afecta solo a **Display_HMI**. No toca `shared/`, ni el protocolo serie, ni la
`motherBoard`.

La curva de peso es el dato clínico clave del panel de ThingsBoard y del
historial de bebés, y hoy depende de que alguien se acuerde de pesar y de
teclear el peso. El HMI solo pide un peso en un momento: al activar la
terapia de un bebé (asistente `BabyWizard`). Un bebé que lleva días dentro de
la incubadora sin que se reactive nada no vuelve a pasar por ese paso, y no
hay ningún aviso ni ningún camino rápido para registrar una pesada nueva.

## What Changes

- **Aviso de pesada diaria**: con un bebé activo (`BabyWizard_HasLiveSession()`)
  y sin pesada registrada en las últimas 24 h, al desbloquear la pantalla sale
  un aviso con dos botones, **PESAR AHORA** y **MÁS TARDE**. Mismo motor y
  mismas reglas de ceder paso que el aviso de mantenimiento: armado por el
  desbloqueo, solo en `ui_ScreenMain`, sin otro modal abierto, cede ante
  cualquier alarma o enlace perdido, tope de 3 min de inactividad, snooze de
  24 h.
- **Sello de la última pesada en la NVS del HMI**. El protocolo solo da
  resolución de día relativa a la admisión (`CTRL,WEIGHT_HISTORY`) y no hay
  epoch de la última pesada, así que el HMI lo sella él mismo al enviar
  `HMI,PROFILE_WEIGHT` con un peso real (≠ SKIP): epoch UTC y `seq` en claves
  nuevas de `hmi_cfg` (`wgh_*`). Sin hora sincronizada no se avisa.
- **Decisión pura y testeada en host** (`modules/maintenance/weigh_reminder`,
  sin LVGL ni Arduino): primer código del HMI de este tipo con suite Unity en
  `[env:native]`.
- **PESAR AHORA** abre una vista de entrada de peso dentro del mismo diálogo
  (teclado numérico, 400-5000 g, el mismo rango que el asistente) que envía
  `HMI,PROFILE_WEIGHT` para el bebé activo. Hoy no existe ningún flujo que
  pese al bebé activo sin pasar por la activación de terapia, así que se crea
  este mínimo.
- **Corrección del hueco de cesión**: `BabyHistory` (pantalla Bebés) no estaba
  en la lista de overlays ante los que espera el aviso de mantenimiento. Se
  añade (`fix(hmi)`), y vale para los dos avisos.
- **Corrección documental**: `docs/hmi.md` y el spec
  `hmi-maintenance-reminder` decían que el tope de 3 min se mide con
  `lv_tick_elaps()` desde la apertura; el código usa
  `lv_disp_get_inactive_time()` (3 min SIN ningún toque), que es correcto y
  está documentado así en `MaintenanceDialog.h`.
- Cadenas nuevas en las cuatro lenguas.

## Impact

- Código: `src/modules/maintenance/weigh_reminder.{h,cpp}` (puro),
  `src/modules/maintenance/weigh_tracker.{h,cpp}` (estado + NVS),
  `src/ui/MaintenanceDialog.cpp` (modo de pesada), `src/tasks/CommTask.cpp`
  (sello al enviar la pesada), `src/tasks/UITask.cpp` (flush de NVS fuera de
  `LVGL_Lock`), catálogo de cadenas, `include/config/EEPROM_defines.h`,
  `platformio.ini` (filtro de `[env:native]`), `test/test_weigh_reminder/`.
- Specs: nueva capacidad `hmi-weigh-reminder`; `hmi-maintenance-reminder`
  modificada (lista de diálogos y medida del tope).
- Known issues: no toca alarmas, ni el reparto de tramas, ni la carga de
  firmware. El envío de `PROFILE_WEIGHT` no cambia; solo se anota en la NVS
  del HMI.
- Sin cambio de `FWversion`.
