# hmi-brightness Specification

## Purpose
TBD - created by archiving change hmi-brightness. Update Purpose after archive.
## Requirements
### Requirement: Nivel de brillo de usuario

El HMI SHALL permitir fijar el brillo de la retroiluminación entre 20 % y
100 % desde Ajustes > Modos, con un slider. El valor SHALL persistirse en NVS
(`hmi_cfg`, clave `bl_pct`) y restaurarse en el arranque. El valor por defecto
SHALL ser 100 %, que escribe el mismo byte que el firmware anterior
(`DISPLAY_BL_ON_VALUE`). Un valor leído de NVS fuera de `[20, 100]` SHALL
sanearse a ese rango; nunca SHALL resultar en una pantalla apagada.

#### Scenario: Unidad sin configuración previa
- **WHEN** el HMI arranca y NVS no tiene la clave `bl_pct`
- **THEN** el nivel de usuario es 100 %
- **AND** el byte escrito al controlador es `DISPLAY_BL_ON_VALUE`

*Cobertura: la conversión y el saneado, `[env:native]` de Display_HMI. El
arranque en la unidad es verificación manual.*

#### Scenario: Valor corrupto en NVS
- **WHEN** `bl_pct` contiene 0 o un valor mayor que 100
- **THEN** el nivel efectivo se limita a `[20, 100]`

*Cobertura: `[env:native]` de Display_HMI.*

#### Scenario: El usuario ajusta el slider
- **WHEN** el usuario arrastra el slider de brillo en Ajustes > Modos
- **THEN** el brillo cambia en vivo mientras arrastra
- **AND** el valor se escribe en NVS al soltar, fuera de `LVGL_Lock`

*Cobertura: manual en banco (la UI y la escritura NVS no tienen entorno de
test).*

### Requirement: Modo noche

El HMI SHALL ofrecer un switch "modo noche", desactivado por defecto y
persistido en NVS (`bl_night`). Con el modo noche activo, mientras la pantalla
de bloqueo esté cargada y hayan pasado al menos 30 s desde el último toque, el
brillo SHALL bajar a `min(nivel de usuario, 20 %)`. Con el modo noche
desactivado el brillo nunca SHALL bajar del nivel de usuario por inactividad.

#### Scenario: Atenuación tras 30 s bloqueada
- **GIVEN** el modo noche activo y el nivel de usuario 100 %
- **WHEN** la pantalla de bloqueo lleva cargada y sin toques hasta cumplir 30 s
  de inactividad
- **THEN** el brillo objetivo es 20 %

*Cobertura: política pura, `[env:native]` de Display_HMI; byte real en banco.*

#### Scenario: Modo noche desactivado
- **GIVEN** el modo noche desactivado
- **WHEN** la pantalla está bloqueada y pasan más de 30 s sin toques
- **THEN** el brillo objetivo sigue siendo el nivel de usuario

*Cobertura: `[env:native]` de Display_HMI.*

#### Scenario: Aún sin 30 s sin toques
- **GIVEN** el modo noche activo y la pantalla bloqueada
- **WHEN** hay menos de 30 s de inactividad
- **THEN** el brillo objetivo es el nivel de usuario

*Cobertura: `[env:native]` de Display_HMI.*

#### Scenario: Pantalla no bloqueada
- **GIVEN** el modo noche activo
- **WHEN** la pantalla activa no es la de bloqueo, con cualquier inactividad
- **THEN** el brillo objetivo es el nivel de usuario

*Cobertura: `[env:native]` de Display_HMI.*

### Requirement: Restauración por toque

Cualquier toque SHALL devolver el brillo al nivel de usuario, tanto en la
pantalla de bloqueo como durante el desbloqueo, en la siguiente pasada de
`UI_Task`. El HMI SHALL usar para ello la inactividad de LVGL como fuente
única.

