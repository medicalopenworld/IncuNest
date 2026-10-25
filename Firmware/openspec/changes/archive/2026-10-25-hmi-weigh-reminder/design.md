## Context

`BabyWizard` pide el peso solo al activar una terapia. La única salida de un
peso hacia la placa es `Communication_SendProfileWeight(seq, grams)`
(`grams == 0` es `SKIP`), llamada desde `BabyWizard.cpp` (`sendWeightAndWait`)
y `BabyHistory.cpp` (alta de bebé desde la pantalla Bebés). La respuesta
`CTRL,WEIGHT_HISTORY` trae `dayOffset[]` relativo a la admisión: sirve para
dibujar la curva, no para saber QUÉ HORA fue la última pesada. Hace falta un
epoch propio.

`maintenance.cpp` ya resuelve el mismo problema de forma: epochs UTC en NVS,
`Tick()` que siembra y reancla, snooze de 24 h, y un diálogo armado por el
desbloqueo. Este cambio reutiliza ese motor, no lo duplica.

## Decisiones

### D1. Decisión pura separada del estado

`weigh_reminder.{h,cpp}` es una función pura de una estructura de entradas
(`weigh_inputs_t`) a un booleano, sin Arduino, NVS ni LVGL. Es lo único que
entra en `[env:native]`. El estado y la NVS viven en `weigh_tracker.{h,cpp}`
(Arduino, no testeable en host), que monta la estructura y delega.

Regla (referencia = el instante contra el que se cuentan las 24 h):

1. referencia = `weighEpoch` si `weighSeq == activeSeq` y `weighEpoch != 0`;
2. si no, `seenEpoch` si `seenSeq == activeSeq` y `seenEpoch != 0` (el momento
   en que el HMI vio al bebé por primera vez, persistido);
3. si no, no hay referencia.

Avisa si y solo si: sesión viva, `activeSeq != 0`, hora válida (`now != 0`),
hay referencia, `referencia <= now`, `now - referencia >= 24 h`, y no hay
snooze vigente (`snoozeUntil == 0 || now >= snoozeUntil`). Es `>=`, igual que
`periodElapsed()` del mantenimiento.

### D2. Sello al enviar, no al recibir

El protocolo no confirma una pesada (la única respuesta es el rango NTE, que
además no llega en todos los caminos). Se sella al ENVIAR, con una pesada
real (`grams > 0`). `SKIP` NO sella: es "no hay peso", no una pesada. El sello
se pone dentro de `Communication_SendProfileWeight()` justo después de las
guardas de formación, de modo que cualquier llamador actual o futuro lo hace
bien y la formación nunca escribe NVS (ADR-0002).

Solo se sella si `seq == BabyWizard_GetActiveSeq()`. El alta de un bebé desde
la pantalla Bebés (`BabyHistory`) pesa a un bebé que NO está al mando y no
debe pisar el sello del que sí lo está (hay un único hueco de sello). Cuando
ese bebé pase a estar activo, `seenEpoch` cubre el caso.

Sin hora en el momento de la pesada no se puede fechar: se anota en RAM que
hay una pesada pendiente de fecha y `Tick()` la sella con la primera hora
válida. Mientras tanto no se avisa (sin hora no se avisa).

### D3. Tiempos por epoch, nunca por `millis()`

Todo es epoch UTC (`HMI_GetEpochNow()`), como `mnt_*`. `millis()` no
sobrevive a un reinicio y el equipo se reinicia. Un sello en el futuro
(reloj corregido hacia atrás) se reancla a hoy en `Tick()`, igual que el
mantenimiento; la función pura además lo trata como "no vencido" por si acaso.

### D4. Ciclo de vida del sello

`Tick()` (cada vuelta de UI_Task, fuera de formación) observa
`BabyWizard_GetActiveSeq()`:

