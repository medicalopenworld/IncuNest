# 🚨 Sistema de Alarmas y Seguridad

El sistema de alarmas es un componente crítico de IncuNest, diseñado para garantizar la seguridad del neonato mediante monitorización constante y redundancia en la comunicación.

## 1. Catálogo de Alarmas

| ID | Nombre | Condición de Activación | Gravedad |
| :--- | :--- | :--- | :--- |
| **1** | Error Humedad | Desviación >12% HR del setpoint. | Media |
| **2** | Temperatura Alta | Desviación >1°C por encima del setpoint. | Alta |
| **3** | Corte Térmico Aire | Temperatura de aire > límite de seguridad (38°C-40°C). | Crítica |
| **4** | Corte Térmico Piel | Temperatura de piel > límite de seguridad (38°C). | Crítica |
| **5** | Fallo Sensor Aire | No se reciben datos válidos del sensor ambiental. | Alta |
| **6** | Fallo Sensor Piel | No se reciben datos del sensor de piel (modo piel activo). | Alta |
| **7** | Error Ventilador | Anomalía detectada en el consumo o flujo. | Alta |
| **8** | Error Calentador | Fallo en el circuito de calentamiento. | Crítica |
| **9** | Error Alimentación | Voltaje fuera de rango o fallo en fuente/batería. | Crítica |

## 2. Lógica de Seguridad

- **Silenciado Temporal:** Las alarmas pueden silenciarse desde el HMI por un periodo de 30 minutos, tras el cual volverá a sonar si la condición persiste.
- **Prioridad de Visualización:** El HMI muestra las alarmas activas en orden de llegada, dando prioridad visual a las críticas (Corte Térmico, Fallo de Sensores).
- **Control de Actuadores:** En caso de alarma Crítica (ej. Corte Térmico), la Motherboard corta inmediatamente la alimentación a los calefactores de forma independiente al control PID.

## 3. Comunicación MB <-> HMI

Las alarmas se transmiten mediante el comando `CTRL,ALM`.

1. **Cola de Alarmas Pendientes:** Si el HMI no está conectado (o se está reiniciando), la Motherboard almacena hasta 10 alarmas en una cola local.
2. **Handshake de Sincronización:** Cuando el HMI se conecta y envía `HMI,REQ,STATE`, la Motherboard responde con el estado general y seguidamente vacía la cola de alarmas para que el HMI las muestre todas de golpe.
3. **Multi-idioma:** Los textos descriptivos de las alarmas se generan en la Motherboard según el idioma configurado (Español, Inglés, Francés), asegurando que el mensaje de seguridad sea comprensible inmediatamente.

## 4. Monitorización de Sensores

El sistema realiza un `securityCheck()` periódico que valida la "salud" de los sensores. Si un sensor no responde por más de 20 segundos, se dispara la alarma de fallo de sensor correspondiente.
