# IncuNest — Descripción Funcional Completa del Proyecto

> **Propósito de este documento**: Describir en detalle toda la funcionalidad implementada en el sistema IncuNest, para servir de base en la futura elaboración de una hoja de requisitos formal.

---

## 1. ¿Qué es IncuNest?

IncuNest es una **incubadora neonatal de código abierto** desarrollada por Medical Open World. Está diseñada para hospitales con recursos limitados, especialmente en países de bajos ingresos. Su coste de fabricación es de aproximadamente **350 €** en componentes frente a los **35.000 €** de una incubadora comercial equivalente.

El sistema ha sido desplegado en más de **200 unidades** en 30 países.

### Objetivo clínico

- Mantener al recién nacido prematuro o de bajo peso en un entorno térmico y húmedo controlado.
- Proporcionar fototerapia para el tratamiento de la ictericia neonatal.
- Monitorizar de forma continua la temperatura de la piel y del aire, la humedad y el estado del bebé.
- Alertar al personal médico ante cualquier situación de riesgo.
- Conectarse a plataformas de monitorización remota para seguimiento clínico.

---

## 2. Arquitectura General del Sistema

IncuNest está compuesto por **dos módulos electrónicos independientes** que se comunican entre sí:

```
┌─────────────────────────────┐        USB / UART          ┌─────────────────────────────┐
│       PLACA MADRE           │ ◄─────────────────────────► │         PANTALLA HMI        │
│  (Control y seguridad)      │        115200 bps           │   (Interfaz de usuario)     │
│  ESP32-S3 (240 MHz)         │                             │  CrowPanel ESP32-S3 7"      │
└─────────────────────────────┘                             └─────────────────────────────┘
         │
         ├── Sensores (I2C, ADC)
         ├── Actuadores (PWM, GPIO)
         ├── GPRS/GSM (SIM800)
         └── WiFi (interno ESP32-S3)
```

Esta separación garantiza que el **control y la seguridad son independientes de la interfaz**. Si la pantalla falla, la incubadora sigue funcionando.

---

## 3. Hardware del Sistema

### 3.1 Procesadores

| Módulo | MCU | Frecuencia | Flash | PSRAM | Función |
|--------|-----|-----------|-------|-------|---------|
| **Placa madre** | ESP32-S3 | 240 MHz | 16 MB | — | Control, sensores, seguridad |
| **Pantalla HMI** | ESP32-S3 | 240 MHz | 16 MB | 8 MB OPI | Interfaz gráfica, audio |

> Versiones de hardware ≤ v14 usaban ESP32 FireBeetle32. Desde v15 se usa ESP32-S3.

---

### 3.2 Sensores

| Sensor | Tipo | Interfaz | Propósito | Cantidad |
|--------|------|----------|-----------|---------|
| **STS3x (Sensirion)** | Temperatura digital | I2C (0x4A, 0x4B) | Temperatura del aire de la cabina (principal + redundante) | 2 |
| **SHT4x** | Temperatura + Humedad | I2C (0x44) | Referencia ambiental externa | 1 |
| **SHTC3** | Temperatura + Humedad | I2C (0x70) | Sensor de sala alternativo | 1 |
| **NTC Termistor** | Temperatura analógica | ADC (GPIO 34/39) | Temperatura de la piel del bebé (sonda dérmica) | 1 |
| **INA3221 (principal)** | Corriente/Tensión | I2C (0x41) | Monitorización de rales 12 V y 5 V | 1 |
| **INA3221 (secundario)** | Corriente/Tensión | I2C (0x40) | Monitorización USB y batería | 1 |
| **Encoder de ventilador** | Pulsos RPM | GPIO | Retroalimentación de velocidad del ventilador | 1 |
| **SPO2 (AFE44xx)** | Biométrico IR | SPI + ADC | Oxígeno en sangre y pulso *(opcional)* | 1 |

**Filtrado de señal**: Filtro Butterworth de orden 6 aplicado a lecturas de temperatura. Frecuencia de muestreo: 1000 Hz, frecuencia de corte: 10 Hz.

---

### 3.3 Actuadores

