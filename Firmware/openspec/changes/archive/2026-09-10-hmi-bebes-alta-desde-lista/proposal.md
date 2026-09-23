## Why

Afecta solo a **Display_HMI**. No toca `shared/` ni `motherBoard`: el
protocolo `PROFILE_*` (`Firmware/PROTOCOL.md` v2.2.0) ya tiene todo lo que
hace falta.

Hoy el registro de un bebé solo nace **dentro del asistente de activación**
(`BabyWizard.cpp`): al encender la primera terapia, el asistente ofrece BEBE
NUEVO y manda `HMI,PROFILE_NEW` al confirmar las semanas de gestación. La
pantalla **Bebes** (`BabyHistory.cpp`, botón de la barra superior) es de
consulta: lista los activos con ALTA, los archivados paginados y la curva de
peso de cada uno, pero no permite dar de alta a nadie.

Eso tiene dos costes en el uso real:

- **El ingreso y la primera terapia no coinciden.** Un bebé puede ingresar
  en la incubadora sin encender nada todavía (observación, canguro previo,
  espera de la orden médica). Hasta que alguien enciende una terapia el bebé
  no existe para el equipo, y `admissionEpoch` —que la placa sella al crear
  el perfil (`babyStore_createProfile`)— queda fechado en la primera
  terapia, no en el ingreso. Los días de vida que la placa deriva de ahí y la
  curva de peso arrancan tarde.
- **La formación lo enseña mal.** La lección 1 de Enfermería ("Registrar y
  seguir a un bebe") explica que el registro "se crea con BEBE NUEVO al
  encender una terapia" y el alumno practica solo seleccionando a ZOE; el
  paso de introducir nombre y semanas nunca se ejercita, porque en formación
  el asistente rechaza BEBE NUEVO.

El usuario pide (2026-09-10) que desde la pestaña Bebes se pueda añadir un
bebé nuevo y que los cursos de formación se actualicen en consecuencia.

## What Changes

- **Botón BEBE NUEVO en la pantalla Bebes**, bajo la sección Activos. Abre
  un flujo de tres pantallas con el mismo teclado del asistente: nombre
  (letras) → semanas de gestación (20-40) → peso al ingreso (400-5000 g, o
  SIN PESO). El registro se confirma con **REGISTRAR** en la última
  pantalla: solo entonces sale `HMI,PROFILE_NEW,name,gestWeeks` y, si hay
  peso, `HMI,PROFILE_WEIGHT,seq,grams` tras el ACK. La X en cualquier
  pantalla anterior cancela sin haber enviado nada. Al terminar, la lista se
  recarga y el bebé aparece en Activos con un aviso "Bebe registrado".
- **Guarda de los 3 slots**: con tres bebés activos el botón no abre el
  flujo y avisa de que hay que dar de alta a uno antes. Desde esta pantalla
  el alta está a un toque; dejar que la placa desaloje por FIFO a un
  paciente activo en silencio no es aceptable en la pantalla cuyo objeto es
  precisamente el registro.
- **Teclado compartido**: los mapas de teclas (letras sin coma, dígitos), el
  manejador de pulsación, el filtro de comas y la lectura numérica con rango
  salen de `BabyWizard.cpp` a `ui/InputKeypad.{h,cpp}`, usado por el
  asistente y por Bebes. El asistente no cambia de comportamiento.
- **Formación**: registrar desde Bebes durante una lección crea un **bebé de
  prácticas virtual** (`seq 0xFFFE`, con el nombre y las semanas que tecleó
  el alumno) que aparece en la lista de formación junto a ZOE y que el
  asistente deja seleccionar; nada llega a la placa. La lección 1 de
  Enfermería se reescribe alrededor de ese flujo: registrar desde Bebes →
  encender la temperatura → seleccionar al bebé recién registrado → peso,
  edad, APLICAR. El asistente sigue rechazando BEBE NUEVO en formación: el
  registro se enseña desde Bebes.
- **Sin cambios en motherBoard.** El efecto lateral conocido de
  `PROFILE_NEW` sobre `s_wizardSeq` (deuda ya documentada en ADR-0002:
  la placa sella como activo el último `seq` creado o seleccionado al
  arrancar una terapia) no cambia de naturaleza: con SALTAR en el asistente
  la placa ya adivinaba; ahora adivina el último registrado, que es el
  candidato más probable. Con selección explícita, que es el camino normal,
  el sello es correcto. Queda anotado en el diseño.

## Capabilities

### New Capabilities
<!-- Ninguna. -->

### Modified Capabilities
- `baby-history-viewer`: la pantalla Bebes registra bebés nuevos (requisito
  añadido; la capacidad vive todavía en el cambio sin archivar
  `shared-baby-profile-nte-wizard`, así que este delta la crea como spec
  principal al archivar).
- `hmi-training-courses`: el modo formación admite un bebé de prácticas
  registrado por el alumno además de ZOE; la lección 1 de Enfermería enseña
  el registro desde Bebes.

## Impact

- `Display_HMI/src/ui/BabyHistory.cpp` + `include/ui/BabyHistory.h`
  (flujo de registro, `BabyHistory_GetStep()`,
  `BabyHistory_LastRegisteredSeq()`).
- Nuevo `Display_HMI/src/ui/InputKeypad.cpp` + `include/ui/InputKeypad.h`;
  `src/ui/BabyWizard.cpp` pasa a usarlo (refactor sin cambio funcional).
- `Display_HMI/src/state/training_mode.{h,cpp}` (bebé de prácticas
  registrado, `Training_IsPracticeSeq()`),
  `src/ui/training/training_engine.cpp`, `src/ui/training/lessons_nurse.cpp`
  (lección 1), `src/ui/training/lessons_intro.cpp` (texto de Bebes).
- `Display_HMI/include/ui/i18n_strings.def`: cinco cadenas nuevas en los
  cuatro idiomas.
- Docs: `docs/hmi.md` (formación), `docs/communication.md` §A.2,
  `docs/adr/0002-...md` (revisión), `PROTOCOL.md` (corrige la nota de
  `PROFILE_WEIGHT`: el perfil se persiste en `PROFILE_NEW`, como hace
  `babyStore_createProfile`; la nota "hasta este mensaje nada se persiste"
  describe una versión anterior).
