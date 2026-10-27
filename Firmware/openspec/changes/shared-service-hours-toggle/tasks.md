## 1. Tests en rojo
- [ ] 1.1 `test_service_state`: por defecto activado, apagado no avisa, reactivar avisa, `Reset` vuelve a activado.

## 2. Implementación
- [ ] 2.1 Flag `enabled` en `service_state` y persistencia `svc_en` (default ON).
- [ ] 2.2 Interruptor en Ajustes > Mantenimiento, callback con rechazo en formación, textos 4 lenguas.
- [ ] 2.3 `MaintenanceDialog_Poll()` sin `Maintenance_IsEnabled()`.

## 3. Verificación y docs
- [ ] 3.1 `pio test -e native` y `pio run -e main` en Display_HMI.
- [ ] 3.2 `docs/hmi.md` (interruptor y coordenadas del panel).
