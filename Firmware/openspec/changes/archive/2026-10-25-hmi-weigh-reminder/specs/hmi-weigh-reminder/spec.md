## ADDED Requirements

### Requirement: Se avisa si el bebé activo lleva 24 h sin pesada registrada

El HMI SHALL avisar de la pesada cuando se cumplan TODAS estas condiciones:

- hay sesión viva (`BabyWizard_HasLiveSession()`: identidad conocida y terapia
  en marcha ahora mismo);
- hay hora sincronizada (`HMI_GetEpochNow() != 0`);
- existe una referencia y han pasado 24 h o más desde ella
  (`ahora - referencia >= 24 h`);
- no hay un "más tarde" vigente.

La **referencia** SHALL ser, por este orden: el epoch de la última pesada
sellada para ESE `seq`; si no lo hay, el instante en que el HMI vio a ese
`seq` por primera vez al mando (persistido en NVS). Si no hay ninguno, no hay
referencia y no se avisa.

La decisión SHALL ser una función pura sin LVGL ni Arduino
(`modules/maintenance/weigh_reminder`), cubierta por la suite
`test/test_weigh_reminder` en `[env:native]`. Sin hora sincronizada NO SHALL
avisarse: no se inventan tiempos con `millis()`, que no sobreviven a un
reinicio. Una referencia en el futuro (reloj corregido hacia atrás) SHALL
tratarse como "no vencido".

#### Scenario: Justo antes de las 24 h no avisa
- **GIVEN** un bebé activo con sesión viva y última pesada hace 23 h 59 min 59 s
- **WHEN** se evalúa el aviso
- **THEN** no toca avisar
- *(Cubierto por `[env:native]`, `test_weigh_reminder`.)*

#### Scenario: A las 24 h exactas avisa
- **GIVEN** un bebé activo con sesión viva y última pesada hace exactamente 24 h
- **WHEN** se evalúa el aviso
- **THEN** toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Sin hora no se avisa
- **GIVEN** un bebé activo cuya última pesada fue hace 3 días
- **WHEN** el HMI no tiene hora sincronizada
- **THEN** no toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Sin sesión viva no se avisa
- **GIVEN** un bebé recordado pero sin terapia en marcha, con pesada de hace 3 días
- **WHEN** se evalúa el aviso
- **THEN** no toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Sin sello usa el momento en que se vio al bebé
- **GIVEN** un bebé activo sin pesada sellada para su `seq`
- **WHEN** el HMI lo vio por primera vez hace 25 h
- **THEN** toca avisar
- **AND** si lo vio hace 2 h no toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Un sello de otro bebé no cuenta
- **GIVEN** un sello de pesada de hace 3 días perteneciente a otro `seq`
- **WHEN** se evalúa el aviso del bebé activo, visto por primera vez hace 2 h
- **THEN** no toca avisar, porque la referencia es el "visto" y no el sello ajeno
- *(Cubierto por `[env:native]`.)*

#### Scenario: Referencia en el futuro
- **GIVEN** un sello posterior a la hora actual
- **WHEN** se evalúa el aviso
- **THEN** no toca avisar
- *(Cubierto por `[env:native]`.)*

#### Scenario: Un "más tarde" vigente calla el aviso
- **GIVEN** una pesada vencida y un snooze que expira dentro de 1 h
- **WHEN** se evalúa el aviso
- **THEN** no toca avisar
- **AND** al llegar el instante de expiración vuelve a tocar
- *(Cubierto por `[env:native]`.)*

### Requirement: Las pesadas se sellan con la hora UTC al enviarlas

El HMI SHALL sellar en la NVS (`hmi_cfg`: `wgh_seq` y `wgh_last`) el epoch UTC
y el `seq` de cada `HMI,PROFILE_WEIGHT` con un peso real (gramos > 0) enviado
para el bebé que está al mando. Esto SHALL cubrir también la
pesada de admisión del asistente, de modo que el sello de un bebé nuevo
empiece con ella.

- `SKIP` (0 g) NO SHALL sellar: no es una pesada.
- Un peso enviado para un `seq` distinto del activo (alta desde la pantalla
  Bebés) NO SHALL tocar el sello del activo.
- En formación (`Training_IsActive()` o un `seq` de práctica) NO SHALL
  escribirse nada en NVS (ADR-0002).
- Registrar un peso SHALL cancelar el "más tarde" vigente.
- Sin hora en ese instante SHALL anotarse la pesada como pendiente de fecha y
  sellarse con la primera hora válida; mientras tanto no se avisa.
- Al dar de alta al bebé (`BabyWizard_GetActiveSeq()` pasa a 0) se SHALL borrar
  el sello, el "visto" y el "más tarde". Al pasar a un bebé distinto se SHALL
  descartar lo que no le pertenezca.
- Un sello o "visto" fechado en el futuro SHALL reanclarse a la hora actual.
- Las escrituras NVS SHALL hacerse fuera de `LVGL_Lock`.

