## MODIFIED Requirements

### Requirement: Aviso de servicio en el HMI
El HMI SHALL mostrar las horas desde servicio y el umbral de cada actuador en la
vista de servicio del diálogo de mantenimiento, marcando los vencidos. SHALL
abrir esa vista por sí solo al desbloquear la pantalla cuando haya algún
actuador vencido, sin aviso de limpieza pendiente, sin "MAS TARDE" vigente
(24 h) y con el interruptor propio de servicio activado; NO SHALL depender del
interruptor AVISOS de la limpieza. El aviso NO SHALL sonar ni crear una alarma, y
SHALL ceder ante cualquier alarma activa, la pérdida de enlace con la placa, el
wizard del bebé y cualquier otro diálogo modal, y SHALL NO abrirse en modo
formación.

#### Scenario: Actuador vencido al desbloquear
- **WHEN** el humidificador está vencido, no hay alarmas ni diálogos abiertos y el operador desbloquea
- **THEN** se abre la vista de servicio con el humidificador marcado

#### Scenario: Alarma activa
- **WHEN** hay una alarma activa y un actuador vencido
- **THEN** el aviso de servicio no se abre

#### Scenario: AVISOS de limpieza apagado
- **WHEN** el interruptor AVISOS de la limpieza está apagado y el interruptor de servicio activado
- **AND** un actuador está vencido
- **THEN** el aviso de servicio se abre igualmente

## ADDED Requirements

### Requirement: Interruptor propio del aviso de servicio
El HMI SHALL ofrecer en Ajustes > Mantenimiento un interruptor propio del aviso
de servicio, persistido en NVS (`svc_en`), con valor por defecto ACTIVADO también
cuando la NVS está virgen. Apagado, SHALL NO abrirse el aviso por sí solo, pero
la vista SERVICIO SHALL seguir accesible a mano. En modo formación el cambio SHALL
rechazarse con el toast estándar sin escribir NVS.

#### Scenario: NVS virgen
- **WHEN** la unidad arranca sin la clave `svc_en`
- **THEN** el interruptor de servicio está activado

#### Scenario: Interruptor apagado
- **WHEN** el interruptor de servicio está apagado y un actuador está vencido
- **THEN** el aviso no se abre al desbloquear
- **AND** el botón SERVICIO sigue mostrando la vista

#### Scenario: Cambio en formación
- **WHEN** el operador toca el interruptor en modo formación
- **THEN** se muestra el toast de "no disponible en formación" y el valor no cambia
