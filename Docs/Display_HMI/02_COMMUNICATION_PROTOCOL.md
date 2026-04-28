# Protocolo de Comunicación — Motherboard ↔ Display HMI

> Fuentes principales: `PROTOCOL.md`, `Firmware/docs/communication.md`, `include/CommTask.h`, `src/CommTask.cpp`

## 1. Capa Física

| Parámetro | Valor |
|---|---|
| Interfaz | UART asíncrono (bridgeado via CH340C USB-Serial) |
| Baudrate | 115200 bps |
| Formato | 8N1 (8 bits datos, sin paridad, 1 stop bit) |
| Terminador | `\n` (0x0A) |
| Retorno de carro | `\r` ignorado en recepción |
| Buffer RX (HMI) | 512 bytes (`COMM_RX_BUFFER_SIZE`) |
| Timeout RX | 50 ms (`COMM_RX_TIMEOUT_MS`) — si no llega `\n`, se descarta el buffer |
| Puerto HMI | `COMM_SERIAL = Serial` (USB CDC ESP32-S3) |
| Puerto MB | `modemSerial = Serial2` en la Motherboard |

El canal físico es un **bridge USB-Serial CH340C** que presenta el UART de la Motherboard como un puerto COM virtual en el lado USB del ESP32-S3 del HMI. Ambos MCUs se comunican mediante texto ASCII con prefijos de identificación.

## 2. Protocolo de Texto Activo (Producción)

### 2.1 Convención de Prefijos

| Origen | Prefijo | Ejemplo |
|---|---|---|
| Motherboard → Display | `CTRL,` | `CTRL,TEL,35.2,36.8,55,0\n` |
| Display → Motherboard | `HMI,` | `HMI,3,1,1,35.5,36.8,55.0,0,0,1,18\n` |

Cualquier línea que no comience con el prefijo esperado (`EXPECTED_PREFIX`) es descartada silenciosamente.

---

### 2.2 Mensajes Motherboard → Display HMI

#### `CTRL,STATE` — Estado completo del sistema
**Frecuencia**: 1 Hz (mínimo) o bajo demanda cuando el HMI envía `HMI,REQ,STATE`.

**Formato**:
```
CTRL,STATE,<act>,<mode>,<airSet>,<skinSet>,<humSet>,<photo>,<mute>,
           <sn>,<hwNum>,<hwRev>,<fwVer>,<numAlarms>,<skinE>,
           <commStatus>,<photoTimeRem>,<lang>,<probeState>,0x<alarmBitmask>
```

**Campos**:

| Campo | Tipo | Descripción |
|---|---|---|
| `act` | int | Bitmask de actuadores activos (bit 0: calefactor/motor, etc.) |
| `mode` | int | Modo de control: 1=AIR CONTROLLED, 0=BABY CONTROLLED (piel) |
| `airSet` | double | Setpoint temperatura aire (ºC), ej: 35.50 |
| `skinSet` | double | Setpoint temperatura piel (ºC), ej: 36.80 |
| `humSet` | double | Setpoint humedad (%), ej: 55.0 |
| `photo` | int | Estado fototerapia: 0=apagada, 1=encendida |
| `mute` | int | Alarma silenciada: 0=no, 1=sí |
| `sn` | int | Número de serie del equipo |
| `hwNum` | int | Número de versión hardware |
| `hwRev` | char | Revisión hardware ('A', 'B', ...) |
| `fwVer` | string | Versión firmware Motherboard (ej: "2.5.3") |
| `numAlarms` | int | Número de alarmas activas |
| `skinE` | int | Modo piel habilitado: 0/1 |
| `commStatus` | int | Estado conectividad IoT (0=NONE, 1=GPRS, 2=GPRS+SERVER, 3=WIFI, 4=WIFI+SERVER) |
| `photoTimeRem` | double | Tiempo restante fototerapia en formato MM.SS (ej: 18.33 = 18 min 33 s) |
| `lang` | int | Idioma activo: 0=ES, 1=EN, 2=FR |
| `probeState` | int | Estado validado sonda piel (ver tabla estados) |
| `alarmBitmask` | uint32 hex | Máscara bits alarmas activas `0x<hex>` — bit n = alarm ID n activo |

**Estados de sonda de piel** (`probeState`):

| Valor | Nombre | Descripción |
|---|---|---|
| 0 | SKIN_PROBE_NOT_CONNECTED | Sin sonda |
| 1 | SKIN_PROBE_PENDING_VALIDATION | Detectada, validando |
| 2 | SKIN_PROBE_VALID | Sonda válida y en uso |
| 3 | SKIN_PROBE_INVALID | Sonda presente pero lecturas inválidas |
| 4 | SKIN_PROBE_OUT_OF_RANGE | Temperatura fuera de rango físico |
| 5 | SKIN_PROBE_DISCONNECTED_DURING_OPERATION | Desconectada durante control activo |
| 6 | SKIN_PROBE_UNSTABLE | Lecturas inestables |

