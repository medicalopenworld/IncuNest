Todo el cambio es de **Display_HMI**. Sin entorno de test: cada fase se
verifica con `pio run -e main` y prueba **manual** en el CrowPanel con la
motherBoard conectada. Ningún checkbox reclama cobertura automatizada.

## 1. Teclado compartido

Commit: `refactor(hmi): teclados del asistente del bebe en ui/InputKeypad`.

- [x] 1.1 `include/ui/InputKeypad.h` + `src/ui/InputKeypad.cpp`: mapas de
      letras (sin coma) y dígitos, `InputKeypad_Create()`,
      `InputKeypad_StripCommasCb()`, `InputKeypad_ReadNumber()`, con los
      comentarios de `lv_btnmatrix` vs `lv_keyboard` y de la coma.
- [x] 1.2 `BabyWizard.cpp` usa el módulo; sin cambio de comportamiento
      (mismos mapas, misma validación, mismos toasts).
- [x] 1.3 `pio run -e main` en verde.

## 2. Registro desde Bebes

Commit: `feat(hmi): registrar un bebe nuevo desde la pantalla Bebes`.

- [x] 2.1 `i18n_strings.def`: `STR_REGISTER_UC`, `STR_BABY_REGISTERED`,
      `STR_REGISTER_REFUSED`, `STR_THREE_ACTIVE_DISCHARGE_FIRST`,
      `STR_ADMISSION_WEIGHT_G` en es/en/fr/pt, ASCII 32-126.
- [x] 2.2 `BabyHistory.cpp`: botón BEBE NUEVO bajo Activos (guarda de 3
      activos con toast); pasos `NewName` → `NewGest` → `NewWeight` con la
      tarjeta grande y `InputKeypad`; REGISTRAR envía `PROFILE_NEW` y, con
      peso, `PROFILE_WEIGHT` tras el ACK; `WaitingNewAck` / `WaitingNewRange`
      en `BabyHistory_Poll()` con timeout de 3 s sin reintento del NEW;
      `seq == 0` → toast de rechazo; éxito → toast "Bebe registrado" y
      recarga de la lista; X cancela sin enviar.
- [x] 2.3 `BabyHistory.h`: `BabyHistoryStep`, `BabyHistory_GetStep()`,
      `BabyHistory_LastRegisteredSeq()`.
- [x] 2.4 `pio run -e main` en verde; delta de Flash/RAM anotado contra el
      build de referencia del worktree limpio: dev `2a5fd92` RAM 124 884 B /
      Flash 2 658 308 B → feature RAM 124 956 B / Flash 2 665 136 B
      (+72 B / +6 828 B); tras la review RAM 124 996 B / Flash 2 665 888 B
      (+112 B / +7 580 B sobre dev).

## 3. Formación: bebé de prácticas registrado y lección 1

Commit: `feat(hmi): la leccion de registro ensena a dar de alta desde Bebes`.

- [x] 3.1 `training_mode.{h,cpp}`: `TRAINING_NEW_BABY_SEQ`,
      `Training_IsPracticeSeq()`, bebé de prácticas con nombre/semanas/peso
      que `Training_SimProfileNew` rellena, `SIM_LIST` con ZOE + él,
      `Training_SimProfileSelect` contesta el `seq` pedido si es de prácticas
      y carga sus datos para el rango; se borra en `Training_Enter()`.
- [x] 3.2 `training_engine.cpp`: borrado del perfil recordado con
      `Training_IsPracticeSeq()`; `mainDialog` incluye las pantallas de
      teclado de Bebes (`BabyHistory_GetStep()` en `BH_NEW_*`), no la lista.
- [x] 3.3 `lessons_nurse.cpp`: lección 1 reescrita (design decisión 7) en los
      cuatro idiomas, `LESSON_TABLE_IS_ASCII`; `goalTrainingBabyAdmitted`
      acepta cualquier `seq` de prácticas. `lessons_intro.cpp`: el texto de
      Bebes menciona BEBE NUEVO.
- [x] 3.4 `maintenance.cpp`: `Maintenance_Tick()` sí corre en formación
      (`MaintenanceDialog_Poll()` lo llama antes de su propio
      `Training_IsActive()`); el seguimiento del `seq` al mando se salta en
      formación (con ZOE ya escribía `HMI_KEY_MNT_SEQ` / `_TPEND` en NVS).
- [x] 3.5 `pio run -e main` en verde.

## 3b. Review (code-reviewer + security-reviewer, 2026-09-10)

Commit: `fix(hmi): review feedback del registro desde Bebes`.

- [x] R1 ACK huérfano: `discardPendingReplies()` al abandonar cualquier
      espera de Bebes (timeout, alarma crítica, cierre) y descarte justo
      antes de cada envío, también en el asistente (`selectExisting`,
      `PROFILE_NEW` de las semanas) y en el alta.
