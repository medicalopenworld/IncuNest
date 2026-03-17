# IncuNest Firmware — Sistema de Control de Incubadora Neonatal

Este repositorio contiene el firmware completo del sistema **IncuNest**, dividido en dos módulos principales que se comunican de forma asíncrona mediante un protocolo serie robusto.

## Arquitectura del Sistema

El sistema se basa en una arquitectura de arquitectura distribuida para garantizar la máxima seguridad y rendimiento:

### 1. [Motherboard (Módulo de Control)](./motherBoard/)
- **Plataforma**: ESP32-S3 (y variantes).
- **Función**: Cerebro del sistema. Control en tiempo real de temperatura, humedad y fototerapia.
- **Responsabilidades**:
    - Lectura de sensores de grado médico (SHT4x, STS3x, SPO2).
    - Control PID de actuadores (Heist, Ventiladores, Humidificador).
    - Gestión de alarmas de seguridad (física y lógica).
    - Conectividad remota (WiFi, GPRS, ThingsBoard).
    - Host USB para conexión con el display.

### 2. [Display HMI (Interfaz de Usuario)](./Display_HMI/)
- **Plataforma**: Elecrow CrowPanel Advance 7.0 (ESP32-S3).
- **Función**: Interfaz gráfica y visualización.
- **Responsabilidades**:
    - Motor gráfico LVGL 8 de alto rendimiento.
    - Renderizado de gráficas históricas en tiempo real.
    - Reproducción de alarmas sonoras (I2S Audio).
    - Configuración del sistema por parte del usuario.

## Protocolo de Comunicación (Handshake Robusto)

Ambos sistemas se comunican mediante un protocolo serie a 115200 baudios. Se han implementado mejoras críticas de sincronización:

- **Handshake de Arranque**: El HMI envía `HMI,UI_READY` solo cuando el motor gráfico ha terminado de cargar, asegurando que ninguna alarma enviada por la Board durante el boot se pierda.
- **Sincronización de Estado (Bitmask)**: Cada segundo, la Board envía un bitmask (`0xABC`) con el estado de todas las alarmas activas, permitiendo que el HMI se "auto-sincronice" en caso de pérdida de paquetes.

Para más detalles, consulte la [Guía del Protocolo](./PROTOCOL.md).

## Optimización de Rendimiento

Se han aplicado optimizaciones profundas para garantizar una experiencia de usuario fluida:
1. **DMA Pushing**: Uso de buffers de audio y video optimizados para evitar tirones.
2. **Smart Refresh**: Las etiquetas de la UI solo se redibujan cuando hay cambios reales en los valores detectados, reduciendo la carga de CPU en un 40%.
3. **Debug Seleccionado**: Nivel de debug ajustado para evitar cuellos de botella en el puerto serie.

---
© 2026 In3ator - Advanced Neonatal Care Systems
