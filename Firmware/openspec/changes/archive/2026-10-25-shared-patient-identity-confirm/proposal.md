## Why

Afecta a **shared** (el protocolo `CTRL,STATE` y `PROTOCOL.md`), a la
**motherBoard** (emite el campo nuevo) y a **Display_HMI** (lo consume). Es la
corrección de un fallo crítico de la revisión de `hmi-patient-identity`.

Ese cambio restaura de la NVS del HMI la identidad del bebé y la pinta como si
estuviera confirmada. Pero la NVS del HMI solo sabe lo que el HMI vio: nada
la valida contra la placa. Puede mostrar a **otro bebé** si:

- el HMI se traslada a otra placa (los `seq` empiezan en 1 en cada una y
  colisionan);
- la NVS de la placa se borra y reasigna `seq`;
- el alta se hizo por otra vía (otro HMI, la web, el visor);
- el asistente toma el atajo `selectExisting(seq_restaurado)`, que salta el
  selector y atribuye terapia y minutos al bebé equivocado.

La placa sí sabe quién está en la incubadora (`babyStore_getActiveSeq()`), pero
no lo manda en ningún mensaje periódico: el HMI no puede contrastar.

## What Changes

- **`CTRL,STATE` gana un campo 24, `activeSeq`**, el bebé que la placa tiene al
  mando (0 = ninguno), al final tras `latchedBitmask`. Compatible hacia ambos
  lados: un HMI antiguo ignora lo que sobra; una placa antigua no lo manda y el
  HMI lo trata como «desconocido».
- **La identidad restaurada nace sin confirmar** y solo se muestra confirmada
  cuando la placa la confirma (`activeSeq == seq`). La contradicción (otro
  `activeSeq`, o `activeSeq = 0` con terapia viva) la borra. Con una placa
  antigua se contrasta con `PROFILE_LIST` (una petición) y, si el bebé está,
  se muestra **gris y con «?»**, nunca como confirmada.
- **Sin atajo `selectExisting`** con una identidad no confirmada: el asistente
  ofrece el selector.
- **`pat_id` v2 atada a la placa**: guarda el `sn` de la placa; con otro `sn`
  se descarta. La v1 se descarta.
- **Los minutos solo se acreditan** si la placa dice que ese bebé es el activo
  (o, sin dato de la placa, si la identidad está confirmada), y se ocultan hasta
  que una lista los siembre cuando no hay `pat_ther` válido.
- **Mismo `seq` con otro nombre = bebé nuevo**: reinicia edad y acumulados.
- **`PROFILE_ACK 0` tras `PROFILE_NEW` ya no borra** la identidad del bebé que
  sigue en la incubadora; solo la borra tras `PROFILE_SELECT`.
- **Edad desconocida** sin hora de captura o pasado un año; `gestWeeks`
  validada al decodificar.
- **Layout**: la terapia acumulada pasa a una línea, sin «Total», bajo el título
  de cada cabecera (en francés pisaba el título); el detalle del botón «Bebés»
  trunca con puntos y los días se acotan (`99+`).

## Capabilities

### New Capabilities
- `ctrl-state-active-seq`: el campo `activeSeq` de `CTRL,STATE`.

### Modified Capabilities
- `hmi-patient-identity`: edad, persistencia, visibilidad de la identidad y de
  la terapia acumulada.

## Impact

- `shared/include/protocol.h`, `PROTOCOL.md`.
- `motherBoard/src/tasks/CommTask.cpp` (campo y búfer de 192 a 224 bytes).
- `Display_HMI`: `modules/patient/patient_session.{h,cpp}` (+ tests),
  `state/patient_state.{h,cpp}`, `tasks/CommTask.{h,cpp}`, `ui/BabyWizard.cpp`,
  `tasks/UITask.cpp`, `ui/ElementsCreation.cpp`, `i18n_strings.def`.
- Docs: `docs/hmi.md`, `docs/known_issues.md` (#23).
- Compatibilidad: campo opcional al final. No hace falta actualizar las dos
  placas a la vez, pero hasta que la placa lo mande, el HMI cae al contraste
  por `PROFILE_LIST`.