**Ejemplo real** (extraído del código `parse_message()`):
```
CTRL,STATE,3,1,35.50,36.80,55.00,0,0,1234,15,B,2.5.3,0,1,2,18.33,1,2,0x06
```

> Ver: `src/CommTask.cpp:133` — el `sscanf` acepta de 12 a 18 campos para compatibilidad retroactiva con versiones anteriores.

---

#### `CTRL,TEL` — Telemetría en tiempo real
**Frecuencia**: 1 Hz (intercalado con STATE).

**Formato**:
```
CTRL,TEL,<airDet>,<skinDet>,<humDet>,<serverStatus>[,<probeState>]
```

| Campo | Tipo | Descripción |
|---|---|---|
| `airDet` | double | Temperatura aire medida (ºC) |
| `skinDet` | double | Temperatura piel medida (ºC) |
| `humDet` | double | Humedad medida (%) |
| `serverStatus` | int | Estado conectividad IoT |
| `probeState` | int | (Opcional) Estado sonda piel |

**Ejemplo**:
```
CTRL,TEL,35.20,36.85,54.50,3,2
```

> Ver: `src/CommTask.cpp:109` — el `sscanf` acepta 3, 4 o 5 campos.

---

#### `CTRL,ALM` — Evento de alarma
**Frecuencia**: Evento asíncrono — se emite instantáneamente cuando una alarma cambia de estado.

**Formato**:
```
CTRL,ALM,<id>,<short_text>,<desc_text>,<active>
```

| Campo | Tipo | Descripción |
|---|---|---|
| `id` | int | ID de alarma (1-9, ver enum `ALARMS_ID`) |
| `short_text` | string | Texto corto (≤30 chars, ej: "TEMP_ALARM") |
| `desc_text` | string | Descripción larga (≤30 chars en sscanf) |
| `active` | int | 1=alarma activa, 0=alarma extinguida |

**Tabla de IDs de alarma**:

| ID | Nombre | Descripción | Nivel de riesgo |
|---|---|---|---|
| 1 | HUMIDITY_ALARM | Desviación ±12% respecto a setpoint | Medical/Leve |
| 2 | TEMPERATURE_ALARM | Desviación ±1.0ºC respecto a setpoint | Medical/Crítico |
| 3 | AIR_THERMAL_CUTOUT_ALARM | Temperatura compartimento >38.5ºC | HW Fault/Emergencia |
| 4 | SKIN_THERMAL_CUTOUT_ALARM | Temperatura piel >37.5ºC | HW Fault/Emergencia |
| 5 | AIR_SENSOR_ISSUE_ALARM | Fallo sensor Sensirion I2C | HW Fault/Emergencia |
| 6 | SKIN_SENSOR_ISSUE_ALARM | Sonda NTC desconectada o ADC espurio | HW Fault/Emergencia |
| 7 | FAN_ISSUE_ALARM | Pérdida de pulso encoder del ventilador | HW Fault/Emergencia |
| 8 | HEATER_ISSUE_ALARM | Caída de amperaje en resistencia calefactora | HW Fault/Emergencia |
| 9 | POWER_SUPPLY_ALARM | INA3221 detecta anomalía en Vin 12V (HW≥13) | Elec Fault/Crítico |

**Ejemplos**:
```
CTRL,ALM,2,TEMP_ALARM,Temp deviation >1C,1
CTRL,ALM,2,TEMP_ALARM,Temp deviation >1C,0
```

---

#### `CTRL,PPG` — Muestra de pletismografía (SpO2)
**Frecuencia**: 25 Hz.

**Formato**: `CTRL,PPG,<ppg>`
- `ppg`: uint8 normalizado 0–255

---

#### `CTRL,VIT` — Signos vitales
**Frecuencia**: 1 Hz.

**Formato**: `CTRL,VIT,<hr>,<spo2>`
- `hr`: frecuencia cardíaca en bpm (0=sin señal)
- `spo2`: SpO2 % (0=sin señal, actualmente siempre 0 en esta versión)

---

#### `CTRL,PWR_OFF` / `CTRL,PWR_OFF_CANCEL` — Control de apagado
```
CTRL,PWR_OFF,<ms_remaining>
CTRL,PWR_OFF_CANCEL
```
Inicia/cancela la cuenta regresiva de apagado del sistema (3000 ms total).

---

### 2.3 Mensajes Display HMI → Motherboard

