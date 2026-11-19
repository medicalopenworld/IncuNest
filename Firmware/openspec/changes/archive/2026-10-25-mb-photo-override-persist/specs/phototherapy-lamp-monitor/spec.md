## MODIFIED Requirements

### Requirement: Soltar la prueba de depuración devuelve el control al display en el acto

Mientras la prueba de fototerapia de depuración esté activa, la placa SHALL forzar la lámpara a ON y SHALL registrar las órdenes del display (y la caducidad del temporizador de fototerapia como orden de apagado) sin aplicarlas. **El ON forzado SHALL NOT persistirse nunca** en NVS (solo existe en `in3.phototherapy`). **La orden del display SHALL persistirse siempre, también durante la prueba**, porque el campo `photo` de `CTRL,STATE` emite esa misma orden y por tanto lo que el display devuelve nunca está contaminado por el ON forzado. Al soltarla (`PHOTO,0` o apagar el modo depuración), la placa SHALL aplicar en el siguiente tick periódico, por el camino normal de una orden, la **última orden del display** registrada (la del operador manda; si no hubo ninguna, la que había al empezar), SHALL NOT esperar ninguna trama del display, y SHALL persistir además el valor aplicado, porque con el display colgado no llega ningún keepalive que lo guarde. No SHALL haber ventana de gracia, confirmación, mínimo ni plazo de respaldo. Si se vuelve a encender la prueba antes de que el tick aplique la liberación pendiente (`PHOTO,1`, `PHOTO,0`, `PHOTO,1` en una misma pasada de la consola), la placa SHALL conservar la orden del display ya registrada y SHALL NOT capturar como orden del display el `in3.phototherapy` contaminado por la prueba anterior. La escritura del PWM de la lámpara SHALL serializarse entre el lazo de intensidad y el camino de aplicación para que un apagado no pueda ser pisado por una escritura tardía del lazo, y SHALL NOT instalarse fade LEDC en ese canal. La contabilidad de exposición por bebé SHALL NOT contar la prueba ni una lámpara con `ALARM_PHOTO_LAMP_FAULT` declarada, y `timeTrackHandler` SHALL NOT sumar la prueba a las horas de fototerapia.

#### Scenario: Real OFF, prueba y soltar sin tramas

- **GIVEN** la fototerapia real apagada y una prueba activa
- **WHEN** se suelta y no llega ninguna trama del display
- **THEN** `photo_override_take_release()` entrega OFF una sola vez
- **AND** la prueba deja de forzar

Cubierto por `[env:native]` (`test_photo_override`).

#### Scenario: Real ON, vence el temporizador durante la prueba

- **GIVEN** la fototerapia real encendida y una prueba activa
- **WHEN** vence el temporizador de fototerapia y después se suelta
- **THEN** al soltar se entrega OFF y el valor guardado es OFF

Cubierto por `[env:native]` para el valor entregado. La escritura en NVS es verificación manual (`main.cpp` no entra en host).

#### Scenario: El operador manda OFF durante la prueba

- **GIVEN** la fototerapia real encendida y una prueba activa
- **WHEN** el display ordena OFF y después se suelta
- **THEN** al soltar se entrega OFF

Cubierto por `[env:native]`.

#### Scenario: Se persiste la orden del display durante la prueba

- **GIVEN** una prueba activa
- **WHEN** llega un keepalive del display con `photo=0`
- **THEN** `KEY_PHOTO_ACTIVE` se guarda como 0 y la lámpara sigue forzada a ON
- **AND** el ON forzado no se guarda en ningún momento

Verificación manual en banco.

#### Scenario: Se persiste el valor aplicado al soltar

- **GIVEN** una prueba soltada con el display colgado
- **WHEN** el tick aplica la liberación
- **THEN** `KEY_PHOTO_ACTIVE` se guarda con el valor aplicado

Verificación manual en banco.

#### Scenario: Rearme antes del tick

- **GIVEN** una prueba iniciada con la orden del display en OFF y soltada, con la liberación pendiente
- **WHEN** se vuelve a iniciar la prueba con `in3.phototherapy` ya a 1 por la prueba anterior
- **THEN** el campo `photo` sigue emitiendo OFF
- **AND** la liberación siguiente aplica OFF

Cubierto por `[env:native]` (`test_photo_override`).

#### Scenario: Soltar con el display colgado

- **GIVEN** una prueba activa y la tarea Comm del display atascada
- **WHEN** se suelta
- **THEN** la lámpara se apaga en el siguiente tick sin ninguna trama

La lógica pura está cubierta por `[env:native]`; el tick y el apagado físico son verificación manual en banco.

#### Scenario: Escritura del PWM serializada

- **GIVEN** el lazo de intensidad entre la comprobación de `in3.phototherapy` y la escritura del PWM
- **WHEN** la tarea Comm apaga la lámpara en ese intervalo
- **THEN** la escritura tardía del lazo no reenciende la lámpara

Verificación manual en banco (`photoPwmRefresh()` bajo spinlock; no entra en host). La prohibición de fade LEDC en ese canal está documentada en `board.h` y no se comprueba en tiempo de ejecución.

#### Scenario: La prueba no cuenta como horas de fototerapia

- **GIVEN** una prueba de depuración activa con la lámpara forzada a ON
- **WHEN** `timeTrackHandler` suma el tiempo de uso
- **THEN** no se incrementan las horas de fototerapia

Verificación manual: `updateData.cpp` no entra en host. La telemetría `Phototherapy_active` sí publica la prueba como activa (es el estado real de la lámpara).

## ADDED Requirements

### Requirement: La caducidad del temporizador de fototerapia no se deshace con una trama rezagada

Tras vencer el temporizador de fototerapia, la placa SHALL ignorar un `photo=1` sin minutos (`photoMin<=0`) del display hasta ver un `photo=0`, tanto dentro como fuera de una prueba de depuración, porque es una trama que el display tenía en vuelo, o retenida hasta 10 s por su guarda de confirmación, de antes de adoptar el apagado. Un `photo=1` con minutos (`photoMin>0`) SHALL aceptarse siempre y cerrar el cerrojo, y un `photo=0` SHALL cerrarlo. Todo lo que sigue en el parser (contabilidad, temporizador, orden aplicada y guardada) SHALL usar el valor ya filtrado.

#### Scenario: Trama rezagada tras la caducidad

- **GIVEN** el temporizador acaba de vencer y la lámpara está apagada
- **WHEN** llega una trama con `photo=1` y `photoMin=0`
- **THEN** la lámpara sigue apagada
- **AND** una trama idéntica posterior tampoco la enciende

Cubierto por `[env:native]` (`test_photo_expiry_latch`).

#### Scenario: El display adopta el apagado y se inicia una sesión con temporizador

- **GIVEN** el temporizador vencido y una trama rezagada ya ignorada
- **WHEN** llega `photo=0` y después `photo=1` con `photoMin>0`
- **THEN** la lámpara se enciende

Cubierto por `[env:native]`.

#### Scenario: Sesión con temporizador sin haber visto el cero

- **GIVEN** el temporizador vencido
- **WHEN** llega `photo=1` con `photoMin>0`
- **THEN** se acepta y se cierra el cerrojo

Cubierto por `[env:native]`.

#### Scenario: Fuera de la prueba de depuración

- **GIVEN** ninguna prueba de depuración activa y el temporizador recién vencido
- **WHEN** llega la trama rezagada con `photo=1` y `photoMin=0`
- **THEN** la lámpara no se reenciende

Lógica pura cubierta por `[env:native]`; el parser de `CommTask.cpp` es verificación manual en banco.
