# Protocolo de Comunicación — IncuNest

> **Versión documento:** 2026-02-25

---

## 1. Visión General

Los dos ESP32 (Display_HMI y motherBoard) se comunican por un enlace serie compartido:

| Parámetro | Valor |
|---|---|
| Interfaz física | USB-CDC (v15+) / UART (≤v14) |
| Baudrate | 115200 bps |
| Flujo de datos | Full-duplex, ASCII delimitado por `\n` |
| Protocolo de alto nivel | Mensajes ASCII tag-value |
| Protocolo alternativo disponible | TLV binario con ACK/CRC16 (driver `display_comms.h`) |

---

## 2. Protocolo ASCII (activo)

### 2.1 Mensajes del Display → Motherboard

#### Comando de control HMI
```
HMI,<act>,<skinMode>,<ctrlMode>,<airTemp>,<skinTemp>,<hum>,<photo>,<mute>,<lang>,<photoMinutes>\n
```
| Campo | Tipo | Descripción |
|---|---|---|
| `act` | int | Modo actuación: 0=Ninguno, 1=Temp, 2=Hum, 3=Temp+Hum |
| `skinMode` | bool | Modo piel activo |
| `ctrlMode` | int | 0=Aire, 1=Piel |
| `airTemp` | float %.2f | Temperatura aire deseada (ºC) |
| `skinTemp` | float %.2f | Temperatura piel deseada (ºC) |
| `hum` | float %.0f | Humedad deseada (%) |
| `photo` | bool | Foterapia ON/OFF |
| `mute` | bool | Muteo de alarma |
| `lang` | int | 0=ES, 1=EN, 2=FR |
| `photoMinutes` | int | Minutos restantes temporizador foterapia |

#### Solicitud de estado completo
```
HMI,REQ,STATE\n
```

#### Envío de credenciales WiFi
```
HMI,WIFI,<ssid>,<password>\n
```

---

### 2.2 Mensajes Motherboard → Display

#### Telemetría de sensores (periódica)
```
CTRL,TEL,<airTemp>,<skinTemp>,<humidity>,<commStatus>\n
```
| Campo | Tipo | Descripción |
|---|---|---|
| `airTemp` | double %.2f | Temperatura ambiente medida |
| `skinTemp` | double %.2f | Temperatura piel medida |
| `humidity` | double %.2f | Humedad relativa medida |
| `commStatus` | int | 0=Sin red, 1=GPRS, 2=GPRS+Server, 3=WiFi, 4=WiFi+Server |

#### Estado completo del sistema
```
CTRL,STATE,<act>,<mode>,<airSet>,<skinSet>,<humSet>,<photo>,<mute>,<sn>,<hwNum>,<hwRev>,<fwVer>,<numAlarms>,<skinEnabled>,<commStatus>,<MM.SS>\n
```
- Enviado como respuesta a `HMI,REQ,STATE`
- El Display acepta versiones con 12 a 15 campos (compatibilidad hacia atrás)
- `MM.SS`: tiempo foterapia restante en formato minutos.segundos

#### Alarma
```
CTRL,ALM,<id>,<tipo>,<descripcion>,<estado>\n
```
| Campo | Descripción |
|---|---|
| `id` | ID numérico de la alarma (0-8) |
| `tipo` | Cadena corta (≤31 chars), localizada |
| `descripcion` | Cadena descriptiva (≤31 chars), localizada |
| `estado` | 1=activa, 0=resuelta |

---

## 3. Sincronización de Estado al Arranque

```
Display arranca
    │
    ├─ Envía "HMI,REQ,STATE\n"
    ├─ Reintenta cada 500 ms hasta recibir respuesta
    │
    └─ Al recibir "CTRL,STATE,..."
         ├─ Display_ApplyCtrlState() sincroniza toda la UI
         ├─ g_stateSynced = true
         └─ Restaura temporizador de foterapia si estaba activo
```

---

## 4. Gestión de Alarmas Pendientes

Si el Display **no está conectado** cuando se genera una alarma en el motherboard:

1. La alarma se encola en `pending_alarms[]` (máximo 10)
2. Al conectarse el Display (detectado por `setHMIConnected(true)`)
3. Se ejecuta `sendPendingAlarms()` enviando todas las pendientes inmediatamente

---

## 5. Protocolo TLV Binario (driver alternativo)

Disponible en `display_comms.h`, preparado para migración futura a protocolo binario robusto.

### 5.1 Estructura de trama

```
[0x AA][0x55][Ver][MsgType][Seq:2][PayloadLen:2][TLVs...][CRC16:2]
```

| Campo | Bytes | Descripción |
|---|---|---|
| Preamble | 2 | 0xAA, 0x55 |
| Version | 1 | Versión protocolo (actual: 1) |
| MsgType | 1 | Tipo de mensaje |
| Seq | 2 | Número de secuencia LE |
| PayloadLen | 2 | Longitud de payload LE |
| Payload | N | TLVs (Type:2, Len:2, Value:N) |
| CRC16 | 2 | CRC16-CCITT (X.25) sobre bytes [2..end-3] |

### 5.2 Tipos de mensaje

| Código | Dirección | Descripción |
|---|---|---|
| `0x01` DC_MSG_TELEMETRY | MB → Display | Datos de sensores en TLVs |
| `0x02` DC_MSG_COMMAND | Display → MB | Comandos de control |
| `0x03` DC_MSG_ACK | Ambos | Confirmación recepción |
| `0x04` DC_MSG_NACK | Ambos | Error de recepción |
| `0x05` DC_MSG_HEARTBEAT | Ambos | Keep-alive periódico (1000 ms) |
| `0x06` DC_MSG_REQUEST_FULL | Display → MB | Solicitar estado completo |

### 5.3 TLVs de telemetría (parcial)

| Código | Tipo | Descripción |
|---|---|---|
| `0x0101` | double[] | Temperaturas (array) |
| `0x0102` | double[] | Humedades (array) |
| `0x0103-0x010B` | double | Corrientes y voltajes (heater, fan, humidifier, USB, BAT...) |
| `0x0112` | uint8 | Modo de control |
| `0x0120` | bool | Alarmas habilitadas |
| `0x0121` | bool[] | Alarmas individuales activas |
| `0x0124` | float | RPM del ventilador |

### 5.4 Parámetros de fiabilidad

| Parámetro | Valor |
|---|---|
| `DC_ACK_TIMEOUT_MS` | 50 ms |
| `DC_MAX_RETRIES` | 3 intentos |
| `DC_HEARTBEAT_PERIOD_MS` | 1000 ms |
| `DC_RX_IDLE_TIMEOUT_MS` | 200 ms |
| `DC_MAX_FRAME_SIZE` | 1024 bytes |

---

## 6. Consideraciones EMI y Robustez

- La comunicación USB entre ambas placas es susceptible a interferencia electromagnética (EMI) generada por el calentador y el motor del ventilador.
- El buffer RX tiene timeout de 50 ms: si no llega un `\n` en tiempo, el buffer se descarta.
- El Display reintenta la sincronización de estado cada 500 ms hasta completarla.
- El motherboard usa un mutex recursivo (`log_mutex`) para serializar el acceso al Serial y evitar corrupción de tramas.
