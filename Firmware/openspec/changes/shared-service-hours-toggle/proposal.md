## Why

Placa afectada: **Display_HMI**.

El aviso de servicio por horas de uso (`shared-service-hours`) se abría solo si
el interruptor AVISOS de la limpieza estaba activado, y ese interruptor viene
**apagado de fábrica** (tanda de fabricación del 2026-09-17). Resultado: una
unidad nueva nunca avisaría de un componente al final de su vida útil, que es
justo lo que el aviso de servicio quiere evitar. La decisión es que el servicio
no dependa de ese interruptor.

## What Changes

- Interruptor propio `svc_en` (NVS `hmi_cfg`) para el aviso de servicio, con
  **valor por defecto ON**, también en una NVS virgen.
- Vive en Ajustes > Mantenimiento, bajo el interruptor AVISOS de la limpieza,
  con textos en las 4 lenguas. En modo formación se rechaza con el toast
  estándar y no escribe NVS.
- `Maintenance_IsEnabled()` deja de intervenir en el aviso de servicio. La vista
  SERVICIO sigue siendo accesible con el interruptor apagado: solo se silencia
  la apertura automática.

## Impact

- `Display_HMI/src/modules/service/service_state.{h,cpp}` (flag
  `enabled`, persistencia en `service_settings.{h,cpp}`), `config/EEPROM_defines.h`,
  `ui/ElementsCreation.cpp`, `tasks/UITask.cpp`, `ui/MaintenanceDialog.cpp`,
  `i18n_strings.def`, `docs/hmi.md`.
- Posiciones verticales del panel documentadas en `docs/hmi.md` por el posible
  conflicto con `feat/hmi-weigh-reminder`.
