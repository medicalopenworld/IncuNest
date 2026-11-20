## Why

Afecta solo a **Display_HMI**. Pone al día el spec `hmi-weigh-reminder` con lo
que cambió tras la revisión de código de `hmi-weigh-reminder` (commits
`c04b4184`, `9730199a`, `dc094532`), que dejó el spec archivado describiendo
un comportamiento que ya no existe.

## What Changes

- **Sello con confirmación desde PESAR AHORA**: el diálogo sella solo al
  recibir `CTRL,PROFILE_RANGE` (ventana de 3 s); sin ella, toast de error y sin
  sello. El asistente y la pantalla Bebés siguen sellando al enviar. Antes, un
  enlace caído callaba el aviso 24 h con un peso que no había llegado.
- **Interruptor `wgh_en`** en Ajustes > Mantenimiento, ON por defecto,
  rechazado en formación.
- **Armados**: no se consume el de pesada sin bebé al mando; un mantenimiento
  cerrado sin contestar no encadena la pesada; el cierre por alarma devuelve el
  armado.
- **NVS ilegible**: no se escribe `wgh_*` si `Init()` no pudo abrirla.
- **Máquina de estados pura** (`weigh_state`) con tests de host; tolerancia de
  60 s al reanclar fechas futuras.
- **Descarte del rango rezagado** cancelado y limpiado al abrirse asistente,
  Bebés o formación.
- Documentado el hueco único de sello.

## Impact

Solo specs: `hmi-weigh-reminder` (3 requisitos modificados, 1 añadido). El
código ya está en la rama. Sin cambio de `FWversion`.
