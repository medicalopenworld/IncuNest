# Requisitos de firmware – Frecuencia del buzzer de alarmas

## 1. Objetivo

Modificar la frecuencia del buzzer de alarmas para que emita un tono de aproximadamente **720 Hz**, sustituyendo la frecuencia actual (~2000 Hz), mejorando la percepción auditiva y adecuación clínica.

---

## 2. Alcance

Este documento cubre:

- generación de señal del buzzer,
- configuración de frecuencia,
- impacto en alarmas existentes.

No cubre:
- lógica de activación de alarmas,
- UI,
- sincronización con HMI,
- patrones de sonido complejos (melodías).

---

## 3. Requisitos funcionales

### RF-BUZZ-001
El buzzer de alarmas debe emitir un tono con frecuencia nominal de **720 Hz**.

### RF-BUZZ-002
Se permite una tolerancia de ±5% en la frecuencia generada.

### RF-BUZZ-003
La frecuencia actual (~2000 Hz) debe ser sustituida por la nueva frecuencia en todos los casos de activación del buzzer.

### RF-BUZZ-004
El cambio de frecuencia no debe alterar:
- la lógica de activación de alarmas,
- la duración de los sonidos,
- los patrones de repetición existentes.

---

## 4. Arquitectura y ubicación

### ARQ-BUZZ-001
La generación del buzzer se realiza en la **Motherboard**, dentro de la task dedicada (`buzzer_Task`). :contentReference[oaicite:1]{index=1}

### ARQ-BUZZ-002
La frecuencia del buzzer debe configurarse en el módulo donde se genera la señal PWM o toggling digital.

### ARQ-BUZZ-003
La frecuencia no debe depender de la HMI ni de comandos externos.

---

## 5. Implementación

### RF-BUZZ-005
La frecuencia del buzzer debe ser definida mediante una constante o parámetro configurable, por ejemplo:

```cpp
#define BUZZER_FREQUENCY_HZ 720