- [x] R2 Guarda positiva: BEBE NUEVO solo con lista venida de la placa
      (`s_activeFromBoard`) y enlace vivo; REGISTRAR sin enlace no envía.
- [x] R3 `s_wizardSeq`: tras un registro aceptado con bebé bajo terapia,
      `afterRegistered()` reenvía `PROFILE_SELECT` del bebé en terapia
      (`WaitingReselectAck`).
- [x] R4 Perfil recordado: `BabyWizard_GetSession` / `_SetSession`; el motor
      lo restaura al salir en vez de borrarlo.
- [x] R5 Estanqueidad: `Training_SimWeightHistoryReq` para los seq de
      prácticas; `CommTask` corta `PROFILE_SELECT/WEIGHT/AGE_MANUAL` con seq
      de prácticas fuera de formación.
- [x] R6 `InputKeypad_ReadName`: nombre sin espacios sobrantes y nunca vacío,
      en Bebes y en el asistente.
- [x] R7 `mainDialog` solo en las pantallas de teclado de Bebes
      (`BH_NEW_*`); `#include <cstring>` huérfano fuera de `BabyWizard.cpp`;
      `design.md` al día.
- [x] R8 `pio run -e main` en verde tras los arreglos.

## 4. Documentación

Commit: `docs: update for hmi-bebes-alta-desde-lista`.

- [x] 4.1 `docs/hmi.md` (formación: ZOE + bebé de prácticas registrado;
      lección 1), `docs/communication.md` §A.2 (Bebes registra).
- [x] 4.2 `docs/adr/0002-...md`: revisión 2026-09-10 (registro desde Bebes
      en formación es virtual; el asistente sigue rechazando BEBE NUEVO).
- [x] 4.3 `PROTOCOL.md`: la nota de `PROFILE_WEIGHT` sobre persistencia pasa
      a `PROFILE_NEW` (es donde `babyStore_createProfile` persiste).

## 5. Verificación manual (banco)

- [ ] 5.1 **Manual** — Bebes → BEBE NUEVO → nombre, semanas, peso →
      REGISTRAR: monitor serie de la placa muestra `HMI,PROFILE_NEW` y
      `HMI,PROFILE_WEIGHT`; el bebé aparece en Activos con su peso; su
      tarjeta abre una curva con un punto.
- [ ] 5.2 **Manual** — misma secuencia con SIN PESO: solo `HMI,PROFILE_NEW`;
      el bebé aparece con `--`.
- [ ] 5.3 **Manual** — X en la pantalla de semanas: ninguna trama `PROFILE_*`
      en el monitor; la lista no cambia.
- [ ] 5.4 **Manual** — con tres activos, BEBE NUEVO muestra el aviso y no
      abre el flujo; tras dar de alta a uno, abre.
- [ ] 5.5 **Manual** — encender la temperatura después de registrar: el
      asistente lista al bebé nuevo; seleccionarlo, peso, APLICAR; `activeSeq`
      de la placa (log `baby`) es el suyo.
- [ ] 5.6 **Manual** — lección 1 de Enfermería completa en formación: el bebé
      tecleado aparece en la lista de Bebes y en el asistente junto a ZOE;
      el monitor serie no muestra ningún `HMI,PROFILE_*`; al terminar, Bebes
      real no lo contiene y el perfil recordado queda limpio.
- [ ] 5.7 **Manual** — placa desconectada al pulsar REGISTRAR: toast "Sin
      respuesta de la placa" a los 3 s y vuelta a la lista.
- [ ] 5.8 **Manual** — con log serie de las dos placas, comprobar que tras
      REGISTRAR (con y sin peso) no aparece BOARD LINK LOST: el 2026-09-10 se
      observó en banco un LINK LOST segundos después de aplicar el asistente
      del bebé, sin log y sin causa determinada; el registro desde Bebes
      recorre el mismo `PROFILE_NEW` / `PROFILE_WEIGHT` en la placa.
- [ ] 5.9 **Manual** — placa desconectada ANTES de abrir Bebes (lista vacía
      por timeout): BEBE NUEVO avisa "Sin respuesta de la placa" y no abre.
      Desconectar con la pantalla de peso abierta: REGISTRAR avisa y no
      envía; reconectar y REGISTRAR funciona.
- [ ] 5.10 **Manual** — con la temperatura activa sobre A, registrar a B:
      el monitor de la placa muestra `PROFILE_NEW,B` y después
      `PROFILE_SELECT,<seqA>`; apagar todo, encender con SALTAR: `activeSeq`
      sella a A (log `baby`).
- [ ] 5.11 **Manual** — con un paciente real registrado y sin terapia, hacer
      la lección 1 completa: al salir, el perfil recordado sigue siendo el
      real (el asistente de salida lo ofrece) y Ajustes > Mantenimiento no
      marca limpieza terminal pendiente.
