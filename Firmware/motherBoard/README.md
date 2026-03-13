# IncuNest — Módulo Motherboard (Control)

Este módulo es el núcleo de control de la incubadora **IncuNest**, responsable de la seguridad del paciente y del mantenimiento de las condiciones ambientales.

## Hardware Principal

| Componente | Descripción |
|---|---|
| **MCU** | ESP32-S3 (WROOM-1 o similar) |
| **Sensores HumID** | Sensirion SHT40/SHT45 (I2C) |
| **Sensores Temp** | Sensirion STS3x (I2C) precisión médica |
| **Actuadores** | Control PWM para Calentador, Ventiladores y Fototerapia |
| **Humidificador** | Celda ultrasónica con control de nivel de agua |
| **Comunicación** | Módulos SIM800 (GPRS) y WiFi integrado |

## Estructura de Software

El firmware está organizado en módulos funcionales:

- `main.cpp`: Inicialización de periféricos y bucle principal coordinado.
- `CommTask.cpp`: Gestión de la comunicación USB Host con el display HMI. Implementa el handshake `UI_READY` y el bitmask de alarmas.
- `security.cpp`: Sistema de alarmas. Monitoriza fallos de sensores, sobretemperatura y desconexiones.
- `PID.cpp`: Algoritmos de control para estabilidad térmica y de humedad.
- `sensors.cpp`: Drivers y lógica de promediado de sensores médicos.
- `initHardware.cpp`: Configuración de bajo nivel de GPIOs y periféricos.

## Lógica de Seguridad

La Motherboard actúa como el "Guardian de Seguridad":
1.  **Independencia**: Si el display se desconecta, la placa sigue controlando la temperatura según el último setpoint seguro.
2.  **Alarmas**: Las alarmas se disparan físicamente (buzzer) y se envían al HMI para su representación visual.
3.  **Persistencia**: Estado guardado en EEPROM para recuperación tras fallos de alimentación.

## Compilación

```bash
# Compilar para v1.5 (S3)
pio run -e in3ator_V15
pio run -e in3ator_V15 -t upload
```

---
© 2026 In3ator
