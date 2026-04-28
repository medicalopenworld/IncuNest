# Documentación Técnica — Display HMI (IncuNest)

> Generado el 2026-04-28. Basado en análisis exhaustivo de `Firmware/Display_HMI` (rama `dev`).

Este directorio contiene la documentación técnica completa del subsistema Display HMI de IncuNest,
orientada a servir como base para la **reimplementación nativa en ESP-IDF** eliminando PlatformIO.

---

## Índice de Documentos

| Archivo | Descripción |
|---|---|
| `01_ARCHITECTURE_OVERVIEW.md` | Visión general del sistema completo y arquitectura interna del HMI (tareas, drivers, LVGL). |
| `02_COMMUNICATION_PROTOCOL.md` | Protocolo de comunicación serie Motherboard ↔ Display HMI: formato de trama, todos los mensajes, diagramas de secuencia. |
| `03_DISPLAY_DRIVER_ANALYSIS.md` | Análisis del driver RGB paralelo actual, diferencias con el código de referencia del fabricante, configuración LVGL. |
| `04_PLATFORMIO_DEPENDENCIES.md` | Inventario completo de dependencias PlatformIO con equivalente en ESP-IDF IDF Component Manager y notas de migración. |
| `05_KNOWN_ISSUES_AND_ROOT_CAUSES.md` | Problemas de estabilidad conocidos (parpadeos, reinicios, colisiones) con causa raíz y referencia exacta al código. |
| `06_ESPIDF_MIGRATION_REQUIREMENTS.md` | Hoja de requisitos funcionales (RF) y no funcionales (RNF) para la reimplementación en ESP-IDF nativo. |
| `07_ESPIDF_PROJECT_STRUCTURE.md` | Estructura de proyecto ESP-IDF propuesta: árbol de directorios, CMakeLists.txt, idf_component.yml, particiones. |
| `08_LVGL_CONFIGURATION.md` | Configuración LVGL optimizada para ESP-IDF nativo con CrowPanel 7": lv_conf.h, buffer strategy, tick. |
| `09_GLOSSARY_AND_REFERENCES.md` | Glosario de términos, referencias a código fuente original y documentación oficial ESP-IDF. |
| `10_REGULATORY_COMPLIANCE.md` | Análisis de cumplimiento normativo: IEC 62304, IEC 60601-1-8, IEC 60601-2-19, IEC 62366-1. GAP analysis completo. |

---

## Contexto del Proyecto

**IncuNest** es una incubadora neonatal de código abierto clasificada como **Dispositivo Médico Clase IIb** bajo la normativa europea MDR 2017/745.

El subsistema **Display HMI** es el segundo de los dos microcontroladores del sistema:

- **Motherboard (MCU principal)**: ESP32-S3, responsable del control PID de temperatura/humedad, lectura de sensores y seguridad.
- **Display HMI (este subsistema)**: ESP32-S3 en CrowPanel Advance 7" (800×480), responsable de la interfaz táctil, visualización de alarmas y comunicación con el usuario médico.

### Estado actual del firmware

| Aspecto | Estado |
|---|---|
| Framework | Arduino 3.x sobre PlatformIO |
| Versión firmware | 2.0.2 |
| LVGL | 8.3.11 |
| ESP32-Arduino | platform-espressif32 53.03.10 |
| Flash / PSRAM | 16 MB / 8 MB OPI |
| Pantallas UI | 6 (Intro, Lock, Main, Alarms, Charts, PulseOxi, Settings) |

### Objetivo de la migración

Reescribir el Display HMI desde cero usando **ESP-IDF nativo** (sin PlatformIO, sin capa Arduino) sobre la misma base hardware CrowPanel Advance 7", mejorando la estabilidad del driver RGB y eliminando las dependencias problemáticas de la capa Arduino.
