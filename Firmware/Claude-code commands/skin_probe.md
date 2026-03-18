# Requisitos de firmware – Sonda de piel y modos de control

## 1. Objetivo

Permitir que la incubadora funcione correctamente en **modo aire** sin necesidad de sonda de temperatura de piel, y asegurar que el **modo piel** solo pueda activarse cuando la sonda esté conectada y sea válida.

---

## 2. Alcance

Este documento cubre:

- detección de sonda de piel,
- estados de la sonda,
- relación entre sonda y modos de control,
- restricciones en la UI,
- comportamiento ante desconexión durante modo piel.

No cubre:
- sensores de aire,
- Wi-Fi,
- especificación completa de alarmas.

---

## 3. Requisitos funcionales

### RF-SKIN-001
La incubadora debe poder funcionar en **modo aire** aunque la sonda de temperatura de piel no esté conectada.

### RF-SKIN-002
La ausencia de sonda de piel no debe impedir el arranque del equipo en modo aire.

### RF-SKIN-003
La ausencia de sonda de piel en modo aire no debe generar alarma acústica.

### RF-SKIN-004
La UI podrá mostrar la ausencia de sonda de piel como estado informativo no crítico.

### RF-SKIN-005
El sistema debe detectar el estado de conexión y validez de la sonda de piel.

### RF-SKIN-006
El sistema debe diferenciar al menos los siguientes estados:
- no conectada,
- conectada pendiente de validar,
- válida,
- inválida,
- lectura fuera de rango,
- desconectada durante funcionamiento,
- conexión inestable.

### RF-SKIN-007
El firmware no debe permitir activar el **modo piel** si la sonda no está conectada o no es válida.

### RF-SKIN-008
La restricción del modo piel debe aplicarse tanto en la lógica interna como en la UI.

### RF-SKIN-009
La UI no debe mostrar el modo piel como seleccionable si la sonda no es válida, o debe bloquear su confirmación de forma explícita.

### RF-SKIN-010
Cuando el usuario intente activar modo piel sin sonda válida, la UI debe mostrar un mensaje claro indicando que la sonda es obligatoria para ese modo.

---

## 4. Lógica de modos

### RF-SKIN-011
El sistema debe soportar al menos dos modos de control:
- modo aire,
- modo piel.

### RF-SKIN-012
El modo aire debe estar disponible independientemente del estado de la sonda de piel.

### RF-SKIN-013
El modo piel solo debe estar disponible cuando la sonda de piel esté en estado `válida`.

### RF-SKIN-014
Si la sonda deja de ser válida durante el modo piel, el sistema debe abandonar el control por piel.

### RF-SKIN-015
La salida de modo piel por fallo de sonda debe realizarse automáticamente.

### RF-SKIN-016
La transición desde modo piel tras fallo de sonda debe ir a un estado seguro definido por firmware.

### RF-SKIN-017
La política de recuperación tras reconexión de la sonda no debe reactivar automáticamente el modo piel sin confirmación explícita del usuario, salvo que se decida lo contrario de forma expresa.

---

## 5. Detección de fallos de sonda

### RF-SKIN-018
El sistema debe detectar:
- sonda ausente,
- lectura imposible,
- desconexión brusca,
- señal inestable o intermitente.

### RF-SKIN-019
Debe aplicarse lógica de antirrebote o validación temporal para evitar falsas detecciones provocadas por mal contacto del jack.

### RF-SKIN-020
El tiempo de antirrebote debe ser configurable.

### RF-SKIN-021
Una sonda solo debe marcarse como `válida` cuando cumpla los criterios eléctricos y/o térmicos definidos en el firmware.

### RF-SKIN-022
Una sonda con lectura fuera de rango no debe considerarse válida para activar el modo piel.

---

## 6. Requisitos de seguridad

### RS-SKIN-001
La ausencia de sonda de piel no debe afectar a la operación segura en modo aire.

### RS-SKIN-002
El control por piel nunca debe iniciarse con una sonda ausente o inválida.

### RS-SKIN-003
Si la sonda falla durante el modo piel, el sistema debe abandonar ese modo de forma segura.

### RS-SKIN-004
La pérdida de sonda durante modo piel debe considerarse condición de fallo relevante.

### RS-SKIN-005
La lógica de detección de sonda no debe depender exclusivamente de la interfaz de usuario.

---

## 7. Requisitos de interfaz de usuario

### UI-SKIN-001
La UI debe mostrar el modo actual del sistema.

### UI-SKIN-002
La UI debe indicar cuándo el modo piel no está disponible por ausencia o invalidez de la sonda.

### UI-SKIN-003
La UI debe impedir la activación del modo piel si no hay sonda válida.

### UI-SKIN-004
La UI debe informar claramente al usuario del motivo del bloqueo del modo piel.

### UI-SKIN-005
En modo aire, la ausencia de sonda puede mostrarse como texto informativo no crítico.

---

## 8. Requisitos de trazabilidad

### RT-SKIN-001
El sistema debe registrar:
- sonda conectada,
- sonda desconectada,
- sonda válida,
- sonda inválida,
- intento de activar modo piel sin sonda válida,
- entrada en modo piel,
- salida de modo piel por fallo de sonda.

### RT-SKIN-002
Cada evento debe incluir timestamp o referencia temporal equivalente.

---

## 9. Requisitos de arquitectura

### ARQ-SKIN-001
La detección de sonda debe implementarse en un módulo independiente de la UI.

### ARQ-SKIN-002
La lógica de disponibilidad de modos debe depender del estado interno validado de la sonda.

### ARQ-SKIN-003
La UI debe consultar el estado validado de la sonda, no deducirlo por su cuenta.

### ARQ-SKIN-004
Los parámetros de antirrebote, validación y rango deben centralizarse en configuración.

---

## 10. Parámetros configurables

- Tiempo de antirrebote de conexión/desconexión: `configurable`
- Rango válido de lectura de sonda: `configurable`
- Política de transición tras fallo de sonda en modo piel: `configurable`

---

## 11. Criterios de aceptación

### CA-SKIN-001
La incubadora funciona en modo aire sin sonda de piel.

### CA-SKIN-002
La ausencia de sonda no genera alarma acústica en modo aire.

### CA-SKIN-003
La UI no permite activar modo piel si la sonda no está conectada o no es válida.

### CA-SKIN-004
Si el usuario intenta activar modo piel sin sonda válida, la UI muestra un mensaje claro de bloqueo.

### CA-SKIN-005
Si la sonda falla durante modo piel, el sistema abandona ese modo y pasa a estado seguro.

### CA-SKIN-006
Los eventos relacionados con la sonda quedan registrados.