| Actuador | Tipo | Control | Función | GPIO (h/w v15) |
|----------|------|---------|---------|----------------|
| **Calefactor** | Elemento resistivo | PWM | Control de temperatura | GPIO 16 |
| **Ventilador principal** | Motor DC | PWM + encoder | Circulación de aire | GPIO 12 |
| **Humidificador** | Ultrasónico/evaporativo | PWM o I2C | Control de humedad | I2C o GPIO |
| **Fototerapia** | Array de LEDs UV | PWM con temporizador | Tratamiento de ictericia | GPIO 13 |
| **Buzzer** | Piezoeléctrico | PWM frecuencia | Alarmas sonoras | GPIO 5 |

**Límites de seguridad del calefactor**:
- Corriente máxima h/w v15+: 10,5 A
- Corriente de operación segura: 9,5 A
- Si se supera el límite, el sistema reduce el PWM máximo de forma dinámica.

---

### 3.4 Gestión de Energía

- **Cargador de batería**: BQ25792 (Li-ion)
- **Monitorización de alimentación**: INA3221 en dos nodos (12 V, 5 V, USB, batería)
- **Alarma de alimentación**: Se activa si el rail de 12 V sale del rango esperado
- **Escalado dinámico del calefactor**: Reduce la potencia máxima en tiempo real si hay sobrecorriente

---

### 3.5 Pantalla y Audio (HMI)

- **LCD**: 7 pulgadas, 800×480 px, 16 bits de color (RGB565), interfaz paralela RGB
- **Táctil**: Capacitivo GT911 (I2C, pines 15/16)
- **Retroiluminación**: PWM vía controlador I2C STC8H1K28 @ 0x30
- **Audio**: Biblioteca ESP32-audioI2S para reproducción MP3 por altavoz interno
- **PCLK del LCD**: 12–13,5 MHz (limitado para evitar interferencias con el DMA de audio)

---

## 4. Software: Arquitectura

### 4.1 Placa Madre — Tareas FreeRTOS

| Tarea | Función |
|-------|---------|
| `sensors_Task` | Lee todos los sensores I2C/ADC a intervalos variables |
| `security_Task` | Monitoriza alarmas, sobretemperaturas y fallos de sensores |
| `buzzer_Task` | Genera tonos de alarma y confirmación |
| `GPRS_Task` | Comunicación GSM/GPRS con ThingsBoard IoT |
| `GPRSMonitor_Task` | Watchdog del módem GPRS (Core 0) |
| `OTA_WIFI_Task` | Actualizaciones OTA por WiFi y ThingsBoard |
| `TimeTrack_Task` | Seguimiento de tiempo acumulado de operación |
| `Communication_Task` | Envío de datos a HMI por UART (v15+) |
| `Comm_Receiver` | Recepción de comandos del HMI (v15+) |
| `UI_Task` | Pantalla ILI9341 legada (solo h/w ≤ v14) |

**Framework**: C++ con Arduino + FreeRTOS. El código compila con PlatformIO.

---

### 4.2 Pantalla HMI — Tareas FreeRTOS

| Tarea | Core | Función |
|-------|------|---------|
| `UITask` | 1 | Renderizado LVGL, entrada táctil, gestión de pantallas |
| `CommTask` | 0 | Recepción y parseo de mensajes UART desde la placa madre |
| `OTA_Task` | 0 | Actualizaciones OTA por WiFi |
| `AudioTask` | 0 | Reproducción de audio por DMA I2S |

**Librería gráfica**: LVGL 8.3.11 con LovyanGFX 1.1.12 como driver LCD (DMA optimizado).

---

## 5. Modos de Control de Temperatura

### 5.1 Modo Aire (Air Control)

- **Entrada**: Temperatura del aire de la cabina (STS3x)
- **Consigna**: `desiredAirTemperature` (30–38,5 °C)
- **Salida**: PWM del calefactor
- **Disponibilidad**: Siempre activo

### 5.2 Modo Piel (Skin Servocontrol)

