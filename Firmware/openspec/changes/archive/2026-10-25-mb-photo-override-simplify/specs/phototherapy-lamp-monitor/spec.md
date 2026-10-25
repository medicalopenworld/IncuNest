## MODIFIED Requirements

### Requirement: CTRL,STATE informa del estado real de la lámpara, no del eco de la orden

El campo de fototerapia de `CTRL,STATE` (campo 8, `photo`) SHALL reflejar el estado que la placa tiene **aplicado** a la lámpara (`in3.phototherapy`), y SHALL NOT repetir la última orden recibida del display. **Única excepción**: mientras una prueba de fototerapia de depuración (`PHOTO,1`) esté activa, el campo SHALL emitir la **última orden del display** y SHALL NOT emitir el ON forzado, para que el display (que adopta este campo y lo devuelve en su keepalive) nunca adopte el ON de depuración. El ON de la prueba SHALL ser visible en `/debug/state` (`ctl.photo_dbg`) y en el log.

#### Scenario: Fuera de una prueba el campo es el estado aplicado

- **GIVEN** ninguna prueba de depuración activa
- **WHEN** la lámpara está aplicada ON u OFF
- **THEN** el campo 8 vale 1 u 0 respectivamente

Cubierto por `[env:native]` (`test_photo_override`, `photo_override_state_field`). El camino de `CommTask.cpp` es verificación manual.

#### Scenario: Durante la prueba el campo emite la orden del display

- **GIVEN** una prueba de depuración activa con la lámpara forzada a ON y la última orden del display en OFF
- **WHEN** la placa compone `CTRL,STATE`
- **THEN** el campo 8 vale 0
- **AND** si el operador ordena ON desde la pantalla, vale 1

Cubierto por `[env:native]` (`photo_override_state_field`). La pantalla en banco es verificación manual.

#### Scenario: Orden del display aún no aplicada

- **GIVEN** la lámpara apagada
- **WHEN** el display pide fototerapia ON y un `CTRL,STATE` sale antes de que la placa aplique la orden
- **THEN** ese `CTRL,STATE` lleva el campo 8 a 0 (estado real)
- **AND** la pantalla mantiene el ON local hasta que la placa lo confirma, gracias a la guarda de confirmación del display

Verificación manual en banco.

## REMOVED Requirements

### Requirement: El apagado del encendido de depuración se confirma, no se espera

**Reason**: la liberación por confirmación (valor del display == valor real, mínimo 2 s, respaldo de 60 s) tenía tres fallos: con el temporizador vencido durante la liberación el ON se auto-confirmaba y se guardaba como sesión continua, un OFF del operador en esa ventana se perdía, y una trama `CTRL,STATE` rancia procesada tarde por el anillo de RX del display seguía colándose. Todos nacían de emitir el ON forzado en el campo 8.

**Migration**: el campo 8 deja de emitir el ON forzado durante la prueba y soltar devuelve el control en el acto (ver el requisito nuevo). No hay ventana, confirmación, mínimo ni respaldo.

## ADDED Requirements

### Requirement: Soltar la prueba de depuración devuelve el control al display en el acto

Mientras la prueba de fototerapia de depuración esté activa, la placa SHALL forzar la lámpara a ON, SHALL registrar las órdenes del display (y la caducidad del temporizador de fototerapia como orden de apagado) sin aplicarlas, y SHALL NOT guardar nada en NVS. Al soltarla (`PHOTO,0` o apagar el modo depuración), la placa SHALL aplicar en el siguiente tick periódico, por el camino normal de una orden, la **última orden del display** registrada (la del operador manda; si no hubo ninguna, la que había al empezar), SHALL NOT esperar ninguna trama del display, y SHALL NOT persistir ese valor. No SHALL haber ventana de gracia, confirmación, mínimo ni plazo de respaldo. La escritura del PWM de la lámpara SHALL serializarse entre el lazo de intensidad y el camino de aplicación para que un apagado no pueda ser pisado por una escritura tardía del lazo. La contabilidad de exposición por bebé SHALL NOT contar la prueba ni una lámpara con `ALARM_PHOTO_LAMP_FAULT` declarada.

#### Scenario: Real OFF, prueba y soltar sin tramas

- **GIVEN** la fototerapia real apagada y una prueba activa
- **WHEN** se suelta y no llega ninguna trama del display
- **THEN** `photo_override_take_release()` entrega OFF una sola vez
- **AND** la prueba deja de forzar y se puede guardar

Cubierto por `[env:native]` (`test_photo_override`).

#### Scenario: Real ON, vence el temporizador durante la prueba

- **GIVEN** la fototerapia real encendida y una prueba activa
- **WHEN** vence el temporizador de fototerapia y después se suelta
- **THEN** al soltar se entrega OFF y no se persiste ningún ON

Cubierto por `[env:native]`.

#### Scenario: El operador manda OFF durante la prueba

- **GIVEN** la fototerapia real encendida y una prueba activa
- **WHEN** el display ordena OFF y después se suelta
- **THEN** al soltar se entrega OFF

Cubierto por `[env:native]`.

#### Scenario: Soltar con el display colgado

- **GIVEN** una prueba activa y la tarea Comm del display atascada
- **WHEN** se suelta
- **THEN** la lámpara se apaga en el siguiente tick sin ninguna trama

La lógica pura está cubierta por `[env:native]`; el tick y el apagado físico son verificación manual en banco.

#### Scenario: Escritura del PWM serializada

- **GIVEN** el lazo de intensidad entre la comprobación de `in3.phototherapy` y la escritura del PWM
- **WHEN** la tarea Comm apaga la lámpara en ese intervalo
- **THEN** la escritura tardía del lazo no reenciende la lámpara

Verificación manual en banco (`photoPwmRefresh()` bajo spinlock; no entra en host).