#### `HMI,...` — Comando de usuario (configuración)
**Frecuencia**: Evento — se envía cuando el usuario guarda cambios.

**Formato**:
```
HMI,<act>,<skinE>,<mode>,<airSet>,<skinSet>,<humSet>,<photo>,<mute>,
    <lang>,<photoMin>,<babyWeight>,<babyGest>,<babyAgeDays>
```

| Campo | Tipo | Descripción |
|---|---|---|
| `act` | int | Bitmask actuadores (temp/hum control activo) |
| `skinE` | int | Modo piel habilitado (0/1) |
| `mode` | int | Modo de control: 1=AIR, 0=BABY (piel) |
| `airSet` | double | Setpoint temperatura aire deseada (ºC) |
| `skinSet` | double | Setpoint temperatura piel deseada (ºC) |
| `humSet` | double | Setpoint humedad deseada (%) |
| `photo` | int | Estado fototerapia (0/1) |
| `mute` | int | Silenciar alarma (0/1) |
| `lang` | int | Idioma (0=ES, 1=EN, 2=FR) |
| `photoMin` | int | Minutos restantes de fototerapia |
| `babyWeight` | int | Peso bebé en gramos (Auto Air) |
| `babyGest` | int | Edad gestacional en semanas (Auto Air) |
| `babyAgeDays` | int | Edad postnatal en días (Auto Air) |

> Ver: `src/CommTask.cpp:77` — función `SendMessageToOtherESP()`

**Ejemplo**:
```
HMI,3,1,1,35.50,36.80,55,0,0,1,18,1500,32,3
```

---

#### `HMI,UI_READY` — Handshake crítico de inicialización
Enviado **una sola vez** cuando el HMI ha completado la inicialización de LVGL y la UI está completamente dibujada.

**Efecto**: La Motherboard, al recibir este mensaje, reenvía inmediatamente el estado completo de todas las alarmas activas (`sendAlarmUSB()`), garantizando sincronía audiovisual perfecta tras un reinicio del HMI.

---

#### `HMI,REQ,STATE` — Solicitud de sincronización manual
Solicita a la Motherboard que envíe un `CTRL,STATE` inmediatamente.

---

#### `HMI,BOOT,<rst>,<count>` — Información de arranque
```
HMI,BOOT,<reset_reason>,<boot_count>
```
- `reset_reason`: código `esp_reset_reason_t`
- `boot_count`: número acumulado de arranques (NVS "diag/boots")

---

#### `HMI,WIFI,<ssid>,<password>` — Credenciales WiFi
Transmite credenciales WiFi del usuario a la Motherboard para conectividad IoT.

---

## 3. Mecanismos de Robustez

### 3.1 Sincronización de Estado (State Sync)

La función `Display_StateSync_Service()` (ver `src/CommTask.cpp:390`) implementa la lógica de sincronización inicial:

```
                  HMI Arranque
                       │
                       ▼
              ┌─────────────────┐
              │ g_stateSynced?  │─── Sí ──► bucle normal
              └────────┬────────┘
                       │ No
                       ▼
            ┌──────────────────────┐
            │ Enviar HMI,REQ,STATE │ cada 500ms
            └──────────┬───────────┘
                       │
            ┌──────────▼───────────┐
            │ ¿ctrl_state_msg.new? │
            └──────────┬───────────┘
                       │ Sí
                       ▼
            ┌──────────────────────┐
            │ Display_ApplyCtrlState│ Aplica estado
            │ g_stateSynced = true  │ Sincronizado
            └──────────────────────┘
```

### 3.2 Auto-corrección via Bitmask de Alarmas

Cada vez que llega un `CTRL,STATE`, el campo `alarmBitmask` se compara con el estado visual del HMI:

```c
// src/CommTask.cpp:364
for (int id = 1; id < MAX_ALARMS; id++) {
    bool boardActive = (st.alarmBitmask >> id) & 1;
    if (alarmList[id].state && !boardActive) {
        alarmList[id].state = false;  // Auto-limpieza de "phantoms"
        changed = true;
    }
}
```

Esto elimina las "alarmas fantasma" causadas por pérdidas de paquetes UDP.

### 3.3 Filtro de Actualizaciones de Labels

En `update_labels()` (`src/UITask.cpp:263`), cada label solo se actualiza si el valor cambió más de 0.05ºC (`TEMP_LABEL_UPDATE_THRESHOLD`), evitando saturación del DMA de LVGL:

```c
if (abs(airTempValueDetected - l_airDet) > TEMP_LABEL_UPDATE_THRESHOLD) {
    lv_label_set_text(ui_TempAirDetected, buffer);
}
```

### 3.4 Timeout y Overflow del Buffer RX