- **Entrada**: Temperatura de la piel del bebé (NTC)
- **Consigna**: `desiredSkinTemperature` (35–37,5 °C)
- **Salida**: PWM del calefactor (con prioridad sobre modo aire)
- **Disponibilidad**: Solo cuando la sonda de piel es válida y está conectada
- **Seguridad**: Sale automáticamente del modo si la sonda se desconecta o invalida durante el uso

---

### 5.3 Control de Humedad

- **Bucle PID independiente**
- **Entrada**: Humedad del SHTC3/SHT4x
- **Consigna**: `desiredControlHumidity` (20–90 % HR)
- **Salida**: Ciclo de trabajo del humidificador
- **Tiempo de muestreo**: 200 ms

---

### 5.4 Estados de Operación del Sistema

| Estado | Descripción |
|--------|-------------|
| **Standby** | PIDs desactivados, solo monitorización. Sin calefacción ni humidificación |
| **Control Activo** | PIDs regulando activamente. Todos los actuadores disponibles |
| **Fallo Crítico** | Todos los actuadores desactivados. Requiere resolución y reset manual |

---

## 6. Sonda de Piel (Skin Probe)

### 6.1 Detección y Validación

- La sonda NTC se conecta mediante conector externo → pin ADC del ESP32-S3
- Se lee cada 5 segundos
- Validación de rango: 25–50 °C
- Tiempo de debounce configurable para evitar falsos positivos por conexión mecánica

### 6.2 Máquina de Estados

```
NOT_CONNECTED ──► PENDING_VALIDATION ──► VALID ◄──► INVALID
                                            │
                                            ▼
                                  DISCONNECTED_DURING_USE
```

| Estado | Descripción |
|--------|-------------|
| `NOT_CONNECTED` | Sin señal de la sonda |
| `PENDING_VALIDATION` | Conectada pero pendiente de verificación |
| `VALID` | Conectada y leyendo en rango válido |
| `INVALID` | Lectura fuera de rango o inestable |
| `DISCONNECTED_DURING_USE` | Desconexión durante operación en modo piel |

### 6.3 Restricciones de Uso

- El modo piel queda **bloqueado** si la sonda no está en estado `VALID`
- La HMI desactiva el botón correspondiente y muestra mensaje informativo
- Si la sonda se invalida durante el uso, el sistema vuelve automáticamente a modo aire
- En modo aire, la ausencia de sonda solo es informativa (sin alarma)

---

## 7. Fototerapia

- **Sistema**: Array de LEDs UV para tratamiento de ictericia neonatal
- **Modos**:
  - Continuo (hasta apagado manual)
  - Temporizador (apagado automático tras N minutos)
- **Rango del temporizador**: 1–120 minutos, resolución 1 minuto
- **Seguimiento**: La placa madre lleva el tiempo transcurrido en formato MM.SS
- **Seguridad**: Apagado automático al llegar a 00:00 (gestionado por placa madre)
- **HMI**: Muestra la cuenta regresiva, no gestiona el apagado

---

## 8. Sistema de Alarmas

### 8.1 Tipos de Alarma

| ID | Nombre | Condición | Nivel de Riesgo | Auto-reset |
|----|--------|-----------|-----------------|-----------|
| 0 | NO_ALARMS | — | Ninguno | — |
| 1 | HUMIDITY_ALARM | ±12 % del punto de consigna | Médico/Leve | Sí (histéresis 5 %) |
| 2 | TEMPERATURE_ALARM | ±1,0 °C del punto de consigna | Médico/Crítico | Sí (histéresis 0,05 °C) |
| 3 | AIR_THERMAL_CUTOUT | >38,5 °C absoluto | HW Fallo/Emergencia | Sí (reset a 37,0 °C) |
| 4 | SKIN_THERMAL_CUTOUT | >37,5 °C absoluto (piel) | HW Fallo/Emergencia | Sí (reset a 37,3 °C) |
| 5 | AIR_SENSOR_ISSUE | Sin datos I2C/señal >20 s | HW Fallo/Emergencia | Auto al restaurar señal |
| 6 | SKIN_SENSOR_ISSUE | Sin lectura ADC o inválida | HW Fallo/Emergencia | Auto al validar |
| 7 | FAN_ISSUE | Sin pulsos RPM detectados | HW Fallo/Emergencia | Auto al recuperar RPM |
| 8 | HEATER_ISSUE | Caída de corriente anormal | HW Fallo/Emergencia | Auto al normalizar |
| 9 | POWER_SUPPLY_ALARM | Rail 12 V fuera de rango | Fallo Elec./Crítico | Auto al normalizar |

