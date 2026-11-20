Un commit por fase. Todo en `Display_HMI/` salvo la documentación. Tests de
host: `pio test -e native` desde `Firmware/Display_HMI` (gcc de
`C:\msys64\mingw64\bin`, ver `pre_native.py`).

## 1. Módulo puro y tests en rojo (TDD, host)

Commit: `test(hmi): failing specs for patient session`.

- [x] 1.1 Escribir `test/test_patient_session/test_patient_session.cpp` con los
      escenarios de `specs/hmi-patient-identity/`: edad (mismo día, días
      transcurridos, sin hora, edad desconocida, gestación 0), acumulación
      (act 1/2/3, foto, tope de 5 s, enlace caído, seq 0), resincronización
      (parado, en marcha, seq ajeno), formato de minutos y validación de los
      dos registros de NVS.
- [x] 1.2 Dar de alta `patient/patient_session.cpp` en el filtro de
      `[env:native]` de `platformio.ini` y un `.h` con solo las firmas, de modo
      que la suite compile y falle por comportamiento, no por enlace.

## 2. Identidad en Main y en el bloqueo

Commit: `feat(hmi): identidad del paciente en Main y bloqueo`.

- [x] 2.1 Implementar `modules/patient/patient_session.{h,cpp}` (edad, formato,
      registros) hasta dejar en verde lo que cubre esta fase.
- [x] 2.2 `state/patient_state.{h,cpp}`: sesión capturada, restauración desde
      NVS en el arranque, borrado en el alta, escritura diferida fuera de
      `LVGL_Lock`. Claves `pat_id` (y `pat_ther`) en `EEPROM_defines.h`.
- [x] 2.3 `BabyWizard_Poll`: capturar `ageDays` + `HMI_GetEpochNow()` de
      `PROFILE_RANGE`; enganchar `ClearActiveProfile`/`SetSession`.
- [x] 2.4 Cadenas en las cuatro lenguas en `ui/i18n_strings.def`.
- [x] 2.5 Widgets: nombre/detalle dentro de `ui_BabiesButton` y réplica en
      `ui_ScreenLock`; refresco `patient_identity_update()` en `UITask.cpp` con
      el patrón de `clock_update()`.
- [x] 2.6 Pasar `fmtMinutes` de `BabyHistory.cpp` al helper compartido.

## 3. Horas de terapia acumuladas

Commit: `feat(hmi): horas de terapia acumuladas en los paneles`.

- [x] 3.1 Acumulador (`patient_accum_*`) hasta verde en `[env:native]`.
- [x] 3.2 Espejo de `CTRL,PROFILE_LIST` en el parser de `CommTask.cpp`, sin
      tocar `g_pendingProfileList`, con generación y `portMUX`.
- [x] 3.3 Siembra en el `PROFILE_ACK` desde `s_list`; resincronización y
      acumulación a 1 Hz y volcado a NVS cada 60 s, en `UI_Task`, fuera de
      `LVGL_Lock`.
- [x] 3.4 Etiqueta «Total / Xh Ym» en la cabecera de los tres paneles y su
      refresco.

## 4. Verificación

- [x] 4.1 `pio test -e native` en verde (`Display_HMI`).
- [x] 4.2 `pio run -e main` en verde (`Display_HMI`); anotar RAM y Flash.
- [ ] 4.3 **Manual, en banco** (el HMI se flashea con la motherBoard sin
      corriente):
  - [ ] Heading de Main con sesión viva: nombre, `EGC 34+5 - 5d`, sin solapes; sin
        sesión, «Bebés».
  - [ ] Nombre de 23 caracteres: se desplaza y se lee entero.
  - [ ] Bloqueo: réplica sin truncar, no estorba al desbloqueo.
  - [ ] Cabeceras de los tres paneles con las cuatro lenguas y `12h 20m`.
  - [ ] Reiniciar el HMI con terapia en marcha: vuelven nombre y minutos.
  - [ ] Dar de alta: desaparece todo y el reinicio posterior no lo resucita.
  - [ ] Abrir el asistente y el visor de bebés mientras tanto: siguen
        recibiendo su lista (sin competencia por el flag).
  - [ ] Cambiar de idioma con la sesión viva.

## 5. Documentación

Commit: `docs(hmi): update for patient identity`.

- [x] 5.1 `Firmware/docs/hmi.md`: heading, paneles y claves NVS.
- [x] 5.2 `Firmware/docs/known_issues.md` si se cierra o descubre algo.
