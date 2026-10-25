Un commit por fase. Todo en `Display_HMI/` salvo documentación. Tests de host
con `pio test -e native` en `Firmware/Display_HMI` (gcc de MSYS2 en el PATH).

## 1. Display_HMI: correcciones previas

- [ ] 1.1 Añadir `BabyHistory_IsOpen()` a la lista de diálogos ante los que espera el aviso (`MaintenanceDialog_Poll`). Commit `fix(hmi)`.
- [ ] 1.2 Corregir `docs/hmi.md` §8: el tope se mide con `lv_disp_get_inactive_time()`, no con `lv_tick_elaps()`. Commit `docs(hmi)`.

## 2. Display_HMI: decisión pura (TDD)

- [ ] 2.1 Rojo: `test/test_weigh_reminder/` con los escenarios del spec, alta en el filtro de `[env:native]` y `weigh_reminder.h` con stubs. Commit `test(hmi)`.
- [ ] 2.2 Verde: `weigh_reminder.cpp`. `pio test -e native` en verde.

## 3. Display_HMI: estado, sello y UI

- [ ] 3.1 Claves `wgh_*` en `EEPROM_defines.h` y `weigh_tracker.{h,cpp}` (estado, `Tick`, `NoteWeighed`, `Snooze`, `Flush`).
- [ ] 3.2 Sello desde `Communication_SendProfileWeight()` y flush de NVS fuera de `LVGL_Lock` en `UITask.cpp`.
- [ ] 3.3 Cadenas nuevas en las cuatro lenguas.
- [ ] 3.4 `MaintenanceDialog`: modos `WeighPrompt` y `WeighEntry`, armado independiente, cesión.

## 4. Verificación

- [ ] 4.1 `pio test -e native` en verde.
- [ ] 4.2 `pio run -e main` en verde; anotar RAM/Flash.
- [ ] 4.3 **Manual (banco, sin entorno de test)**: checklist del spec — aviso a las 24 h, snooze, PESAR AHORA registra y calla, SKIP no sella, alta borra, sin hora no avisa, cesión ante alarma/BabyHistory/otros diálogos, 4 idiomas, formación no escribe NVS.

## 5. Documentación

- [ ] 5.1 `docs/hmi.md`: sección de avisos (pesada) y claves NVS. Commit `docs(hmi)`.