**Histéresis**: Cada alarma tiene umbral de disparo y umbral de reset diferente para evitar oscilaciones.

**Retardo de ignición**: Las alarmas se suprimen ~30 minutos tras el arranque en frío, para evitar falsos positivos durante el calentamiento inicial.

### 8.2 Sincronización de Alarmas entre Módulos

- La placa madre transmite un **bitmask** de alarmas activas en cada mensaje de estado
- La HMI usa ese bitmask para eliminar alarmas "fantasma" que pudieran quedar tras una desconexión USB
- Si la HMI muestra una alarma que el bitmask no incluye, la elimina automáticamente (**Auto-Healing UI**)

### 8.3 Audio de Alarma

| Evento | Frecuencia | Duración |
|--------|-----------|---------|
| Standby (sin alarma) | Chirp suave 500 µs | Cada 10 s |
| Alarma activa | 500 µs, ciclos on/off 500 ms | ~5 min si no se silencia |
| Feedback de usuario (encoder) | 2200 µs | Instantáneo |

**Función silencio (Mute)**:
- Solo silencia el buzzer, **nunca la visualización** (requisito de seguridad médica)
- Se reactiva si ocurre una nueva alarma

---

## 9. Controladores PID

### 9.1 Configuración de los Bucles

| Bucle | Entrada | Consigna | Salida | Kp | Ki | Kd | Tiempo muestreo |
|-------|---------|----------|--------|----|----|----|----------------|
| **Control Aire** | Temp. aire (STS3x) | `desiredAirTemperature` | PWM calefactor | 150 | 0,75 | 250 | 4000 ms |
| **Control Piel** | Temp. piel (NTC) | `desiredSkinTemperature` | PWM calefactor | 100 | 0,5 | 250 | 4000 ms |
| **Humedad** | Humedad (SHTC3) | `desiredControlHumidity` | Duty humidificador | 200 | 2 | 20 | 200 ms |

**Anti-windup**: Offset de 2 (aire/piel) y 5 (humedad) para evitar saturación del integrador.

**Límites de salida**: 0 a `heaterSafeMAXPWM` (escalado dinámico según corriente medida).

---

## 10. Sistema de Calibración

### 10.1 Tipos de Calibración

**Calibración de dos puntos (manual)**
1. Se registran dos temperaturas de referencia conocidas (p.ej. baño de hielo y agua caliente)
2. Se leen los valores ADC brutos en cada punto
3. Se calcula la corrección lineal: `y = mx + b`
4. Se guarda en EEPROM

**Auto-calibración**
1. La máquina alcanza estado estacionario en la consigna de aire
2. Compara lectura detectada con referencia calculada desde la calibración de dos puntos
3. Ajusta offsets automáticamente
4. Sin intervención manual

**Fine-Tune (ajuste fino)**
- Ajustes incrementales de ±0,5 °C sobre la calibración existente
- Para mantenimiento periódico o correcciones menores

### 10.2 Fórmula de Corrección

```
Temperatura = (ValorBruto - RawLow) / RawRange × ReferenceRange + ReferenceLow + FineTune
```

---

## 11. Memoria No Volátil (EEPROM)

**Total**: 256 bytes (emulada en flash del ESP32)