#### Scenario: Toque con la pantalla atenuada
- **GIVEN** el modo noche activo y el brillo atenuado
- **WHEN** el usuario toca la pantalla
- **THEN** la inactividad vuelve a 0 y el brillo objetivo es el nivel de usuario

*Cobertura: la política, `[env:native]`; el tiempo de reacción real, manual en
banco.*

### Requirement: Alarma activa fuerza brillo pleno

El brillo objetivo SHALL ser 100 % mientras cualquier alarma esté activa o el
enlace con la motherBoard esté perdido, con independencia del modo noche, de
la pantalla bloqueada y del nivel de usuario. Al cesar la última, el
brillo SHALL volver al valor que correspondería sin alarma. Nunca SHALL haber
una pantalla atenuada con una alarma activa.

#### Scenario: Alarma durante la atenuación
- **GIVEN** el modo noche activo, bloqueada, atenuada
- **WHEN** salta una alarma
- **THEN** el brillo objetivo pasa a 100 % sin esperar a ningún toque

*Cobertura: `[env:native]` de Display_HMI.*

#### Scenario: Alarma con nivel de usuario bajo
- **GIVEN** el nivel de usuario 20 %
- **WHEN** hay una alarma activa
- **THEN** el brillo objetivo es 100 %

*Cobertura: `[env:native]` de Display_HMI.*

#### Scenario: Cese de la alarma
- **GIVEN** una alarma activa con la pantalla bloqueada, modo noche activo y
  más de 30 s sin toques
- **WHEN** la alarma cesa
- **THEN** el brillo objetivo es el nivel noche

*Cobertura: `[env:native]` de Display_HMI.*

### Requirement: Escritura segura al controlador

El HMI SHALL escribir el brillo al STC8H1K28 solo fuera de `LVGL_Lock`, SHALL
reintentar una escritura fallida en la siguiente pasada sin dar el nivel por
aplicado, y NUNCA SHALL escribir como brillo un byte mayor que 240 (el 245 es
"apagado", solo vía `Backlight_Off()`, y los bytes >= 246 son opcodes de
zumbador y altavoz). Con `DISPLAY_BL_LEVELS_SUPPORTED` a 0, todo nivel mayor
que 0 SHALL escribir `DISPLAY_BL_ON_VALUE`.

#### Scenario: Conversión monótona y acotada
- **WHEN** se convierte cada porcentaje de 0 a 255 a byte
- **THEN** el byte está en `[10, 240]`
- **AND** a más porcentaje, menos o igual byte (más brillo)
- **AND** 100 % da `DISPLAY_BL_ON_VALUE`

*Cobertura: `[env:native]` de Display_HMI.*

#### Scenario: Controlador sin soporte de niveles
- **GIVEN** `DISPLAY_BL_LEVELS_SUPPORTED` a 0
- **WHEN** se convierte cualquier porcentaje mayor que 0
- **THEN** el byte es `DISPLAY_BL_ON_VALUE`

*Cobertura: `[env:native]` de Display_HMI.*

#### Scenario: Escritura I2C fallida
- **WHEN** la escritura del nuevo nivel no es ACKeada
- **THEN** el HMI mantiene el nivel anterior como aplicado y reintenta en la
  siguiente pasada

*Cobertura: manual (requiere forzar un NACK en el bus del banco).*

#### Scenario: Apagado por pwroff
- **WHEN** la cuenta atrás de apagado llega a 0
- **THEN** el HMI escribe el byte de apagado (245) y se detiene, con
  independencia del nivel de usuario y del modo noche

*Cobertura: manual en banco.*

### Requirement: Cadenas en cuatro idiomas

Las dos filas nuevas de Ajustes SHALL tener texto en español, inglés, francés
y portugués (pt-PT) en el catálogo `i18n_strings.def`, y SHALL actualizarse al
cambiar de idioma.

#### Scenario: Cambio de idioma
- **WHEN** el usuario cambia de idioma en Ajustes
- **THEN** las etiquetas de brillo y modo noche se muestran en el idioma elegido

*Cobertura: manual en banco.*

