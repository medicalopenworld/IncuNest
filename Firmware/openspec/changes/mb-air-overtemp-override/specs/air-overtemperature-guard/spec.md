## ADDED Requirements

### Requirement: El aire por encima de 37 °C se avisa aunque la consigna se esté siguiendo bien

Todas las alarmas térmicas actuales son relativas a la consigna, así que una consigna alta seguida correctamente no produce ningún aviso. El equipo SHALL declarar una condición de sobretemperatura de aire cuando la temperatura de aire supere **37 °C**, **con independencia de la consigna y de la desviación respecto a ella**. La condición SHALL ser de prioridad MEDIA, SHALL no ser latching y SHALL no cortar el calefactor: es un aviso recuperable, no una protección. El corte térmico de 38 °C SHALL seguir existiendo sin cambios.

La condición SHALL declararse con histéresis, de modo que no se retire hasta caer claramente por debajo del umbral, y SHALL no producir anuncios intermitentes alrededor del límite (known issue nº 1, `Firmware/docs/known_issues.md`).

#### Scenario: Consigna alta seguida correctamente

- **WHEN** el modo es aire, la consigna es 36.5 °C y la temperatura de aire es 37.4 °C
- **AND** la desviación respecto a la consigna es de solo +0.9 °C, por debajo del umbral de desviación
- **THEN** se declara la sobretemperatura de aire y se anuncia
- **AND** no se declara ninguna alarma de desviación ni el corte térmico

Lógica pura cubierta por `[env:native]`; el comportamiento completo en banco es verificación manual.

#### Scenario: Fuente de calor externa con consigna baja

- **WHEN** el modo es aire, la consigna es 35 °C y una fuente externa lleva el aire a 37.5 °C
- **THEN** se declara la sobretemperatura de aire
- **AND** el corte térmico de 38 °C no se dispara

Verificación manual en banco con una fuente de calor externa.

#### Scenario: La condición se retira con histéresis

- **WHEN** la condición está declarada y la temperatura de aire baja por debajo del umbral
- **THEN** la condición no se retira hasta caer por debajo del umbral menos la histéresis
- **AND** no se producen anuncios intermitentes durante el descenso

Cubierto por `[env:native]`.

### Requirement: Una consigna de aire por encima de 37 °C exige un gesto deliberado

Poner el aire a 37 °C o más es legítimo para un gran prematuro, así que una alarma seca a 37 °C sonaría en uso correcto. El equipo SHALL topar la consigna de aire en **37 °C** salvo que el operador active explícitamente el override, que SHALL exigir un gesto deliberado — difícil de hacer sin querer y fácil de hacer a propósito con guantes. El tope SHALL aplicarse en **todos** los puntos de entrada de consigna: el enlace con el HMI, la página de configuración por WiFi y la carga desde NVS al arrancar.

#### Scenario: Consigna por encima de 37 sin override

- **WHEN** se intenta fijar una consigna de aire de 37.5 °C sin override activo, por cualquiera de los puntos de entrada
- **THEN** la consigna queda en 37 °C

Cubierto por `[env:native]` en la parte de acotado; los puntos de entrada son verificación manual.

#### Scenario: Consigna por encima de 37 con override

- **WHEN** el operador activa el override con el gesto deliberado y fija 37.5 °C
- **THEN** la consigna se acepta

Verificación manual en banco.

### Requirement: El override es visible de forma permanente mientras dure

Un equipo en override que no se anuncia se hereda entre turnos sin que nadie lo sepa. Mientras el override esté activo, la pantalla SHALL indicarlo de forma permanente y visible sin navegar a ninguna pantalla concreta. La alarma de sobretemperatura de aire SHALL no anunciarse mientras el override esté activo — avisaría de lo que el clínico acaba de pedir a propósito —, pero la indicación de override SHALL permanecer.

Al retirarse el override, el equipo SHALL reevaluar la condición de sobretemperatura de inmediato, sin esperar a un nuevo cruce del umbral.

#### Scenario: Sobretemperatura silenciada bajo override

- **WHEN** el override está activo, la consigna es 37.5 °C y el aire está a 37.5 °C
- **THEN** la sobretemperatura de aire no se anuncia
- **AND** la indicación permanente de override sigue visible

Lógica pura cubierta por `[env:native]`; la indicación en pantalla es verificación manual.

#### Scenario: Quitar el override reevalúa la condición

- **WHEN** el override se retira con el aire todavía por encima de 37 °C
- **THEN** la sobretemperatura de aire se declara de inmediato, sin esperar a otro cruce del umbral

Cubierto por `[env:native]`.