| Dirección | Variable | Tipo | Propósito |
|-----------|----------|------|-----------|
| 0 | CHECK_STATUS | uint8 | Flag de integridad |
| 10 | FIRST_TURN_ON | bool | Flag de primer arranque |
| 20 | AUTO_LOCK | bool | Timeout de retroiluminación |
| 30 | LANGUAGE | uint8 | 0=ES, 1=EN, 2=FR |
| 40 | SERIAL_NUMBER | uint32 | ID único del dispositivo |
| 60 | CONTROL_ACTIVE | bool | Actuación encendida/apagada |
| 65 | PHOTOTHERAPY_ACTIVE | bool | Luces UV encendidas/apagadas |
| 66 | PHOTO_TIMER_MINUTES | uint8 | Duración de fototerapia |
| 70 | CONTROL_MODE | bool | 0=Piel, 1=Aire |
| 80 | DESIRED_AIR_TEMP | double | Consigna aire (°C) |
| 85 | DESIRED_SKIN_TEMP | double | Consigna piel (°C) |
| 90 | DESIRED_HUMIDITY | double | Consigna humedad (%) |
| 100–110 | Calibración (piel) | double | Offsets de temperatura NTC |
| 115–144 | WIFI_SSID | char[30] | Nombre de red WiFi |
| 145–174 | WIFI_PASSWORD | char[30] | Contraseña WiFi |
| 170–180 | Calibración (referencia) | double | Temperaturas de referencia |
| 190–194 | Fine-tune | float | Correcciones incrementales |
| 200 | THINGSBOARD_PROVISIONED | bool | Vinculado a TB |
| 205–225 | THINGSBOARD_TOKEN | char[21] | Token de acceso al dispositivo |
| 226–250 | Tiempos activos | uint32 | Horas acumuladas por actuador |
| 251 | AUDIO_VOLUME | uint8 | Volumen del altavoz (0–21) |
| 252 | DARK_MODE | uint8 | 0=apagado, 1=encendido |

---

## 12. Protocolo de Comunicación (v1.5.0)

### 12.1 Capa Física

- **Interfaz**: UART vía chip CH340C (USB-Serie)
- **Velocidad**: 115200 bps, 8N1
- **Terminador de trama**: `\n` (0x0A)
- **Dirección**: Bidireccional (Placa madre ↔ HMI)

### 12.2 Mensajes Placa Madre → HMI

**CTRL,STATE** (cada 1 segundo)
```
CTRL,STATE,<act>,<mode>,<airSet>,<skinSet>,<humSet>,<photo>,<mute>,<sn>,<hwNum>,<hwRev>,<fwVer>,<numAlarms>,<skinE>,<commStatus>,<photoTimeRem>,<lang>,<alarmBitmask>
```
Incluye: flag de actuación, modo, consignas, número de serie, versión HW/FW, número de alarmas, flag de sonda de piel, tiempo restante de fototerapia (MM.SS), idioma, bitmask de alarmas.

**CTRL,TEL** (cada 1 segundo, intercalado)
```
CTRL,TEL,<airDetected>,<skinDetected>,<humDetected>,<serverStatus>,<skinProbeState>
```
Incluye: lecturas actuales de sensores + estado de conexión al servidor IoT + estado de sonda de piel.

**CTRL,ALM** (al cambiar una alarma)
```
CTRL,ALM,<id>,<textoCorto>,<textoLargo>,<activo>
```
Incluye: ID de alarma, descripción corta, descripción detallada, 1 (activada) o 0 (desactivada).

### 12.3 Mensajes HMI → Placa Madre

**HMI,UI_READY** (una vez en el arranque)
Señal de que la interfaz está lista. La placa madre reenvía todas las alarmas pendientes.

**HMI,...** (ante cambio de usuario)
```
HMI,<act>,<skinE>,<mode>,<airSet>,<skinSet>,<humSet>,<photo>,<mute>,<lang>,<photoMin>
```
Nuevas consignas del usuario.

**HMI,REQ,STATE** — solicita resincronización completa del estado.

**HMI,WIFI,<ssid>,<password>** — credenciales WiFi para el IoT.

### 12.4 Robustez

- La placa madre no envía alarmas hasta recibir `UI_READY`
- El bitmask en cada STATE permite a la HMI detectar y eliminar alarmas obsoletas
- Desconexión/reconexión USB gestionada mediante líneas RTS/DTR del CH340C

### 12.5 Protocolo Binario Alternativo (futuro)

Definido en `display_comms.h` pero aún no desplegado:
```
0xAA 0x55 | Ver | MsgType | Seq | PayloadLen | TLVs | CRC16
```
CRC16-CCITT sobre el payload. Tamaño máximo de trama: 1024 bytes.

