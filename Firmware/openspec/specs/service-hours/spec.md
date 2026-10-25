# service-hours Specification

## Purpose
TBD - created by archiving change shared-service-hours. Update Purpose after archive.
## Requirements
### Requirement: Horas desde servicio por actuador
La motherBoard SHALL calcular, para cada uno de los cuatro actuadores con vida
útil acotada (calefactor, ventilador, humidificador, fototerapia), las horas de
uso desde el último servicio registrado como `horas acumuladas − base`, donde la
base es el valor del contador de vida guardado en NVS al registrar el servicio.
Los contadores de vida útil SHALL seguir acumulándose y publicándose sin
modificación; registrar un servicio NO SHALL reiniciarlos.

Si el contador de vida es menor que la base (contador reiniciado por un borrado
de NVS o una migración), las horas desde servicio SHALL ser las del contador, no
un valor negativo ni un desbordamiento.

#### Scenario: Servicio registrado a mitad de vida
- **WHEN** el ventilador lleva 4 200,0 h de vida y se registra su servicio
- **AND** después acumula 30,5 h más
- **THEN** las horas desde servicio del ventilador son 30
- **AND** el contador de vida del ventilador sigue publicándose como 4 230,5 h

#### Scenario: Contador de vida reiniciado
- **WHEN** la base guardada es 4 200 h y el contador de vida vale 12,0 h
- **THEN** las horas desde servicio son 12

### Requirement: Un actuador vence al alcanzar su umbral
Cada actuador SHALL tener un umbral de servicio compilado, en horas. Un
actuador SHALL considerarse vencido cuando sus horas desde servicio sean iguales
o mayores que su umbral. Un umbral de 0 SHALL desactivar el aviso de ese
actuador. Los umbrales provisionales SHALL ser: calefactor 5 000 h, ventilador
10 000 h, humidificador 2 000 h, fototerapia 10 000 h.

#### Scenario: Umbral alcanzado
- **WHEN** las horas desde servicio del humidificador son 2 000 y su umbral 2 000
- **THEN** el humidificador está vencido

#### Scenario: Umbral desactivado
- **WHEN** el umbral de un actuador es 0
- **THEN** ese actuador nunca está vencido

### Requirement: Línea CTRL,SERVICE
La motherBoard SHALL informar al HMI con `CTRL,SERVICE,n{,id,horas,umbral}×n`
(n entradas; `id` 0 calefactor, 1 ventilador, 2 humidificador, 3 fototerapia;
`horas` y `umbral` en horas enteras). La línea SHALL enviarse al recibir
`HMI,UI_READY`, al recibir `HMI,SERVICE_REQ`, tras registrar un servicio y cada
hora. NO SHALL añadirse campos a `CTRL,STATE`. El parseo SHALL descartar en
silencio la línea entera si es malformada (n incoherente con el número de
campos, no numérico, id desconocido o repetido).

#### Scenario: Arranque del enlace
- **WHEN** la motherBoard recibe `HMI,UI_READY`
- **THEN** envía una línea `CTRL,SERVICE,4,...` con las cuatro entradas

#### Scenario: Línea truncada
- **WHEN** el HMI recibe una `CTRL,SERVICE` cuyo n es 4 pero trae solo tres entradas
- **THEN** descarta la línea y conserva el último estado válido

### Requirement: Registro de servicio hecho
La motherBoard SHALL atender `HMI,SERVICE_DONE,<id>` guardando en NVS (namespace
`mb_rt`) las horas de vida actuales del actuador como base y el epoch UTC del
servicio (0 si no hay hora válida), y SHALL contestar con un `CTRL,SERVICE`
actualizado. Un id fuera de rango SHALL descartarse sin efecto. Los valores
SHALL sobrevivir a un reinicio.

#### Scenario: Servicio registrado
- **WHEN** llega `HMI,SERVICE_DONE,1`
- **THEN** las horas desde servicio del ventilador pasan a 0
- **AND** la siguiente `CTRL,SERVICE` lo refleja

#### Scenario: Id inválido
- **WHEN** llega `HMI,SERVICE_DONE,9`
- **THEN** no cambia ninguna base ni se escribe NVS

### Requirement: Aviso de servicio en el HMI
El HMI SHALL mostrar las horas desde servicio y el umbral de cada actuador en la
vista de servicio del diálogo de mantenimiento, marcando los vencidos. SHALL
abrir esa vista por sí solo al desbloquear la pantalla cuando haya algún
actuador vencido, sin aviso de limpieza pendiente, y sin "MAS TARDE" vigente
(24 h). El aviso NO SHALL sonar ni crear una alarma, y SHALL ceder ante
cualquier alarma activa, la pérdida de enlace con la placa, el wizard del bebé y
cualquier otro diálogo modal, y SHALL NO abrirse en modo formación.

#### Scenario: Actuador vencido al desbloquear
- **WHEN** el humidificador está vencido, no hay alarmas ni diálogos abiertos y el operador desbloquea
- **THEN** se abre la vista de servicio con el humidificador marcado

#### Scenario: Alarma activa
- **WHEN** hay una alarma activa y un actuador vencido
- **THEN** el aviso de servicio no se abre

### Requirement: Registrar servicio exige confirmación y no se permite en formación
En el HMI, registrar un servicio SHALL requerir dos pasos (HECHO y luego
CONFIRMAR) y SHALL NO ejecutarse en modo formación. No SHALL registrarse nada si
no hay enlace con la motherBoard.

#### Scenario: Un solo toque
- **WHEN** el operador pulsa HECHO una vez y no confirma
- **THEN** no se envía `HMI,SERVICE_DONE`

### Requirement: Telemetría de horas desde servicio
La motherBoard SHALL publicar `svc_heater_h`, `svc_fan_h`, `svc_hum_h` y
`svc_photo_h` en el grupo DIAG, sin superar el presupuesto de 112 campos por
publicación.

#### Scenario: Grupo DIAG activo
- **WHEN** el transporte tiene el grupo DIAG encendido
- **THEN** cada publicación de configuración incluye las cuatro claves

