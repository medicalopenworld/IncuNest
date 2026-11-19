# ctrl-state-active-seq Specification

## Purpose
TBD - created by archiving change shared-patient-identity-confirm. Update Purpose after archive.
## Requirements
### Requirement: CTRL,STATE informa del bebé al mando

La motherBoard SHALL incluir en cada `CTRL,STATE` un campo 24, `activeSeq`, con
el valor de `babyStore_getActiveSeq()`: el `seq` del perfil que tiene al mando
mientras alguna terapia corre, o `0` si no tiene ninguno. El campo SHALL ir al
final, tras `latchedBitmask`, y el búfer de la línea SHALL tener holgura
suficiente para que ningún valor lo trunque.

El HMI SHALL parsearlo con el resto de la línea y SHALL distinguir «la placa
dice 0» de «la placa no lo manda» (placa anterior a este campo, línea con menos
de 24 campos): en el segundo caso el valor es **desconocido**, no `0`. Una línea
con campos de menos SHALL seguir aplicando todos los campos que sí trae.

#### Scenario: Placa con el bebé al mando
- **WHEN** la placa tiene la fototerapia en marcha para el bebé `seq` 7
- **THEN** el `CTRL,STATE` termina en `...,0x<latched>,7`

#### Scenario: Placa sin bebé al mando
- **WHEN** no hay ninguna terapia en marcha
- **THEN** el campo 24 vale `0`

#### Scenario: Placa antigua
- **WHEN** llega un `CTRL,STATE` de 23 campos
- **THEN** el HMI aplica los 23 campos
- **AND** `activeSeq` queda desconocido, no `0`

#### Scenario: HMI antiguo
- **WHEN** un HMI anterior recibe la línea de 24 campos
- **THEN** ignora el sobrante y funciona como antes

*Cobertura: la composición de la línea y el `sscanf` son verificación manual en
banco (no hay entorno de test para `tasks/`); el HMI se compila y la placa
también. La semántica de «desconocido» se cubre en los tests de decisión del
HMI.*