```c
// src/CommTask.cpp:221
if (rxIndex > 0 && (millis() - lastRxTime > COMM_RX_TIMEOUT_MS)) {
    rxIndex = 0;  // Descartar trama incompleta después de 50ms
}
if (rxIndex < sizeof(rxBuffer) - 1) {
    rxBuffer[rxIndex++] = c;
} else {
    rxIndex = 0;  // Overflow: descartar línea demasiado larga
}
```

## 4. Protocolo Binario Alternativo (TLV) — No activo en producción

Definido en `include/display_comms.h`. Preparado para migración futura a protocolo más robusto.

### Estructura de trama TLV

```
┌────────┬────────┬───────┬─────────┬────────┬────────────┬──────────────┬──────────┐
│ 0xAA   │ 0x55   │  Ver  │ MsgType │  Seq   │ PayloadLen │   Payload    │  CRC16   │
│ 1 byte │ 1 byte │ 1 byte│  1 byte │ 2 bytes│   2 bytes  │   N bytes    │  2 bytes │
└────────┴────────┴───────┴─────────┴────────┴────────────┴──────────────┴──────────┘
```

- **Preamble**: 0xAA 0x55
- **CRC**: CRC16-CCITT (X.25) sobre bytes [2..end-3]
- **TLVs**: `[Type:u16][Len:u16][Value:Len]`

### Tipos de mensaje TLV

| Tipo | Valor | Dirección |
|---|---|---|
| DC_MSG_TELEMETRY | 0x01 | MB → HMI |
| DC_MSG_COMMAND | 0x02 | HMI → MB |
| DC_MSG_ACK | 0x03 | Ambos |
| DC_MSG_NACK | 0x04 | Ambos |
| DC_MSG_HEARTBEAT | 0x05 | Ambos |
| DC_MSG_REQUEST_FULL | 0x06 | HMI → MB |

### Parámetros de fiabilidad TLV

| Parámetro | Valor |
|---|---|
| ACK timeout | 50 ms |
| Max reintentos | 3 |
| Heartbeat period | 1000 ms |
| RX idle timeout | 200 ms |
| Max frame size | 1024 bytes |

> **Estado**: Este protocolo está **completamente implementado** en `include/display_comms.h` y `src/CommTask.cpp` pero **NO está activo** en producción. El protocolo ASCII de texto es el que usa el sistema actualmente.

## 5. Diagramas de Secuencia

### 5.1 Arranque del sistema (Cold Boot)

```
     HMI                    Motherboard
      │                          │
      │  [boot: ~2-3 segundos]   │  [boot: ~200ms]
      │                          │  CTRL,STATE inicial...
      │                          │  (HMI no escucha aún)
      │                          │
      │─── HMI,BOOT,rst,count ──►│  Informa reset reason
      │─── HMI,REQ,STATE ────────►│  Solicita estado
      │                          │
      │◄── CTRL,STATE,... ───────│  Estado completo
      │                          │
      │  [LVGL inicializado]     │
      │─── HMI,UI_READY ─────────►│  UI lista
      │                          │
      │◄── CTRL,ALM,id,txt,1 ───│  Re-envío alarmas activas
      │◄── CTRL,ALM,id,txt,1 ───│  (todas las pendientes)
      │                          │
      │◄── CTRL,TEL,t,t,h,s ────│  Telemetría 1 Hz (continua)
      │◄── CTRL,STATE,... ───────│  Estado 1 Hz (continua)
```

### 5.2 Cambio de setpoint por usuario

```
     Usuario          HMI                    Motherboard
       │               │                          │
       │──Toca UP───►  │                          │
       │               │  airTempValue += 0.2     │
       │               │  (cambio local inmediato) │
       │◄─ Actualiza ──│                          │
       │   label       │                          │
       │               │─── HMI,3,0,1,35.70,... ─►│  Comando UART
       │               │                          │  PID: nuevo setpoint
       │               │◄── CTRL,STATE,...,35.70  │  Confirmación 1 Hz
       │               │  g_stateSynced = true    │
```

### 5.3 Alarma activa y silencio

```
     Motherboard           HMI              Usuario
          │                  │                  │
          │  T > 37.5ºC      │                  │
          │─── CTRL,ALM,2,TEMP,Desc,1 ──────►   │
          │                  │  alarmList[2].state = true
          │                  │  update_alarm_panels()
          │                  │  AlarmSound_Update()
          │                  │─────────────────►│  Alarma visual+sonido
          │                  │                  │
          │                  │  ◄──Toca Mute──  │
          │                  │  alarmsMuted=true│
          │◄─── HMI,...,1,... ─────────────────  │  muteAlarm=1
          │  Silencia piezo  │                  │
          │                  │  Alarma visual persiste (IEC 60601-1-8 §6.8.1)
```
