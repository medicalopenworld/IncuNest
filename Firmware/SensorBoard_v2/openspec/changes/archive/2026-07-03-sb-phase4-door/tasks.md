# Tasks — sb-phase4-door

## 1. Red

- [x] 1.1 `door_sensor` con lógica pura stub (`sb_door_logic.h/.c`: fsm + builder) + `test_apps/door_test` con TEST_CASE por Scenario — fallan

## 2. Green

- [x] 2.1 Implementar fsm pura (estado inicial, cambio estable, supresión de repetidos, polaridad) y builder
- [x] 2.2 Driver: GPIO IO47 pull-up + ISR ANYEDGE con hand-off (`vTaskNotifyGiveFromISR`), `door_task` prio 4 con debounce Kconfig, publicación + `sensors.door`
- [x] 2.3 `main.c`: init no fatal

## 3. Verify

- [x] 3.1 Builds en verde (app + door_test); Scenarios cubiertos

## 4. Review

- [x] 4.1 code-reviewer + security-reviewer; hallazgos resueltos

## 5. Docs / cierre

- [x] 5.1 README/CHANGELOG; archivar; retro; checkbox EPIC-001

> Verificación manual on-target pendiente: generar rebotes rápidos en IO47 durante la ventana de debounce y confirmar que `door_task` vuelve a bloquear sin ciclos fantasma (el contador se vacía con `ulTaskNotifyValueClear`); medir `uxTaskGetStackHighWaterMark` de `door` (stack 3072 asumido).
