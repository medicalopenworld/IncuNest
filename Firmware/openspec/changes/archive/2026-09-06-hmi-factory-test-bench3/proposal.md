## Why

Afecta a **Display_HMI** y **motherBoard**. Cuarta ronda de banco del test de
hardware (2026-09-06, placa solo con batería):

- **El HMI se reinició** con `Interrupt wdt timeout on CPU1` tras varios
  "anillo FTEST lleno". Causa: el log de descarte se emitía **dentro** de la
  sección crítica del anillo de resultados (`taskENTER_CRITICAL`); escribir por
  serie con las interrupciones deshabilitadas dispara el Interrupt WDT. El
  anillo de 8 se desborda porque la motherBoard emite ráfagas de RUNNING+SKIP
  casi instantáneas al omitir varios tests seguidos.
- **Cabecera incoherente**: dos botones en rojo, "0 errores" y `HW ERROR`. La
  cabecera usaba los contadores de `FTEST_DONE` (solo si había llegado) y el
  veredicto miraba las filas.
- El test de **zumbador** seguía preguntando al operario porque el snapshot
  no traía `sound_seen`.
- `power_src` y `charger` no se pueden verificar a batería: el BQ25730 no está
  alimentado sin VBUS. `sb_door` no se puede verificar sin la puerta montada.
- El aviso de "equipo vacío" debe ser un pop-up, no un texto en la zona de
  acción.

## What Changes

- **Display_HMI**: ninguna llamada de log dentro de secciones críticas; anillo
  de 32 con contador de descartes; cabecera y persistencia calculadas siempre
  desde las filas; el aviso de entrada pasa a pop-up modal; el drenado del
  anillo no depende del estado de la UI.
- **motherBoard**: `buzzer` sin CONFIRM (SKIP `sin microfono` si no hay
  micrófono); `power_src` y `sb_door` omitidos por ahora (SKIP `omitido`);
  `charger` da WARN `sin vbus` si el BQ25730 no responde; pausa de 60 ms entre
  tests para no desbordar al display.

**Pendiente para el futuro** (anotado en `docs/hardware.md`): reactivar
`power_src` y verificar `charger` cuando el jig alimente por VBUS; reactivar
`sb_door` cuando el jig monte la puerta; `humid_usb` y el audio del display
siguen omitidos.

## Capabilities

### New Capabilities
<!-- Ninguna. -->

### Modified Capabilities
- `hmi-factory-test`: anillo, cabecera desde filas, pop-up de entrada.
- `mb-factory-test`: buzzer sin CONFIRM, power_src y sb_door omitidos,
  charger WARN sin VBUS, pausa entre tests.

## Impact

- `Display_HMI/src/tasks/CommTask.cpp`, `include/tasks/CommTask.h`,
  `src/ui/FactoryTest.cpp`.
- `motherBoard/src/modules/factory_test/factory_test_hw.cpp`,
  `factory_test_task.cpp`, `src/tasks/CommTask.cpp`.
- `PROTOCOL.md`, `docs/hmi.md`, `docs/hardware.md`.
