# IncuNest HMI — Firmware de Pantalla

> Firmware del módulo de interfaz humana (HMI) para la incubadora neonatal **IncuNest**.  
> Plataforma: **Elecrow CrowPanel Advance 7.0** · ESP32-S3 · LVGL 8 · LovyanGFX

---

## Hardware

| Componente | Descripción |
|---|---|
| **MCU** | ESP32-S3 (Xtensa LX7 dual-core, 240 MHz) |
| **Pantalla** | RGB LCD 7.0″ 800×480 @ 16 bpp (RGB565) |
| **Touch** | Goodix GT911 vía I2C |
| **Backlight** | Controlador STC8H1K28 vía I2C @ 0x30 |
| **Flash** | 16 MB (QIO, 80 MHz) |
| **PSRAM** | 8 MB OPI |
| **Audio** | DAC I2S externo |

---

## Estructura del Proyecto

```
Display_HMI/
├── include/
│   ├── display_config.h      ← ⭐ FUENTE DE VERDAD de pines y timings del display
│   ├── main.h                ← Constantes globales del firmware
│   ├── UITask.h / .cpp       ← Task principal: display + LVGL + touch
│   ├── CommTask.h / .cpp     ← Task de comunicación UART con motherboard
│   ├── AudioManager.h / .cpp ← Manager de audio I2S
│   ├── EEPROM_defines.h      ← Layout de EEPROM persistente
│   └── ...
├── src/
│   ├── main.cpp              ← Setup y creación de FreeRTOS tasks
│   ├── UITask.cpp            ← Driver LGFX + LVGL + lógica de UI
│   └── ...
├── lib/
│   └── TAMC_GT911_Fixed/     ← Driver GT911 local (version parcheada)
├── data/                     ← Archivos SPIFFS (audio .mp3)
└── platformio.ini
```

---

## Configuración del Display

### ⚠️ Información Crítica de Compatibilidad

El archivo `include/display_config.h` es la **única fuente de verdad** para la configuración del bus RGB. **No modifiques los pines ni timings en ningún otro lugar.**

> **Historial del problema resuelto (v2.0.0):**  
> El proyecto tenía dos configuraciones de display incompatibles entre sí. La clase LGFX en `UITask.cpp` usaba pines intercambiados y polaridades de sincronización incorrectas (`hsync_polarity=1`, `vsync_polarity=1`). Esto causaba que algunas unidades mostrasen **pantalla en blanco** y otras **parpadeo RGB**, dependiendo de las tolerancias de fabricación del controlador del panel. La solución fue centralizar la configuración correcta en `display_config.h` basándose en la referencia oficial de Elecrow (`gfx_conf.h`).

### Pines del Bus RGB (CrowPanel 7.0)

| Señal | GPIO | Señal | GPIO |
|---|---|---|---|
| B0 | 15 | R0 | 14 |
| B1 | 7 | R1 | 21 |
| B2 | 6 | R2 | 47 |
| B3 | 5 | R3 | 48 |
| B4 | 4 | R4 | 45 |
| G0 | 9 | DE (HENABLE) | 41 |
| G1 | 46 | VSYNC | 40 |
| G2 | 3 | HSYNC | 39 |
| G3 | 8 | PCLK | 0 |
| G4 | 16 | TOUCH SDA | 19 |
| G5 | 1 | TOUCH SCL | 20 |

### Timings de Sincronización

| Parámetro | Valor | Notas |
|---|---|---|
| Frecuencia PCLK | 12 MHz | Reducido de 15 MHz para compatibilidad DMA/audio |
| HSYNC polarity | **0** | ⚠️ No usar 1, causa parpadeo RGB |
| HSYNC front porch | 40 | — |
| HSYNC pulse width | 48 | — |
| HSYNC back porch | 40 | — |
| VSYNC polarity | **0** | ⚠️ No usar 1, causa parpadeo RGB |
| VSYNC front porch | 1 | — |
| VSYNC pulse width | 31 | — |
| VSYNC back porch | 13 | — |
| PCLK active neg | 1 | Datos válidos en flanco descendente |

---

## Configuración de Credenciales

