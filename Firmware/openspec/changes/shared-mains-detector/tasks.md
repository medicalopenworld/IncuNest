## 1. motherBoard — detector (lógica pura, TDD en `[env:native]`)

- [ ] 1.1 Tests Unity en rojo `motherBoard/test/test_mains_detector/` y alta de `modules/control/mains_detector.cpp` en `build_src_filter` de `[env:native]` (commit `test(motherboard): failing specs for mains detector`)
- [ ] 1.2 `modules/control/mains_detector.{h,cpp}`: antirrebote de 2 lecturas, plazo mínimo de 10 min desde la declaración, lectura no válida sin efecto, `vbat_mv == 0` no válida
- [ ] 1.3 `pio test -e native` en verde en motherBoard

## 2. motherBoard — cableado (sin entorno de test)

- [ ] 2.1 `mains_present_now()` y muestreo tras `charge_status()` en `sensors_Task` (`src/main.cpp`)
- [ ] 2.2 `checkMainsInterruption()` en `securityCheck()` (`src/system/security.cpp`): `alarm_machine_condition(ALARM_MAINS_INTERRUPTION, ...)`, sin declarar con `g_factoryTestActive`; sin cargador, nunca y un log
- [ ] 2.3 Confirmar que `ALARM_MAINS_INTERRUPTION_KEY` ya se publica en `GPRS.cpp` y `Wifi_OTA.cpp` (sin cambio de código)
- [ ] 2.4 `pio run -e IncuNest_V18` en verde

## 3. Display_HMI — indicador (sin entorno de test)

- [ ] 3.1 Cadena `STR_ON_BATTERY` en 4 lenguas (`include/ui/i18n_strings.def`)
- [ ] 3.2 Widgets en el heading de Main y en la réplica de Lock (`ElementsCreation.cpp/.h`)
- [ ] 3.3 `on_battery_heading_update()` junto a `clock_update()` (`UITask.cpp`), sin E/S dentro de `LVGL_Lock`
- [ ] 3.4 `pio run -e main` en verde en Display_HMI

## 4. Verificación manual en banco (no hay entorno de test; no se afirma cobertura)

- [ ] 4.1 **Manual**: desenchufar la red con terapia activa (aire + fototerapia) y comprobar que el aviso sale en ≈ 10 s, con zumbador y pop-up, y que la terapia sigue
- [ ] 4.2 **Manual**: cortes breves (< 5 s, enchufar/desenchufar rápido) no deben levantar el aviso
- [ ] 4.3 **Manual**: red de vuelta antes de 10 min: el aviso se mantiene hasta cumplirse 10 min desde su declaración
- [ ] 4.4 **Manual**: red de vuelta después de 10 min: el aviso se retira a los ≈ 10 s del retorno
- [ ] 4.5 **Manual**: el indicador «en batería» aparece y desaparece en Main y en Lock, en ES/EN/FR/PT, y no desplaza otros elementos
- [ ] 4.6 **Manual (hardware)**: la reserva de energía sostiene zumbador y micro 10 min sin red, y estos cuelgan de VSYS
- [ ] 4.7 **Manual**: placa sin cargador: nunca alarma y una línea de log

## 5. Documentación

- [ ] 5.1 `docs/alarms.md` §1 (fila 7) y §9
- [ ] 5.2 `docs/alarms_bench_verification.md`: casos de banco
