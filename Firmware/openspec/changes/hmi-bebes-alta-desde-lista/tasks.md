Todo el cambio es de **Display_HMI**. Sin entorno de test: cada fase se
verifica con `pio run -e main` y prueba **manual** en el CrowPanel con la
motherBoard conectada. Ningún checkbox reclama cobertura automatizada.

## 1. Teclado compartido

Commit: `refactor(hmi): teclados del asistente del bebe en ui/InputKeypad`.

- [ ] 1.1 `include/ui/InputKeypad.h` + `src/ui/InputKeypad.cpp`: mapas de
      letras (sin coma) y dígitos, `InputKeypad_Create()`,
      `InputKeypad_StripCommasCb()`, `InputKeypad_ReadNumber()`, con los
      comentarios de `lv_btnmatrix` vs `lv_keyboard` y de la coma.
- [ ] 1.2 `BabyWizard.cpp` usa el módulo; sin cambio de comportamiento
      (mismos mapas, misma validación, mismos toasts).
- [ ] 1.3 `pio run -e main` en verde.

## 2. Registro desde Bebes

Commit: `feat(hmi): registrar un bebe nuevo desde la pantalla Bebes`.

- [ ] 2.1 `i18n_strings.def`: `STR_REGISTER_UC`, `STR_BABY_REGISTERED`,
      `STR_REGISTER_REFUSED`, `STR_THREE_ACTIVE_DISCHARGE_FIRST`,
      `STR_ADMISSION_WEIGHT_G` en es/en/fr/pt, ASCII 32-126.
- [ ] 2.2 `BabyHistory.cpp`: botón BEBE NUEVO bajo Activos (guarda de 3
      activos con toast); pasos `NewName` → `NewGest` → `NewWeight` con la
      tarjeta grande y `InputKeypad`; REGISTRAR envía `PROFILE_NEW` y, con
      peso, `PROFILE_WEIGHT` tras el ACK; `WaitingNewAck` / `WaitingNewRange`
      en `BabyHistory_Poll()` con timeout de 3 s sin reintento del NEW;
      `seq == 0` → toast de rechazo; éxito → toast "Bebe registrado" y
      recarga de la lista; X cancela sin enviar.
- [ ] 2.3 `BabyHistory.h`: `BabyHistoryStep`, `BabyHistory_GetStep()`,
      `BabyHistory_LastRegisteredSeq()`.
- [ ] 2.4 `pio run -e main` en verde; delta de Flash/RAM anotado contra el
      build de referencia del worktree limpio.

## 3. Formación: bebé de prácticas registrado y lección 1

Commit: `feat(hmi): la leccion de registro ensena a dar de alta desde Bebes`.

- [ ] 3.1 `training_mode.{h,cpp}`: `TRAINING_NEW_BABY_SEQ`,
      `Training_IsPracticeSeq()`, bebé de prácticas con nombre/semanas/peso
      que `Training_SimProfileNew` rellena, `SIM_LIST` con ZOE + él,
      `Training_SimProfileSelect` contesta el `seq` pedido si es de prácticas
      y carga sus datos para el rango; se borra en `Training_Enter()`.
- [ ] 3.2 `training_engine.cpp`: borrado del perfil recordado con
      `Training_IsPracticeSeq()`; `mainDialog` incluye `BabyHistory_IsOpen()`.
- [ ] 3.3 `lessons_nurse.cpp`: lección 1 reescrita (design decisión 7) en los
      cuatro idiomas, `LESSON_TABLE_IS_ASCII`; `goalTrainingBabyAdmitted`
      acepta cualquier `seq` de prácticas. `lessons_intro.cpp`: el texto de
      Bebes menciona BEBE NUEVO.
- [ ] 3.4 `maintenance.cpp`: si `Maintenance_Tick()` corre en formación, no
      anotar el cambio de `seq` (comprobar primero si ocurre).
- [ ] 3.5 `pio run -e main` en verde.

## 4. Documentación

Commit: `docs: update for hmi-bebes-alta-desde-lista`.

- [ ] 4.1 `docs/hmi.md` (formación: ZOE + bebé de prácticas registrado;
      lección 1), `docs/communication.md` §A.2 (Bebes registra).
- [ ] 4.2 `docs/adr/0002-...md`: revisión 2026-09-10 (registro desde Bebes
      en formación es virtual; el asistente sigue rechazando BEBE NUEVO).
- [ ] 4.3 `PROTOCOL.md`: la nota de `PROFILE_WEIGHT` sobre persistencia pasa
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
