# hmi-on-battery-indicator Specification

## Purpose
TBD - created by archiving change shared-mains-detector. Update Purpose after archive.
## Requirements
### Requirement: El heading muestra un indicador «en batería» mientras el aviso de corte de red está señalizado

El display SHALL mostrar un indicador persistente en el heading mientras el bit `ALARM_MAINS_INTERRUPTION` esté activo en `alarmBitmask` de `CTRL,STATE`: **solo el icono de batería** en el heading de la pantalla principal (dentro del botón del reloj, en la fila de la fecha y a su derecha, sin pisar la fecha ni la hora) e **icono y palabra corta** en la réplica del heading de la pantalla de bloqueo (slot de ayuda). SHALL ocultarlo cuando el bit no esté activo, cuando `alarmBitmask` tenga el valor nulo (`0xFFFFFFFF`) y mientras el enlace con la placa esté caído. No SHALL añadirse ningún campo al protocolo.

El indicador sigue a la condición señalizada, no a la presencia física de red: se retira cuando la placa confirma que la red ha vuelto.

#### Scenario: Corte de red con la pantalla principal

- **WHEN** llega un `CTRL,STATE` con el bit 7 de `alarmBitmask` a 1
- **THEN** el heading de la pantalla principal muestra el icono de batería
- **AND** el icono no pisa la fecha ni la hora del reloj en ningún idioma, tampoco con el texto «Sans heure» ni sin hora

Verificación manual con captura en ES y FR (Display_HMI no tiene entorno de test).

#### Scenario: Corte de red con la pantalla bloqueada

- **WHEN** el bit 7 está activo y la pantalla está bloqueada
- **THEN** la réplica del heading muestra el icono y la palabra, legibles sobre el fondo oscuro, en el hueco del botón de ayuda

Verificación manual.

#### Scenario: La red vuelve y el aviso termina

- **WHEN** el bit 7 pasa a 0
- **THEN** el indicador desaparece en ambas pantallas

Verificación manual.

#### Scenario: Sin enlace con la placa

- **WHEN** `Display_IsBoardLinkLost()` es verdadero
- **THEN** el indicador está oculto, porque no hay dato fresco del que fiarse

Verificación manual.

#### Scenario: Cambio de idioma con el indicador visible

- **WHEN** el idioma cambia con el indicador visible en la pantalla de bloqueo
- **THEN** la palabra pasa al nuevo idioma sin esperar a que cambie la alarma

Verificación manual, en ES, EN, FR y PT.

### Requirement: El refresco del indicador no hace E/S dentro del bloqueo de LVGL

El refresco del indicador SHALL limitarse a leer estado en memoria y tocar widgets de LVGL (ARQ-LOCK-001): ninguna escritura en NVS ni acceso I2C. SHALL repintar solo cuando cambie la señal o el idioma.

#### Scenario: Pasadas sin cambios

- **WHEN** el bucle de UI pasa repetidamente sin que cambie la señal ni el idioma
- **THEN** no se llama a `lv_label_set_text` ni a ninguna función de estilo del indicador

Verificación manual (revisión de código).

