## ADDED Requirements

### Requirement: Sexo al nacer del bebé

El HMI SHALL preguntar el sexo al nacer de un bebé nuevo (NIÑO / NIÑA / NO CONSTA) y, tras el `CTRL,PROFILE_ACK` de su `PROFILE_NEW`, SHALL enviar `HMI,PROFILE_SEX,<seq>,<sex>` con códigos ISO/IEC 5218 (`1` niño, `2` niña). Con NO CONSTA, en modo formación o para un `seq` de práctica NO SHALL enviar nada. La motherBoard SHALL guardarlo en el slot activo sin cambiar el tamaño del blob NVS (`sizeof(BabyProfile) == 64`), SHALL ignorar un `seq` desconocido y cualquier `sex` fuera de 0..2, y NO SHALL contestar. SHALL publicarlo como client attribute `baby_sex`, presente siempre (también `0`, también con la incubadora vacía).

#### Scenario: Niña registrada desde el asistente
- **WHEN** la enfermera elige NIÑA y la placa contesta `CTRL,PROFILE_ACK,17`
- **THEN** el HMI envía `HMI,PROFILE_SEX,17,2` antes de `PROFILE_WEIGHT` y los atributos publican `baby_sex: 2`

#### Scenario: No consta
- **WHEN** la enfermera elige NO CONSTA
- **THEN** no sale `PROFILE_SEX` y los atributos publican `baby_sex: 0`

#### Scenario: Placa antigua
- **WHEN** un HMI nuevo envía `PROFILE_SEX` a una placa sin soporte
- **THEN** la placa descarta la línea con un log y el registro del bebé no se ve afectado

#### Scenario: Actualización con bebés activos
- **WHEN** se actualiza la placa con tres bebés activos guardados por el firmware anterior
- **THEN** los tres siguen en sus slots y su sexo es `0`

#### Scenario: Siguiente bebé
- **WHEN** se registra un bebé con `baby_sex: 2` y después otro sin sexo
- **THEN** los atributos del segundo llevan `baby_sex: 0`, no heredan el `2`
