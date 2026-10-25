## MODIFIED Requirements

### Requirement: El aviso espera su turno y cede la pantalla

El aviso SHALL armarse solo al DESBLOQUEAR la pantalla (y una vez en la
inicialización de la UI, porque quien enciende el equipo está delante), nunca
en medio de una maniobra.

Estando armado, SHALL abrirse únicamente cuando la pantalla activa sea
`ui_ScreenMain` y NINGÚN otro diálogo modal esté abierto (menú de ayuda,
tutorial guiado, centro de alarmas, asistente de bebé, diálogo de salida,
ajuste de hora, prueba de fábrica o pantalla Bebés).

El aviso SHALL ceder la pantalla con el mismo criterio que los overlays de
solo lectura (`TelemetryHistory`, `HelpDialog`): cualquier alarma activa
(`UI_IsAnyAlarmActive()`, sin filtrar por prioridad) o la pérdida del enlace
con la placa (`Display_IsBoardLinkLost()`) lo cierran en la siguiente pasada
del bucle de UI. No lleva información de alarma propia que compense tapar la
pantalla.

Mientras el aviso esté abierto, la inactividad NO SHALL llevar la pantalla a
`ui_ScreenLock` durante `MNT_IDLE_TIMEOUT_MS` (3 min): leer el QR con el móvil
lleva más de los 20 s del auto-bloqueo. Pasado ese tope el aviso SHALL
cerrarse solo, SIN contar como respuesta — volverá a salir en el siguiente
desbloqueo si sigue habiendo algo vencido, porque solo pulsar un botón lo
calla.

El tope SHALL medirse con `lv_disp_get_inactive_time()` (3 min SIN ningún
toque; el contador se reinicia al abrir el aviso con `lv_disp_trig_activity()`)
y NO con `lv_tick_elaps()` desde la apertura. Esto es fiable porque la
exención del auto-bloqueo en `inactivity_timer_cb()` NO llama a
`lv_disp_trig_activity()` para estos overlays: si lo hiciera cada 200 ms, el
tope no llegaría nunca.

#### Scenario: No se cuela encima de otro diálogo
- **WHEN** hay algo vencido y el operador desbloquea la pantalla
- **AND** el asistente de bebé, el diálogo de salida o la pantalla Bebés está abierto
- **THEN** el aviso espera, y sale cuando ese diálogo se cierra
- *(Verificación manual en banco.)*

#### Scenario: Cierre por alarma o enlace perdido
- **WHEN** el aviso está abierto
- **AND** la placa anuncia una alarma de cualquier prioridad, o se pierde el
  enlace con la placa
- **THEN** el aviso se cierra en la siguiente pasada del bucle de UI y no se
  reabre mientras la condición dure
- *(Verificación manual en banco: provocar una alarma con el aviso abierto.)*

#### Scenario: Sin auto-bloqueo con el aviso abierto, con tope
- **WHEN** el aviso lleva abierto más de `INACTIVITY_TIMEOUT_MS` (20 s) pero
  menos de `MNT_IDLE_TIMEOUT_MS` (3 min) sin ningún toque
- **THEN** la pantalla NO se bloquea
- **AND** pasados los 3 min sin tocar el aviso se cierra solo, el auto-bloqueo vuelve a
  contar, y el aviso vuelve a salir en el siguiente desbloqueo
- *(Verificación manual en banco, cronómetro en mano.)*
