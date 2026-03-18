# Requisitos de firmware – Selección de red WiFi desde SSID

## 1. Objetivo

Permitir al usuario seleccionar una red WiFi desde un listado de redes detectadas por el HMI, o introducir manualmente un SSID, mejorando la usabilidad del sistema en la configuración de conectividad.

---

## 2. Alcance

Este documento cubre:

- detección de redes WiFi desde el HMI,
- visualización de redes disponibles,
- interacción del usuario con el campo SSID,
- entrada manual de SSID.

No cubre:
- autenticación avanzada,
- gestión de contraseñas,
- conexión automática a redes guardadas,
- lógica de backend WiFi en la Motherboard.

---

## 3. Requisitos funcionales

### RF-WIFI-001
El sistema debe permitir detectar redes WiFi disponibles desde el HMI.

### RF-WIFI-002
Al pulsar sobre el campo SSID en la pantalla de settings, debe mostrarse un desplegable (dropdown) con las redes detectadas.

### RF-WIFI-003
El desplegable debe listar:
- nombre de la red (SSID),
- opcionalmente intensidad de señal (RSSI).

### RF-WIFI-004
El usuario debe poder seleccionar una red del listado.

### RF-WIFI-005
Al seleccionar una red:
- el SSID debe rellenarse automáticamente en el campo correspondiente.

### RF-WIFI-006
El desplegable debe incluir una opción adicional:
- "Introducir manualmente" o equivalente.

### RF-WIFI-007
Si el usuario selecciona la opción manual:
- debe mostrarse un teclado o input para introducir el SSID.

### RF-WIFI-008
El usuario debe poder editar manualmente el SSID en cualquier momento.

---

## 4. Comportamiento de la UI

### UI-WIFI-001
El desplegable debe aparecer únicamente al interactuar con el campo SSID.

### UI-WIFI-002
El desplegable debe cerrarse automáticamente cuando:
- el usuario selecciona una red,
- el usuario cancela,
- el usuario pulsa fuera del componente.

### UI-WIFI-003
El sistema debe evitar bloquear la UI durante el escaneo de redes.

### UI-WIFI-004
Debe indicarse visualmente el estado de escaneo (por ejemplo: "Buscando redes...").

### UI-WIFI-005
Si no se detectan redes:
- debe mostrarse un mensaje informativo.

---

## 5. Arquitectura

### ARQ-WIFI-001
La detección de redes WiFi debe ejecutarse en el HMI.

### ARQ-WIFI-002
La UI debe consumir una lista de redes proporcionada por el módulo de WiFi del HMI.

### ARQ-WIFI-003
La lógica de conexión (si existe) debe mantenerse desacoplada de la UI.

### ARQ-WIFI-004
El listado de redes debe almacenarse temporalmente en memoria del HMI.

---

## 6. Rendimiento y comportamiento

### RF-WIFI-009
El escaneo de redes no debe bloquear tareas críticas del sistema (UI, comunicación).

### RF-WIFI-010
El escaneo debe ejecutarse de forma asíncrona.

### RF-WIFI-011
Debe evitarse realizar escaneos continuos innecesarios.

---

## 7. Requisitos de interacción

### UI-WIFI-006
El usuario debe poder:
- seleccionar una red existente,
- introducir una red manualmente.

### UI-WIFI-007
La transición entre selección automática y manual debe ser clara.

### UI-WIFI-008
El campo SSID debe reflejar siempre el valor seleccionado o introducido.

---

## 8. Requisitos de seguridad

### RS-WIFI-001
El sistema no debe guardar automáticamente redes sin confirmación del usuario.

### RS-WIFI-002
La selección de red no debe provocar reinicios ni bloqueos.

---

## 9. Criterios de aceptación

### CA-WIFI-001
Al pulsar el campo SSID aparece un listado de redes disponibles.

### CA-WIFI-002
El usuario puede seleccionar una red y el campo se rellena automáticamente.

### CA-WIFI-003
El usuario puede introducir manualmente un SSID.

### CA-WIFI-004
La UI no se bloquea durante el escaneo.

### CA-WIFI-005
El sistema mantiene su estabilidad tras la implementación.

---

## 10. Verificación

- Compilar firmware
- Subir a dispositivo
- Acceder a settings → WiFi
- Pulsar campo SSID

Validar:
- aparece dropdown
- se listan redes
- se puede seleccionar una
- se puede introducir manualmente