Los secretos de despliegue (WiFi, ThingsBoard, panel web) viven en
`include/protocol/Credentials.h`, que **no está versionado** (`.gitignore`)
y está protegido por un hook. `include/protocol/Credentials_public.h` es el
fichero versionado: si `Credentials.h` no existe (clon nuevo), aporta
valores dummy para que el firmware compile; si existe, sus `#define` tienen
prioridad.

Además de los secretos de conexión, `Credentials_public.h` trae valores por
defecto **no secretos** para el menú de ayuda del heading (`docs/hmi.md`,
§6), redefinibles igual desde `Credentials.h` sin recompilar nada más:

| Define | Valor por defecto | Uso |
|---|---|---|
| `SUPPORT_EMAIL` | `support@medicalopenworld.org` | Destinatario del formulario "Contactar soporte" (telemetría ThingsBoard y QR `mailto:`) |
| `SUPPORT_TUTORIAL_URL` | `https://medicalopenworld.org/incunest/tutorial` | URL codificada en el QR de "Vídeo tutorial" |

---

## Compilación y Flash

### Prerequisitos

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/) o PlatformIO IDE
- Driver USB (CH340 o CP210x según el cable)

### Compilar

```bash
pio run -e main
```

### Flashear

```bash
pio run -e main -t upload
```

> **Nota de rutas largas (Windows):** El proyecto usa `core_dir = C:\pio` y `build_dir = C:\pio\.bld` para evitar el límite de 260 caracteres de Windows. No cambies estas rutas.

### Monitor Serie

```bash
pio device monitor --baud 115200
```

Los logs están habilitados con `CORE_DEBUG_LEVEL=5`. Buscar prefijo `[UI]` para logs del display.

---

## Arquitectura FreeRTOS

El firmware usa tres tasks de FreeRTOS:

| Task | Core | Función |
|---|---|---|
| `UI_Task` | Core 1 | Driver LGFX, LVGL tick/handler, lógica de UI |
| `Comm_Task` | Core 0 | Protocolo UART con motherboard (TLV frames) |
| `OTA_Task` | Core 0 | WiFi OTA updates |

---

## Protocolo de Comunicación con Motherboard

La comunicación con la motherboard usa el protocolo **Display Comms (DC)** definido en `include/display_comms.h`:

- Framing: `0xAA 0x55 | Version | MsgType | Seq | PayloadLen | TLVs | CRC16`
- Mensajes: Telemetría (MB→HMI), Comandos (HMI→MB), Heartbeat, ACK
- UART @ 115200 baud

---

## EEPROM Persistente

El layout de EEPROM está definido en `include/EEPROM_defines.h`.  
Variables persistidas: temperatura objetivo (aire/piel), humedad objetivo, idioma, volumen de audio, tiempo de fototerapia, timeout de inactividad.

---

## Versiones

| Versión | Cambios |
|---|---|
| **2.0.0** | Centralización de configuración del display en `display_config.h`. Corrección de pines RGB y timings hsync/vsync. Eliminación de `sc7277_init()` experimental. Documentación completa. |
| 1.0.5 | Audio I2S, control de brillo por I2C, protocolo TLV con motherboard. |

---

## Solución de Problemas

### Pantalla en blanco al arrancar
- Verificar que `display_config.h` está correctamente incluido
- Comprobar que los pines DE/VSYNC/HSYNC/PCLK son los de la tabla de arriba
- Ver logs de serie (buscar `[UI] UI Task Started`)

### Parpadeo RGB en colores
- Verificar `DISPLAY_HSYNC_POLARITY = 0` y `DISPLAY_VSYNC_POLARITY = 0` en `display_config.h`
- **No usar polarity=1 en ningún caso** para este panel

### Touch no responde
- El GT911 hace 3 intentos de inicialización al arrancar
- Ver logs `[UI] Touch controller initialized OK` o el mensaje de error
- Los pines SDA=19, SCL=20 son los correctos para CrowPanel 7.0

### Error de compilación "path too long" (Windows)
- Verificar que `core_dir` y `build_dir` en `platformio.ini` apuntan a `C:\pio`
- Activar Long Paths en Windows: `regedit` → `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem` → `LongPathsEnabled = 1`