- pasa a 0 (alta): se borran sello, "visto" y snooze;
- pasa a otro `seq` que no coincide con el sello ni el "visto" guardados
  (bebé nuevo): se descartan los que no son suyos y el snooze;
- si hay `seq` y hora y aún no se había visto ese `seq`: `seenSeq/seenEpoch`
  se fijan a ahora. Es el "momento en que el HMI vio el ACK" persistido: el
  ACK de selección/alta actualiza `s_sessionSeq`, y esa es la primera vuelta
  en que `Tick()` lo ve.

Un reinicio del HMI pierde `s_sessionSeq` (vive en RAM), así que tras
reiniciar no hay aviso hasta que el asistente vuelva a identificar al bebé.
Es una limitación del identificador de sesión, no de este cambio; el sello y
el "visto" siguen en NVS y se reconocen cuando el mismo `seq` vuelve.

### D5. NVS fuera de `LVGL_Lock`

`Maintenance` escribe NVS dentro del lock (escrituras raras). Aquí las
escrituras ocurren en cada pesada y en cambios de bebé, así que se sigue la
regla del bucle de UI (`UITask.cpp` ~4984): el tracker solo marca "sucio" y
`WeighTracker_Flush()` escribe tras `LVGL_Unlock()`, junto a
`TrainingProgress_Flush()`.

### D6. UI: un solo diálogo, dos modos

`MaintenanceDialog` ya es el overlay de 780x460 con su gestión de cesión,
tope y devolución al reposo. Se le añade un modo (`Maintenance`, `WeighPrompt`,
`WeighEntry`) en vez de un segundo overlay: la apariencia es la misma y la
lógica de ceder paso queda en un solo sitio.

- `WeighPrompt`: título, "<Nombre>: sin pesar desde hace N h" y botones
  PESAR AHORA / MÁS TARDE.
- `WeighEntry`: textarea numérico + teclado de dígitos (`InputKeypad`),
  400-5000 g, botones REGISTRAR / ATRÁS. Al registrar: en formación se
  rechaza; si no, se envía `Communication_SendProfileWeight()` y se cierra con
  un toast. La placa contesta con el rango NTE (`g_pendingProfileRange`); el
  diálogo lo descarta (como hace `BabyHistory.discardPendingReplies()`) en una
  ventana de 3 s si el asistente no está abierto, para no dejar una respuesta
  huérfana.

Armado: dos banderas independientes (`s_armedMaint`, `s_armedWeigh`), ambas
puestas por el desbloqueo. `Poll()` abre primero el de mantenimiento si toca y
luego el de pesada, cada uno consumiendo su propia bandera: si caen a la vez,
salen uno detrás de otro sin bucles (cerrar el primero no re-arma nada).

Sin interruptor de Ajustes: el aviso de pesada NO cuelga de `mnt_en`, que es
off en NVS virgen y dejaría la función invisible en las unidades nuevas. Se
anota como duda abierta.

### D7. Cesión

Se añade `BabyHistory_IsOpen()` a la lista de modales ante los que espera el
aviso (hueco detectado, afecta también al mantenimiento). Resto igual:
`mustYield()` (alarma o enlace perdido), tope `MNT_IDLE_TIMEOUT_MS`, cambio de
pantalla.

## Alternativas descartadas

- **Pedir el epoch de la última pesada a la placa**: obliga a tocar el
  protocolo y la `motherBoard`, fuera de alcance.
- **Contar con `millis()`**: no sobrevive a reinicios.
- **Segundo overlay/diálogo**: duplica la lógica de cesión y de tope.
- **Sellar al recibir el rango**: el rango no llega siempre y el aviso
  reaparecería tras una pesada válida.

## Verificación

Lógica pura: `pio test -e native` (suite `test_weigh_reminder`). Todo lo demás
(NVS, diálogo, teclado, cesión) no tiene entorno de test: compilación
(`pio run -e main`) y checklist manual de banco, descrito en `tasks.md`.
