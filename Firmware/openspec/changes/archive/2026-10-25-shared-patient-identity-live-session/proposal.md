## Why

Afecta solo a **Display_HMI** (la placa y `shared/` ya emiten y documentan
`activeSeq` desde `shared-patient-identity-confirm`). Es el seguimiento de la
segunda revisión de código de `feat/hmi-patient-identity`, que dio por bueno
que ninguna ruta **pinte** como confirmado a un bebé ajeno, pero encontró
**fugas hacia la placa**: rutas que actúan sobre el `seq` restaurado sin que la
placa lo haya confirmado.

- `BabyExitDialog` tomaba `BabyWizard_GetActiveSeq()` sin confirmar y mandaba
  `PROFILE_DISCHARGE`: archivaría a **otro bebé**.
- `BabyHistory::afterRegistered` reenviaba `PROFILE_SELECT` con ese seq.
- Descartar la identidad llevaba el seq a 0 y `Maintenance_Tick` lo leía como
  un alta: limpieza terminal pendiente **y persistida**, falsa.
- La petición propia de `PROFILE_LIST` no se reintentaba nunca si la respuesta se
  perdía; un descarte con el asistente abierto le borraba el bebé en curso; tras
  `babyStore_wipeAll()` un seq reutilizado pasaba por confirmado; y un `sn` de 0
  se trataba como «otra placa» en vez de como no verificable.

Este change deja en el spec lo ya implementado (commits `3de272fe`..`f185f5ec`).

## What Changes

- **`BabyWizard_HasLiveSession()` exige identidad confirmada.** Cierra de una vez
  todas las rutas que actúan sobre el seq hacia la placa. Quien necesita «hay un
  paciente dentro, sea quien sea» (recordatorio diario, pintado gris) usa el
  nuevo `BabyWizard_HasAnySession()`. `BabyExitDialog_Tick` lleva además su guarda
  directa.
- **Descartar una identidad sin confirmar no es un alta**:
  `Maintenance_NoteSeqDropped()` y `mnt_seq_decide()` (puro, con test).
- **La petición propia de lista caduca** a los 10 s y se reintenta (3 intentos).
- **El descarte se aplaza con el asistente abierto.**
- **Contraste de lista tras confirmar por `activeSeq`** (un seq se reutiliza tras
  `wipeAll`).
- **`sn == 0` no es verificable**: no es «otra placa», pero `activeSeq` solo no
  confirma; exige el contraste de lista.
- Con el enlace caído no se decide nada (el último `CTRL,STATE` está obsoleto).

## Capabilities

### New Capabilities
Ninguna.

### Modified Capabilities
- `hmi-patient-identity`: confirmación con la placa y visibilidad de la identidad;
  requisitos nuevos para sesión viva, descarte, reintento y aplazamiento.

## Impact

- `Display_HMI`: `modules/patient/patient_session.{h,cpp}`,
  `modules/maintenance/maintenance{,_seq}.{h,cpp}`, `state/patient_state.cpp`,
  `ui/BabyWizard.cpp`, `ui/BabyExitDialog.cpp`, `tasks/UITask.cpp`,
  `include/ui/BabyWizard.h`, `include/tasks/CommTask.h`.
- Docs: `docs/hmi.md` §9, `docs/known_issues.md` #23 y #24.
- **Cambio de semántica** para otras ramas abiertas que llamen a
  `BabyWizard_HasLiveSession()` (recordatorio de pesada): hasta que la placa
  confirme verán «sin sesión viva».