---

## 13. Conectividad Remota

### 13.1 WiFi

- Opcional y configurable desde la HMI
- Soporta actualizaciones OTA
- Protocolo 802.11 b/g/n (2,4 GHz)

### 13.2 GPRS/GSM

- **Módulo**: SIM800 (Serial2 de la placa madre)
- **Velocidad**: 115200 bps
- **APN**: TM (por defecto) o personalizable
- **Soporta OTA** por conexión celular

### 13.3 ThingsBoard IoT

- **Protocolo**: MQTT sobre GPRS/WiFi
- **Credenciales**: Token de dispositivo (21 chars) almacenado en EEPROM
- **Periodo de envío de telemetría**:
  - Standby: 3600 s (1 hora)
  - Control activo: 60 s
  - Fototerapia: 180 s
- **Datos publicados**: temperaturas, humedad, consignas, tiempos de actividad, estado de alarmas, batería/alimentación, conectividad

---

## 14. Interfaz Gráfica de Usuario (HMI)

### 14.1 Pantallas y Secciones

**HUD permanente (parte superior)**
- Iconos de conectividad (WiFi, servidor, GPRS)
- Estado de hardware (calefactor, ventilador activos)
- Alarmas prioritarias en marquesina si hay desbordamiento

**Pantalla principal (dashboard)**
- Temperatura del aire (real y consigna)
- Temperatura de la piel (real y consigna, visible condicionalmente)
- Humedad (real y consigna)
- Fuentes grandes adaptadas a personal médico

**Tabs de navegación**
1. **Principal/Dashboard**: Diales, gráficos vectoriales (histórico aire/piel), gráficas en tiempo real
2. **Alarmas**: Lista de alarmas activas con descripciones
3. **Configuración**: WiFi, ThingsBoard, calibración, ajustes del sistema
4. **Fototerapia**: Pantalla del temporizador y cuenta regresiva

### 14.2 Características de la Interfaz

**Smart Refresh**:
- Las etiquetas solo se redibujan cuando el valor cambia
- Reduce la carga del DMA un 40–50 %
- Mejora la respuesta táctil

**State Caching (confirmación de consignas)**:
1. El usuario ajusta la temperatura → la HMI actualiza localmente (parpadeo = "pendiente")
2. Envía el nuevo valor a la placa madre
3. La placa madre confirma → la HMI solidifica el display
4. Si la placa madre rechaza → la HMI vuelve al valor anterior

**Soporte multiidioma**: Español, Inglés, Francés, Portugués

**Diseño táctil para guantes médicos**: Botones y zonas táctiles de mayor tamaño

---

## 15. Tests de Hardware en Arranque

Al encender, el sistema realiza una secuencia de tests de consumo de corriente:

| Test | Actuador activado | Propósito |
|------|------------------|-----------|
| `SYS_current_standby_test` | Ninguno | Línea base |
| `Heater_current_test` | Calefactor al 100 % PWM | Verificar resistencia |
| `Fan_current_test` | Ventilador en marcha | Verificar motor |
| `Phototherapy_current_test` | LEDs UV encendidos | Verificar tira LED |
| `Humidifier_current_test` | Humidificador activo | Verificar carga |
| `Display_current_test` | Retroiluminación encendida | Verificar LCD |
| `Buzzer_current_test` | Buzzer sonando | Verificar piezoeléctrico |

Si la corriente supera los límites seguros → **HW_ERROR**, se impide la operación.

Cada sensor de temperatura/humedad también se valida en el arranque. Si no responde → `SENSOR_NOTFOUND`. El sistema puede continuar con redundancia si hay sensores alternativos disponibles.

---

## 16. Parámetros de Configuración

### 16.1 Temperatura

| Parámetro | Rango | Preset | Incremento |
|-----------|-------|--------|-----------|
| Consigna piel | 35,0–37,5 °C | 36 °C | 0,1 °C |
| Consigna aire | 30,0–38,5 °C | 32 °C | 0,1 °C |

### 16.2 Humedad

