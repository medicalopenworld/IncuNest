## Context

`shared-patient-identity-confirm` hizo que una identidad restaurada de la NVS
naciese sin confirmar y que la placa la confirmase con `activeSeq`. La revisión
siguiente encontró que `BabyWizard_HasLiveSession()` (seq conocido y terapia en
marcha) seguía siendo verdadero para una identidad sin confirmar, y que de él
cuelgan acciones hacia la placa.

## Decisions

### D1. Un solo punto de cierre: `HasLiveSession`

Exigir la confirmación **dentro** de `HasLiveSession()` (`patient_live_session()`,
pura) cierra todas las rutas a la vez, en lugar de parchear una a una:
`BabyExitDialog` (alta), `BabyHistory::afterRegistered` (`PROFILE_SELECT`), el
atajo `selectExisting`, y el gating de humedad y fototerapia en `UITask`. El
coste es un cambio de semántica para cualquier otra llamada, incluidas ramas
abiertas; se documenta en `BabyWizard.h` y `hmi.md`.

`BabyWizard_HasAnySession()` es la excepción explícita, solo para lo que no
depende de **quién** es el bebé y nunca actúa sobre el seq hacia la placa:
el recordatorio de limpieza diaria (hay un paciente dentro) y el pintado gris de
una identidad restaurada que figura en la lista de una placa antigua.

`BabyExitDialog_Tick` añade una guarda directa (`PatientState_IsConfirmed`):
defensa en profundidad para la ruta más grave (archivar un perfil ajeno).

### D2. Descartar no es dar de alta

Descartar la identidad lleva `BabyWizard_GetActiveSeq()` a 0, y `Maintenance_Tick`
compara con el último seq visto: sin más, es un alta y deja una limpieza terminal
pendiente y persistida. `patient_state` anota el seq descartado con
`Maintenance_NoteSeqDropped()` **solo si la identidad nunca se confirmó**, y
`mnt_seq_decide()` excusa exactamente la transición `seq -> 0` de ese mismo seq,
una sola vez (la nota se consume en cada `Tick`, para no excusar una alta real
posterior). Un descarte tras haber confirmado (el contraste de lista descubre un
seq reutilizado) sí cuenta: la placa llegó a creer en ese bebé.

### D3. La petición de lista caduca

`s_listOutstanding` solo bajaba al llegar una lista: si la respuesta se perdía, no
había más intentos. `patient_list_request_due()` (pura, aritmética modular) da la
petición por caducada a `LIST_RETRY_MS` (10 s) y permite reintentar hasta
`LIST_MAX_ATTEMPTS` (3). Sigue sin haber peticionario periódico: el flag
`g_pendingProfileList` tiene un único consumidor.

### D4. Aplazar el descarte con el asistente abierto

El bebé en curso del asistente vive en `s_seq`/`s_name`;
`BabyWizard_ClearActiveProfile()` los pone a cero a mitad de un alta. En vez de
limpiar solo los `s_session*` (que obligaría a partir esa función), se aplaza el
descarte hasta que el asistente se cierre; si el propio asistente confirma o
sustituye la identidad mientras tanto, el descarte pendiente se anula.

### D5. Contraste de lista tras confirmar

Un seq se reutiliza: `babyStore_wipeAll()` reinicia la cuenta en 1, así que un
`seq 3` restaurado puede ser otro bebé de la misma placa con `activeSeq == 3`.
Tras confirmar por `activeSeq`, la siguiente `PROFILE_LIST` (la propia o la de
cualquiera) debe traer el seq con el mismo nombre y semanas, o se descarta. Hasta
que llega, el bebé se muestra confirmado. Una identidad nacida de un
`PROFILE_ACK` no lo necesita: la placa acaba de responder por ella.

### D6. `sn == 0` no es verificable

Sin número de serie (en el registro o en la placa) no se puede decir que sea
«otra placa» ni que sea la misma. `patient_confirm_decide` solo borra por `sn`
cuando **ambos** son distintos de 0; y `patient_confirm_requires_list()` obliga al
contraste de lista antes de confirmar, quedando el bebé oculto mientras tanto.

### D7. Sin decidir con el enlace caído

El último `CTRL,STATE` no dice nada de quién está al mando ahora; decidir con él
podría borrar un bebé válido. `s_legacyBoard` se relee en cada pasada de la
instantánea de la placa.

## Risks

- Identidad restaurada con el mismo nombre y semanas que otro bebé tras un
  `wipeAll`: indistinguible (`known_issues.md` #24).
- `activeSeq` es el último campo y el protocolo no lleva checksum: un `%u` de una
  línea truncada puede dar un prefijo. No se exigen dos mensajes seguidos
  (`known_issues.md` #24).
- Un bebé que se fue por otra vía mientras el HMI estaba apagado ya no deja
  recordatorio de limpieza terminal.
