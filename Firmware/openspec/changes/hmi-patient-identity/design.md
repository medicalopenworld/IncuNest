## Context

Mapa verificado del código existente (Display_HMI 4.4.0):

- La sesión activa son cuatro estáticos de `ui/BabyWizard.cpp` (`s_sessionSeq`,
  `s_sessionName`, `s_sessionGest`, `s_sessionWeight`), fijados al consumir
  `CTRL,PROFILE_ACK` y borrados por `BabyWizard_ClearActiveProfile()` al dar de
  alta. Solo RAM.
- `BabyWizard_HasLiveSession()` = seq != 0 **y** alguna terapia activa. Es la
  definición vigente de «identidad actual, no solo recordada».
- `CTRL,PROFILE_RANGE` trae `ageDays`; `CommTask.cpp` lo parsea a
  `g_profileRange.ageDays` y `BabyWizard_Poll` lo descarta.
- Los minutos por terapia solo llegan en `CTRL,PROFILE_LIST`, bajo demanda, con
  un flag (`g_pendingProfileList`) de **un único consumidor** (wizard o visor).
- La placa acumula minutos enteros por bebé y los vuelca a NVS al flanco de
  apagado o cada 10 min (`THERAPY_FLUSH_INTERVAL_MS`), acreditando al `seq`
  activo en el flanco de encendido. Regla: termo con `act=1|3`, humedad con
  `act=2|3`, foto con `photo != 0`.
- `UI_Task` corre una pasada cada pocos ms; el patrón de «decidir dentro de
  `LVGL_Lock`, escribir NVS fuera» está en el bloque de escritura diferida del
  final del bucle (ARQ-LOCK-001).
- Pantalla: 800x480 (no 1024; el encargo decía 1024, pero los slots del heading
  están calculados sobre 800: `400 - 19 - 40 = 341`).

## Goals / Non-Goals

**Goals**
- Nombre, edad corregida y días de vida visibles en Main y en el bloqueo.
- Minutos acumulados por terapia visibles en los tres paneles de Main.
- Sobrevivir a un reinicio del HMI.
- Que la lógica de cálculo tenga tests de host reales.

**Non-Goals**
- Cambiar el protocolo (no se añade fecha de nacimiento ni epoch de admisión).
- Un peticionario periódico de `PROFILE_LIST` desde Main: competiría por el
  flag de un solo consumidor y rompería al wizard y al visor.
- Mostrar identidad sin terapia en marcha (ver D2).

## Decisions

### D1. Lógica pura aparte, estado vivo aparte

`modules/patient/patient_session.{h,cpp}`: sin Arduino, sin LVGL, sin tiempo
real (todo entra por parámetro). Contiene:

- `patient_age_compute()`: días de vida y edad corregida.
- `PatientAccum` y `patient_accum_*`: acumulador por bebé.
- `patient_fmt_minutes()`: `3h 20m` / `45m` / `3h` (se mueve `fmtMinutes` de
  `BabyHistory.cpp`; el visor pasa a usar el helper, mismo texto).
- Registros de NVS (`PatientIdentityRecord`, `PatientTherapyRecord`) y su
  validación.

`state/patient_state.{h,cpp}` (Arduino, sin LVGL) posee la instancia viva, lee y
escribe NVS y expone una API pequeña a UI y wizard. Nada de LVGL ahí.

### D2. Cuándo se muestra la identidad

Mientras `BabyWizard_HasLiveSession()` sea verdad. Un `seq` recordado con la
incubadora ya inactiva no se pinta: la propia semántica de esa función dice que
entonces la sesión de cuidado terminó y el siguiente alta vuelve a preguntar. Un
nombre ajeno en pantalla con la cabina vacía es justo el error que esta función
quiere evitar. Los acumulados siguen la misma regla (se ocultan sin sesión viva).

### D3. Días de vida y edad corregida

La placa manda `ageDays` en el instante de la respuesta. El HMI guarda
`(ageDays, epochCaptura)` y calcula

    díasVida  = ageDays + floor((ahora - epochCaptura) / 86400)
    totalDías = semanasGest * 7 + díasVida
    corregida = totalDías / 7 "+" totalDías % 7

- Sin hora (`HMI_GetEpochNow() == 0` en la captura o ahora) el transcurrido es
  0: se muestra el valor capturado, que es un límite inferior. Nunca se inventa.
- `ageKnown=false` => no hay días de vida ni edad corregida; se muestra solo la
  gestación al nacer (`34 sem`), que es otro número y se lee distinto.
- Error máximo de ±1 día respecto al cambio de día de la placa, porque se cuenta
  desde el instante de captura y no desde el de ingreso. Aceptado y documentado.
- Solo se captura `ageDays` con `ageKnown=true`, y solo para el `seq` de la
  ejecución del wizard en curso (la misma guarda que ya descarta respuestas
  obsoletas).
- Un bebé elegido solo para lámpara o humidificador (`targetIsIdentityOnly`) no
  pasa por la pantalla de peso/edad, así que no hay `ageDays`: se muestra la
  gestación sin edad corregida hasta que otra ejecución del wizard la capture.

### D4. Acumulación de minutos

- Contadores en **segundos** (con resto en ms compartido) en el HMI; se muestran
  minutos enteros, como la placa.
