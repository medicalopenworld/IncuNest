Un commit por fase. Todo en `Display_HMI/` salvo documentación. Tests de host
con `pio test -e native` en `Firmware/Display_HMI` (gcc de MSYS2 en el PATH).

## 1. Display_HMI: correcciones previas

- [x] 1.1 Añadir `BabyHistory_IsOpen()` a la lista de diálogos ante los que espera el aviso (`MaintenanceDialog_Poll`). Commit `fix(hmi)`.
- [x] 1.2 Corregir `docs/hmi.md` §8: el tope se mide con `lv_disp_get_inactive_time()`, no con `lv_tick_elaps()`. Commit `docs(hmi)`.

## 2. Display_HMI: decisión pura (TDD)

- [x] 2.1 Rojo: `test/test_weigh_reminder/` con los escenarios del spec, alta en el filtro de `[env:native]` y `weigh_reminder.h` con stubs. Commit `test(hmi)`.
- [x] 2.2 Verde: `weigh_reminder.cpp`. `pio test -e native` en verde.

## 3. Display_HMI: estado, sello y UI

- [x] 3.1 Claves `wgh_*` en `EEPROM_defines.h` y `weigh_tracker.{h,cpp}` (estado, `Tick`, `NoteWeighed`, `Snooze`, `Flush`).
- [x] 3.2 Sello desde `Communication_SendProfileWeight()` y flush de NVS fuera de `LVGL_Lock` en `UITask.cpp`.
- [x] 3.3 Cadenas nuevas en las cuatro lenguas.
- [x] 3.4 `MaintenanceDialog`: modos `WeighPrompt` y `WeighEntry`, armado independiente, cesión.

## 4. Verificación

- [x] 4.1 `pio test -e native` en verde (63 casos, 20 de ellos de `test_weigh_reminder`).
- [x] 4.2 `pio run -e main` en verde: RAM 39.5 % (129468 B de 327680), Flash 51.6 % (2704824 B de 5242880).
- [ ] 4.3 **Manual, PENDIENTE en banco (sin entorno de test)**: aviso a las 24 h, snooze, PESAR AHORA registra y calla, SKIP no sella, alta borra, sin hora no avisa, cesión ante alarma/pantalla Bebés/otros diálogos, 4 idiomas (texto sin cortar), formación no escribe NVS, mantenimiento y pesada vencidos a la vez, claves `wgh_*` tras reinicio.

## 5. Documentación

- [x] 5.1 `docs/hmi.md`: sección de avisos (pesada) y claves NVS. Commit `docs(hmi)`.
