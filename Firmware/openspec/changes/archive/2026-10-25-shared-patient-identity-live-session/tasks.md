Documenta lo ya implementado en `3de272fe`..`f185f5ec`. Un commit por fase.

## 1. Spec

Commits: `docs(spec): propose shared-patient-identity-live-session` y
`chore(spec): archive shared-patient-identity-live-session`.

- [x] 1.1 Deltas para sesión viva frente a `HasAnySession`, descarte sin falsa
      alta, reintento de lista que caduca, descarte aplazado con el asistente
      abierto, contraste de lista tras `activeSeq` y `sn == 0`.
- [x] 1.2 `openspec validate shared-patient-identity-live-session --strict`.

## 2. Implementación (ya hecha, en la rama)

- [x] 2.1 `patient_live_session`, `patient_confirm_requires_list`,
      `patient_list_request_due`, `mnt_seq_decide` y el ajuste de `sn` 0 en
      `patient_confirm_decide`, con tests en rojo primero.
- [x] 2.2 `HasLiveSession` / `HasAnySession`, guarda en `BabyExitDialog_Tick`,
      `Maintenance_NoteSeqDropped`, descarte aplazado, contraste de lista.
- [x] 2.3 `pio test -e native` (135 casos) y `pio run -e main` en verde.

## 3. Verificación manual en banco

- [ ] 3.1 Reiniciar el HMI con terapia en marcha y detenerla antes de que la
      placa confirme: no sale el diálogo de alta.
- [ ] 3.2 Descartar una identidad ajena: no queda `mnt_tpend`.
- [ ] 3.3 Perder la respuesta de `PROFILE_LIST`: se reintenta a los 10 s.
- [ ] 3.4 Contradecir la identidad con el asistente abierto: el alta en curso no
      se interrumpe.
- [ ] 3.5 `wipeAll` en la placa y reiniciar el HMI: se descarta al llegar la lista.
- [ ] 3.6 Placa sin número de serie: no se confirma sin la lista.