- `patient_accum_tick(now_ms, act, photo, linkOk)` cada ~1 s. Acredita
  `min(dt, 5000 ms)` a cada contador activo: un atasco de la UI no se convierte
  en horas inventadas, y `linkOk=false` (placa muda) congela, porque
  `ctrl_state_msg` queda obsoleto y no dice nada de lo que pasa ahora.
- Solo acredita con `seq != 0`, igual que la placa, y no en modo formación ni
  con el bebé de prácticas.
- **Siembra**: al consumir el `PROFILE_ACK` se siembra con los minutos del ítem
  de `s_list` (la lista que acaba de traer el wizard) o 0 si es nuevo. Si el
  acumulador ya es del mismo `seq`, no se resiembra (el local es más fresco).
- **Resincronización**: el parser de `CTRL,PROFILE_LIST` copia además la lista a
  `g_profileListMirror` (con contador de generación, bajo un `portMUX`) sin
  tocar `g_pendingProfileList`. `UI_Task` la lee y llama a
  `patient_accum_resync()`. Por contador: si el contador **corre** en el HMI,
  gana el mayor (la placa vuelca cada 10 min, así que su cifra es una cota
  inferior de lo que va); si **no corre**, manda la placa.
- **Persistencia**: blob único `pat_ther` cada 60 s si cambió, y en el flanco de
  parada total. Un registro de 16 B por escritura: ~525 k escrituras/año como
  máximo, muy por debajo del desgaste de la NVS repartido en su partición.

### D5. Qué se persiste (`hmi_cfg`)

Dos blobs, no claves sueltas: la NVS no es transaccional y un corte entre
`seq` y `name` dejaría la identidad de un bebé con los minutos de otro. Mismo
argumento que `rtc_tz`.

| Clave | Contenido | Tamaño |
|---|---|---|
| `pat_id` | versión, `seq`, nombre[24], semanas gest., `ageDays`, `epochCaptura` | 36 B |
| `pat_ther` | `seq`, segundos foto, termo, humedad | 16 B |

Restauración al arrancar (antes de crear la UI, lectura fuera de `LVGL_Lock`):
si el registro valida, se llama a `BabyWizard_SetSession()` y se siembra el
acumulador. `pat_ther` solo se acepta si su `seq` coincide con el de `pat_id`.
Borrado en `BabyWizard_ClearActiveProfile()`. **Manda la placa**: la restaurada
es una identidad *recordada*; el primer `PROFILE_ACK` que la contradiga (el
wizard ya olvida el seq si la placa responde `0`) la sustituye, y cualquier
`PROFILE_LIST` posterior resincroniza minutos. No existe mensaje de la placa que
diga «el bebé activo es otro» sin que el wizard lo pida, así que más allá de
eso no se puede contrastar.

### D6. Layout

**Main**: el heading está lleno (ayuda 20–64, reloj 83–263, conectividad
282–322, candado 341–459, «Bebés» 468–578, alarma 622–670, ajustes 744–794).
La identidad vive **dentro del botón «Bebés»** (110x44): con sesión viva se
oculta su rótulo y aparecen dos líneas, nombre (14 pt, desplazamiento circular
si no cabe) y `34+3 sem - 5d` (12 pt). Sin sesión, vuelve a leerse «Bebés».
Ningún slot se mueve y el botón sigue abriendo el visor de bebés, que es lo
natural. Coste: el nombre largo se lee desplazándose, no de golpe.

**Bloqueo**: el heading de `ui_ScreenLock` no tiene botones a la derecha de la
conectividad. Réplica a la derecha de ella (x desde 350, ancho 430): nombre en
18 pt y detalle en 14 pt, sin truncar.

**Paneles**: etiqueta de dos líneas de 12 pt (`Total` / `3h 20m`) en el hueco
entre el título y el botón de cada cabecera. No hay sitio para una línea nueva
(los márgenes de las cabeceras son de 0 px y están documentados en el código).
Es la parte más arriesgada: el ancho de títulos largos («PHOTOTHERAPIE») deja
unos 50 px y «12h 20m» a 12 pt mide unos 50. Se verifica en banco con las
cuatro lenguas; si roza, se baja a 10 pt (la fuente está habilitada).

### D7. Cadenas

Todo en el catálogo `ui/i18n_strings.def` (cuatro lenguas, sin acentos):
`STR_PATIENT_AGE_FMT`, `STR_PATIENT_GEST_ONLY_FMT`, `STR_PATIENT_DAYS_ONLY_FMT`,
`STR_THERAPY_TOTAL`.

## Risks / Trade-offs

- Identidad restaurada que ya no es la de la placa: se mitiga con D5 (manda la
  placa) y con que solo se muestra con terapia en marcha, pero hay una ventana,
  tras reiniciar el HMI, en la que se pinta el bebé *recordado* hasta que
  llegue un ACK o una lista. Es el compromiso explícito del cambio; la
  alternativa (no restaurar) deja la pantalla sin identidad con terapia en
  marcha, que es peor.
- La placa vuelca minutos cada 10 min, así que `PROFILE_LIST` puede ir por
  detrás del acumulado local: de ahí el `max` en D4.
- `known_issues.md` #10 (eco de órdenes del asistente) no se toca: la
  acumulación lee `ctrl_state_msg` ya parseado y no escribe `hmi_msg`.
