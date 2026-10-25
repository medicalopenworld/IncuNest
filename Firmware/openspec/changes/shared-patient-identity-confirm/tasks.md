Un commit por fase y por placa. Rutas siempre dentro del worktree
`hmi-patient-identity`. Tests: `pio test -e native` en `motherBoard` y
`Display_HMI`; builds: `pio run -e IncuNest_V18` y `pio run -e main`.

## 1. shared: el campo `activeSeq`

Commit: `feat(shared): activeSeq en CTRL,STATE`.

- [ ] 1.1 `PROTOCOL.md`: campo 24 `activeSeq` en la tabla de `CTRL,STATE`, con su
      compatibilidad (placa antigua = desconocido, no 0; HMI antiguo ignora el
      sobrante).
- [ ] 1.2 `shared/include/protocol.h`: `activeSeq` en `Proto_CtrlState`.

## 2. motherBoard

Commit: `feat(motherboard): activeSeq en CTRL,STATE`.

- [ ] 2.1 Emitir `babyStore_getActiveSeq()` como campo 24 en
      `send_state_to_hmi()` y subir el búfer de 192 a 224 explicando el peor
      caso.
- [ ] 2.2 `pio test -e native` y `pio run -e IncuNest_V18` en verde.
- [ ] 2.3 **Manual:** verificar en banco el campo 24 en el log de la placa.

## 3. Display_HMI: lógica pura (TDD, host)

Commits: `test(hmi): failing specs for patient identity confirmation`, luego
`fix(hmi): confirmar la identidad restaurada con la placa`.

- [ ] 3.1 Tests en rojo: tabla de confirmación, veredicto de lista, mismo bebé,
      acreditación, `known`, edad (sin hora, > 365 días, hora ahora), días
      acotados, registro v2 con `sn` y rango de semanas, contador en marcha con
      listas antiguas.
- [ ] 3.2 Implementar en `modules/patient/patient_session.{h,cpp}` hasta verde.

## 4. Display_HMI: integración

Commit: `fix(hmi): la identidad restaurada no se pinta sin confirmar`.

- [ ] 4.1 `ctrl_state_msg`: `activeSeq`, `activeSeqKnown`, contador de estados;
      parseo del campo 24.
- [ ] 4.2 `PatientState`: `confirmed`, contraste de lista para placa antigua con
      una sola petición, `sn` en el registro (v2), acreditación condicionada,
      minutos `known`, mismo-seq-otro-nombre.
- [ ] 4.3 `BabyWizard`: sin atajo `selectExisting` con identidad sin confirmar;
      `OnClear` solo desde `WaitingSelectAck`.
- [ ] 4.4 UI: estado oculto / confirmada / gris con «?».

## 5. Display_HMI: layout

Commit: `fix(hmi): terapia acumulada bajo el titulo y detalle acotado`.

- [ ] 5.1 Etiqueta de una línea bajo el título en las tres cabeceras; quitar
      `STR_THERAPY_TOTAL`.
- [ ] 5.2 Detalle del botón «Bebés» con `LV_LABEL_LONG_DOT` y días `99+`.

## 6. Verificación y docs

Commit: `docs(hmi): ...`.

- [ ] 6.1 `pio test -e native` (MB y HMI) y los dos builds en verde.
- [ ] 6.2 `docs/hmi.md` y `known_issues.md` #23 alineados.
- [ ] 6.3 **Manual, en banco:** reiniciar el HMI con terapia en marcha (se
      confirma y vuelve); cambiar el HMI de placa (no aparece el bebé ajeno);
      HMI con placa antigua (gris con «?»); `100h 5m` en francés sin pisar el
      título; nombre largo en «Bebés».
