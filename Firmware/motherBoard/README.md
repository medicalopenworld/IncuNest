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

> **Nota:** esta sección cubre el build con ESP-IDF (`idf.py`). Otras secciones de este README pueden ser anteriores al port y estar desactualizadas.

### Requisito previo: submódulos

`Firmware/components/incunest_afe4490` es un **submódulo de git** (la librería del
frontal de SpO2, pineada en `v0.92`). Un clon sin `--recursive` deja ese
directorio vacío y el build aborta con un error que te indica cómo clonarlo correctamente.

Los comandos siguientes se ejecutan **desde la raíz del repo**, no desde
`Firmware/motherBoard/`:

```bash
# Al clonar
git clone --recursive <url-del-repo>

# O si ya clonaste sin --recursive
git submodule update --init --recursive
```

### Build

```powershell
# Desde PowerShell. idf.py NO funciona bajo Git Bash: avisa de MSys/Mingw,
# no construye nada y aun asi sale con codigo 0.
Set-Location Firmware\motherBoard
idf.py build
idf.py -p COM30 flash monitor
```

### Build de diagnóstico: tiempos del PPG

Instrumenta la librería del AFE4490 y emite una trama `$TIMING` cada 5 s con
medias y máximos por etapa, incluidos `spi_mean`/`spi_max`. **No es un binario
de producción** (cuesta dos lecturas de `esp_timer` por muestra a 500 Hz):

```powershell
$env:INCUNEST_PPG_TIMING = "1"
idf.py reconfigure build flash monitor
```

**Importante:** `idf.py build` a secas no vuelve a ejecutar el configure de CMake
si solo cambió una variable de entorno. Hace falta `idf.py reconfigure` (o
`idf.py fullclean`) después de poner (o quitar) la variable. La única señal
fiable de que se activó es un `message(WARNING)` en la salida del configure que
dice `INCUNEST_PPG_TIMING: ... ACTIVADA`. Sin ese warning, el binario está sin
instrumentar aunque el build haya terminado.

Para volver al binario de producción:

```powershell
Remove-Item Env:\INCUNEST_PPG_TIMING
idf.py reconfigure build
```

---
© 2026 In3ator
