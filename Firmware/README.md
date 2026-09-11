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

## Compilar

Desde 2026-09 las **tres placas** se construyen con **ESP-IDF v6.0.1** (`idf.py`,
CMake); PlatformIO y el framework Arduino ya no se usan. Detalle, decisiones y
estado del porte en [`docs/porte-esp-idf-nativo.md`](./docs/porte-esp-idf-nativo.md).

```powershell
# idf.py solo desde PowerShell con el export.ps1 de la IDF cargado
idf.py -C Firmware/Display_HMI build
idf.py -C Firmware/motherBoard build          # HW_NUM y variante de taller: idf.py menuconfig
idf.py -C Firmware/SensorBoard_v2 build

# tests Unity de host (25 suites, sin hardware)
pwsh Firmware/tools/host_tests/run_host_tests.ps1   # 25 suites Unity en el PC
pwsh Firmware/tools/host_tests/run_host_tests.ps1   # 25 suites Unity en el PC
```

Flashear y monitorizar (`idf.py -p COMx flash monitor`) es siempre manual. Los
binarios quedan en `build/<proyecto>.bin`, `build/bootloader/bootloader.bin` y
`build/partition_table/partition-table.bin`; el `flasher_tool` los recoge de ahi.

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
© 2026 IncuNest - Advanced Neonatal Care Systems
