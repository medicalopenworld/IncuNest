## Why

Afecta solo a **Display_HMI**. No toca `shared/`, ni la motherBoard, ni
`PROTOCOL.md`: todo lo que se necesita ya viaja por el cable.

Hoy, mirando el mando de la incubadora no hay forma de saber **de qué bebé es**
la terapia que corre. El nombre, las semanas de gestación y los minutos de
terapia solo existen dentro del asistente de alta y del visor de bebés, que son
pantallas que hay que abrir a propósito. Es el escenario clásico del «bebé
equivocado»: dos incubadoras iguales, una terapia en marcha, y nada en la
pantalla principal ni en la de bloqueo que diga a quién pertenece. Tampoco se ve
cuánta terapia lleva acumulada ese bebé, que es el dato que un clínico mira para
decidir si sigue o suspende una fototerapia.

Además, la identidad vive hoy **solo en RAM** (`BabyWizard.cpp`, estáticos
`s_session*`): un reinicio del HMI la pierde aunque la motherBoard conserve el
bebé activo y reanude sus terapias, y el mando se queda sin saber de quién es lo
que está funcionando.

## What Changes

- **Identidad del paciente en Main y en la pantalla de bloqueo**: nombre, edad
  gestacional corregida (`semanas+días`) y días de vida, mientras haya una
  sesión viva (`BabyWizard_HasLiveSession()`). En Main ocupa el botón «Bebés»
  del heading (sin tocar ninguno de los slots); en el bloqueo, una réplica a la
  derecha de la conectividad, donde esa pantalla tiene sitio libre.
- **Horas de terapia acumuladas** del bebé activo en la cabecera de cada panel
  de terapia de Main (temperatura, humedad, fototerapia), con el formato
  `3h 20m`.
- **Días de vida calculados en el HMI**: la placa no manda fecha de nacimiento.
  Se captura `CTRL,PROFILE_RANGE.ageDays` (que `BabyWizard_Poll` descartaba hoy)
  junto con la hora del momento (`HMI_GetEpochNow()`) y se recalcula con el
  tiempo transcurrido.
- **Acumulación local a 1 Hz** de los minutos de terapia a partir del
  `CTRL,STATE` real, con la misma regla que la placa, sembrada con los minutos
  de la última lista de perfiles y resincronizada cada vez que llegue una
  `CTRL,PROFILE_LIST` (por cualquier consumidor, sin consumir su flag).
- **Persistencia en NVS** (`hmi_cfg`): identidad y acumulados sobreviven a un
  reinicio del HMI. Si la placa dice otra cosa, manda la placa.
- **Módulo puro `modules/patient/patient_session`** con la lógica de edad,
  acumulación, formato y validación de los registros NVS, cubierto por tests
  Unity en `[env:native]` del HMI. El pegamento con LVGL/NVS/UART no tiene
  entorno de test y se verifica en banco.

## Capabilities

### New Capabilities
- `hmi-patient-identity`: identidad del paciente y terapia acumulada visibles en
  el mando.

### Modified Capabilities
Ninguna. `hmi-maintenance-reminder` y `baby-history-viewer` consumen
`BabyWizard_GetActiveSeq()`, que no cambia de semántica.

## Impact

- `Display_HMI/src/modules/patient/patient_session.{h,cpp}` (nuevo, puro).
- `Display_HMI/src/state/patient_state.{h,cpp}` (nuevo: estado vivo, NVS).
- `Display_HMI/src/ui/BabyWizard.cpp`, `src/tasks/CommTask.cpp` (+ `.h`),
  `src/tasks/UITask.cpp`, `src/ui/ElementsCreation.cpp` (+ `include/ui/ui.h`),
  `src/ui/BabyHistory.cpp` (reutiliza el formateo de minutos),
  `include/ui/i18n_strings.def`, `include/config/EEPROM_defines.h`,
  `platformio.ini` (`[env:native]`).
- Docs: `Firmware/docs/hmi.md`.
- Riesgo principal: layout del heading y de las cabeceras de panel, que se
  ajustó a mano al píxel; sin captura de pantalla disponible, queda como
  verificación manual en banco.