| Parámetro | Rango | Preset | Incremento |
|-----------|-------|--------|-----------|
| Consigna humedad | 20–90 % HR | 60 % | 5 % |

### 16.3 Fototerapia

| Parámetro | Rango | Resolución |
|-----------|-------|-----------|
| Duración temporizador | 1–120 min | 1 min |

### 16.4 Alarmas

- Habilitación/deshabilitación maestra
- Sonido on/off
- Retardo de ignición: ~30 min en arranque en frío

---

## 17. Seguimiento de Mantenimiento

- Contadores de tiempo acumulado por modo (standby, control activo, por actuador)
- Almacenados en EEPROM (superviven ciclos de alimentación)
- Reportados vía GPRS a ThingsBoard
- Diseñados para mantenimiento preventivo basado en uso real

---

## 18. Actualizaciones de Firmware (OTA)

### Flujo de actualización

1. Comprueba actualizaciones disponibles (GPRS o WiFi)
2. Descarga el binario del firmware
3. Valida CRC16
4. Flashea en la partición secundaria
5. Reinicia y verifica
6. Reporta estado a ThingsBoard

Ambos módulos (placa madre y HMI) soportan OTA independiente.

---

## 19. Depuración y Desarrollo

### 19.1 Flags de Debug (main.h)

```cpp
#define LOG_GPRS          false
#define LOG_MODEM_DATA    false
#define LOG_INFORMATION   false
#define LOG_ERRORS        false
#define LOG_ALARMS        false
```

### 19.2 Categorías de Log

| Categoría | Contenido |
|-----------|-----------|
| `[PID]` | Actualizaciones del bucle de control |
| `[COMM]` | Mensajes de protocolo |
| `[SECURITY]` | Alarmas y fallos |
| `[UI]` | Actualizaciones de pantalla |
| `[GPRS]` | Eventos del módem |

### 19.3 Entorno de Desarrollo

- **IDE**: PlatformIO (VS Code)
- **Framework**: Arduino + ESP-IDF + FreeRTOS
- **Lenguaje**: C++
- **Versión de protocolo**: v1.5.0
- **Versión firmware placa madre**: v15.5+
- **Versión firmware HMI**: v2.0+ (LVGL 8)
- **Licencia**: No comercial, código abierto

---

## 20. Resumen de Funcionalidades

| Área | Funcionalidad | Estado |
|------|--------------|--------|
| **Control temperatura** | PID doble (aire + piel), anti-windup, calibración | Implementado |
| **Control humedad** | PID independiente, tres tipos de interfaz | Implementado |
| **Fototerapia** | LEDs UV con temporizador, apagado automático | Implementado |
| **Sonda de piel** | Máquina de estados, validación, modo servocontrol | Implementado |
| **Seguridad** | 9 tipos de alarma, corte térmico, redundancia sensores | Implementado |
| **Audio** | Buzzer de alarmas, mute solo audio | Implementado |
| **Pantalla HMI** | LVGL 8, táctil, gráficas en tiempo real, multiidioma | Implementado |
| **Conectividad WiFi** | OTA, ThingsBoard, credenciales en EEPROM | Implementado |
| **Conectividad GPRS** | SIM800, ThingsBoard, OTA celular | Implementado |
| **Monitorización remota** | ThingsBoard MQTT, telemetría adaptativa | Implementado |
| **Calibración** | 2 puntos manual + auto-calibración + fine-tune | Implementado |
| **EEPROM** | 256 bytes, todos los ajustes persistentes | Implementado |
| **Tests de arranque** | Corriente por actuador, validación sensores | Implementado |
| **OTA** | WiFi + GPRS, CRC16, partición dual | Implementado |
| **Mantenimiento** | Contadores de horas por actuador, reporte remoto | Implementado |
| **SPO2** | Sensor de oxígeno en sangre (opcional) | Implementado (opcional) |
| **Protocolo binario** | Trama CRC16 alternativa | Definido, no desplegado |

---

*Documento generado a partir del análisis del código fuente de IncuNest — Medical Open World*
*Rama analizada: `Skin_probe` — Fecha: marzo 2026*