Claves nuevas de `hmi_cfg` (≤ 15 caracteres): `wgh_seq`, `wgh_last`,
`wgh_sseq`, `wgh_sfirst`, `wgh_snooze`.

#### Scenario: La pesada de admisión inicia el sello
- **WHEN** el operador admite un bebé nuevo y teclea 1850 g en el asistente
- **THEN** `wgh_seq` es el `seq` del bebé y `wgh_last` el epoch UTC de ese momento
- *(Verificación manual en banco: Display_HMI no tiene entorno de test de NVS; leer las claves en el log serie `WGH`.)*

#### Scenario: SIN PESO no sella
- **WHEN** el operador pulsa SIN PESO en el asistente
- **THEN** `wgh_last` no cambia
- *(Verificación manual en banco.)*

#### Scenario: El alta borra el sello
- **WHEN** el operador da de alta al bebé en el diálogo de salida
- **THEN** `wgh_seq`, `wgh_last`, `wgh_sseq`, `wgh_sfirst` y `wgh_snooze` quedan a 0
- *(Verificación manual en banco.)*

#### Scenario: Formación no escribe NVS
- **WHEN** el alumno registra un peso durante una lección
- **THEN** ninguna clave `wgh_*` cambia
- *(Verificación manual en banco.)*

### Requirement: El aviso ofrece PESAR AHORA y MÁS TARDE

El aviso SHALL usar la misma tarjeta que el de mantenimiento (780x460, overlay
reutilizado), con el nombre del bebé, las horas transcurridas desde la
referencia y dos botones, sin X:

- **PESAR AHORA** abre en el mismo diálogo una entrada numérica (teclado de
  dígitos) para el bebé activo, con el rango 400-5000 g del asistente. Al
  registrar envía `HMI,PROFILE_WEIGHT` y sella la pesada (cancelando el
  "más tarde"). En formación SHALL rechazarse con el aviso estándar.
- **MÁS TARDE** calla el aviso 24 h (`wgh_snooze`, epoch UTC), y solo si hay
  hora.

La respuesta de rango que la placa envía tras la pesada SHALL descartarse en el
diálogo cuando el asistente no esté abierto, para no dejarla huérfana.
Las cadenas SHALL existir en español, inglés, francés y portugués.

#### Scenario: PESAR AHORA registra y calla el aviso
- **GIVEN** el aviso abierto para un bebé con 26 h sin pesar
- **WHEN** el operador pulsa PESAR AHORA, teclea 1900 y pulsa REGISTRAR
- **THEN** el HMI envía `HMI,PROFILE_WEIGHT,<seq>,1900`
- **AND** el aviso se cierra y no vuelve a salir hasta pasadas 24 h
- *(Verificación manual en banco.)*

#### Scenario: Peso fuera de rango
- **WHEN** el operador teclea 300 o 6000 en la entrada
- **THEN** se muestra el error de rango y no se envía nada
- *(Verificación manual en banco.)*

#### Scenario: MÁS TARDE calla 24 h
- **WHEN** el operador pulsa MÁS TARDE
- **THEN** no vuelve a avisar en los siguientes desbloqueos durante 24 h
- *(Verificación manual en banco.)*

#### Scenario: Idiomas
- **WHEN** el equipo está en cada una de las cuatro lenguas
- **THEN** el aviso y la entrada muestran sus cadenas en esa lengua sin texto cortado
- *(Verificación manual en banco.)*

### Requirement: El aviso de pesada espera su turno y cede la pantalla como el de mantenimiento

El aviso de pesada SHALL armarse con el mismo desbloqueo que el de
mantenimiento y SHALL cumplir sus mismas reglas: abrirse solo en
`ui_ScreenMain`, sin ningún otro diálogo modal abierto (ayuda, tutorial,
centro de alarmas, asistente de bebé, diálogo de salida, ajuste de hora,
prueba de fábrica y **pantalla Bebés**), fuera de formación; cerrarse sin
contestar ante cualquier alarma, enlace perdido, cambio de pantalla o
`MNT_IDLE_TIMEOUT_MS` sin toque.

Si coinciden un aviso de mantenimiento y uno de pesada, SHALL salir uno detrás
del otro (primero mantenimiento), sin bucles.

#### Scenario: No se cuela sobre la pantalla Bebés
- **GIVEN** una pesada vencida y el desbloqueo hecho
- **WHEN** la pantalla Bebés está abierta
- **THEN** el aviso espera y sale al cerrarla
- *(Verificación manual en banco.)*

#### Scenario: Cede ante una alarma
- **WHEN** el aviso de pesada está abierto y la placa anuncia una alarma
- **THEN** se cierra en la siguiente pasada y no cuenta como respuesta
- *(Verificación manual en banco.)*

#### Scenario: Mantenimiento y pesada vencidos a la vez
- **WHEN** ambos están vencidos y se desbloquea
- **THEN** sale el de mantenimiento; al pulsar MÁS TARDE sale el de pesada
- *(Verificación manual en banco.)*
