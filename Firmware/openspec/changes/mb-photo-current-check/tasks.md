Cada fase es un commit atómico y no se mezclan dos placas en uno, salvo que el
cambio esté en `shared/` (`.claude/rules/commits.md`). Tests de host:
`pio test -e native` en `Firmware/motherBoard`.

## 1. shared: la condición nueva

- [x] 1.1 Añadir `ALARM_PHOTO_LAMP_FAULT = 20` **al final** del enum, antes de `ALARM_COUNT` (`shared/include/alarm_ids.h`)
- [x] 1.2 Prioridad BAJA en `alarm_priority()`, fuera de `alarm_cuts_heater()` y de `alarm_is_latching()` (`shared/src/alarm_policy.cpp`)
- [x] 1.3 Título y acción en EN/ES/FR/PT dentro de `ALARM_TITLE_MAX_CHARS` / `ALARM_DESC_MAX_CHARS` (`shared/src/alarm_text.cpp`)
- [x] 1.4 Actualizar `test_alarm_policy` (reparto 4 BAJA / 9 MEDIA / 7 ALTA) y comprobar `test_alarm_text`

## 2. motherBoard: módulo puro (TDD, host)

- [x] 2.1 Escribir en rojo `test/test_photo_fault/test_photo_fault.cpp` con los escenarios de `specs/phototherapy-lamp-monitor/`
- [x] 2.2 Dar de alta `modules/control/photo_fault.cpp` en `build_src_filter` de `[env:native]` (`platformio.ini`)
- [x] 2.3 Implementar `modules/control/photo_fault.{h,cpp}` (C-linkage, sin Arduino). Verde en `pio test -e native`

## 3. motherBoard: cableado

- [x] 3.1 `send_state_to_hmi()`: campo 8 = `in3.phototherapy` (`tasks/CommTask.cpp`), con el comentario del porqué
- [x] 3.2 Cablear `photo_fault_update()` a `alarm_machine_condition()` en `sensors_module.cpp`, respetando `g_factoryTestActive` y `PHOTO_SETTLE_MS`
- [x] 3.3 Clave de telemetría de la alarma (`telemetry_keys.h`) y su `case` en `GPRS.cpp` y `Wifi_OTA.cpp`
- [x] 3.4 Comprobar que `PHOTOTHERAPY_ACTIVE_KEY` publica `in3.phototherapy` (ya lo hace)

## 4. Verificación

- [x] 4.1 `pio test -e native` en verde
- [x] 4.2 `pio run -e IncuNest_V18` en `motherBoard` en verde, con tamaños
- [x] 4.3 `pio run` en `Display_HMI` en verde (consume `shared/`)
- [ ] 4.4 **Manual (banco)**: `PHOTO,1` con modo depuración — la pantalla muestra fototerapia ON y, al soltar, vuelve a OFF con la lámpara apagada
- [ ] 4.5 **Manual (banco)**: encender la fototerapia desde la pantalla — el switch no parpadea a OFF al encender (guarda del display)
- [ ] 4.6 **Manual (banco)**: con la lámpara desconectada y la fototerapia ON, la alarma BAJA aparece ~8 s después y se retira al reconectar o apagar
- [ ] 4.7 **Manual (banco)**: el pop-up de la alarma nueva se ve bien en las cuatro lenguas

## 5. Documentación

- [ ] 5.1 `Firmware/PROTOCOL.md`: campo 8 de `CTRL,STATE` refleja la lámpara (nota fechada 2026-10-25)
- [ ] 5.2 `Firmware/docs/known_issues.md` §20 resuelto
- [ ] 5.3 `Firmware/docs/alarms.md` §1 (fila 20, «Las 20 condiciones»), §2 (reparto) y §9 si procede
