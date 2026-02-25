# Sistema de Alarmas — IncuNest

> **Versión documento:** 2026-02-25

---

## 1. Tipos de Alarma

IncuNest implementa **9 alarmas** definidas en el enum `ALARMS_ID` (compartido entre ambos firmwares):

| ID | Nombre | Clasificación | Descripción |
|---|---|---|---|
| 0 | `TEMPERATURE_ALARM` | Advertencia | Temperatura medida supera setpoint en ±1 ºC (con histéresis 0.05 ºC) |
| 1 | `HUMIDITY_ALARM` | Advertencia | Humedad medida supera setpoint en ±12 %RH (histéresis 5 %RH) |
| 2 | `AIR_THERMAL_CUTOUT_ALARM` | **CRÍTICA** | Temperatura aire supera el máximo absoluto configurado (`AIR_TEMPERATURE_SET_MAX`) |
| 3 | `SKIN_THERMAL_CUTOUT_ALARM` | **CRÍTICA** | Temperatura piel supera el máximo absoluto (`SKIN_TEMPERATURE_SET_MAX`) |
| 4 | `AIR_SENSOR_ISSUE_ALARM` | **CRÍTICA** | Sensor de aire no actualiza datos en >20 s (fallo I2C o sensor) |
| 5 | `SKIN_SENSOR_ISSUE_ALARM` | **CRÍTICA** | Sensor de piel no actualiza datos en >20 s |
| 6 | `FAN_ISSUE_ALARM` | Advertencia | Ventilador no funciona correctamente (corriente inadecuada) |
| 7 | `HEATER_ISSUE_ALARM` | **CRÍTICA** | Calentador con corriente fuera de rango (cableado o cortocircuito) |
| 8 | `POWER_SUPPLY_ALARM` | **CRÍTICA** | Voltaje del sistema fuera del rango esperado (monitoreado por INA3221) |

---

## 2. Lógica de Evaluación

### 2.1 Función evaluateAlarm()

```cpp
// Condición de disparo (con margen de error)
measuredValue > (setPoint + errorMargin + hysteresis) → setAlarm()

// Condición de reset
measuredValue < (setPoint + errorMargin - hysteresis) → resetAlarm()
```

### 2.2 Alarmas con retardo temporal (30 min)

Las alarmas de **corte térmico** tienen un mecanismo de retardo:  
- Si la alarma ya se disparó hace menos de 30 min, suena **silenciada** (no emite buzzer).  
- Esto evita alarmas acústicas repetitivas en fases de calentamiento inicial.

### 2.3 Alarmas críticas y PID

Cuando `ongoingCriticalAlarm()` devuelve `true`:
```cpp
ledcWrite(HEATER_PWM_CHANNEL, HeaterPIDOutput * !ongoingCriticalAlarm());
// → El calentador se fuerza a 0 (apagado inmediato)
```

Las alarmas críticas son: `AIR_THERMAL_CUTOUT`, `SKIN_THERMAL_CUTOUT`, `AIR_SENSOR_ISSUE`, `SKIN_SENSOR_ISSUE`, `HEATER_ISSUE`, `POWER_SUPPLY`.

---

## 3. Flujo de una Alarma

```
Motherboard detecta condición de alarma
    │
    ▼
setAlarm(alarmID, sound?)
    ├─ alarmOnGoing[id] = true
    ├─ buzzerConstantTone() (sólo motherboard)
    └─ sendAlarmUSB(alarmID, true)
         ├─ Si HMI conectado → envía inmediatamente
         │    "CTRL,ALM,<id>,<tipo>,<descripción>,1\n"
         └─ Si HMI NO conectado → encola en pending_alarms[]
              (máx 10 alarmas pendientes)
                   │
                   ▼
           Al reconectar → sendPendingAlarms()

Display_HMI recibe "CTRL,ALM,..."
    ├─ processReceivedAlarm()
    ├─ alarmsMuted = false (nueva alarma activa desmutea)
    ├─ update_alarm_panels() → muestra card roja en UI
    └─ AlarmSound_Update() → reproduce audio de alerta
```

---

## 4. Representación en el Display

- Hasta **4 alarmas simultáneas** visibles en pantalla.
- Cada alarma muestra: tipo (título corto) + descripción larga.
- Alarmas activas = card con fondo rojo.
- Alarmas resueltas = card con fondo gris/verde.
- El usuario puede **mutear** alarmas desde la UI (switch de mute).
- El mute se cancela automáticamente cuando llega una **nueva** alarma activa.

---

## 5. Soporte Multiidioma

Las alarmas envían su texto en el idioma activo del motherboard:

| Alarma | Español | English | Français |
|---|---|---|---|
| `TEMPERATURE_ALARM` | TEMP MUY ALTA | TEMP VERY HIGH | TEMP TRES ELEVEE |
| `HUMIDITY_ALARM` | ERROR HUMEDAD | HUMIDITY ERROR | ERREUR HUMIDITE |
| `AIR_THERMAL_CUTOUT_ALARM` | CORTE TERMICO AIRE | AIR THERMAL CUTOUT | COUPURE THERMIQUE AIR |
| `SKIN_THERMAL_CUTOUT_ALARM` | CORTE TERMICO PIEL | SKIN THERMAL CUTOUT | COUPURE THERMIQUE PEAU |
| `AIR_SENSOR_ISSUE_ALARM` | ERROR SENSOR AIRE | AIR SENSOR ERROR | ERREUR CAPTEUR AIR |
| `SKIN_SENSOR_ISSUE_ALARM` | ERROR SENSOR PIEL | SKIN SENSOR ERROR | ERREUR CAPTEUR PEAU |
| `FAN_ISSUE_ALARM` | ERROR VENTILADOR | FAN ERROR | ERREUR VENTILATEUR |
| `HEATER_ISSUE_ALARM` | ERROR CALENTADOR | HEATER ERROR | ERREUR CHAUFFAGE |
| `POWER_SUPPLY_ALARM` | ERROR ALIMENTACION | POWER SUPPLY ERROR | ERREUR ALIMENTATION |

Cuando el idioma cambia (desde HMI), el motherboard re-envía todas las alarmas activas con `resendActiveAlarms()`.

---

## 6. Configuración de Umbrales

| Parámetro | Valor | Definición |
|---|---|---|
| `TEMPERATURE_ERROR` | ±1.0 ºC | Margen para `TEMPERATURE_ALARM` |
| `TEMPERATURE_ERROR_HYSTERESIS` | 0.05 ºC | Histéresis de reset |
| `HUMIDITY_ERROR` | ±12 %RH | Margen para `HUMIDITY_ALARM` |
| `HUMIDITY_ERROR_HYSTERESIS` | 5 %RH | Histéresis de reset |
| `AIR_THERMAL_CUTOUT` | = `AIR_TEMPERATURE_SET_MAX` = 37 ºC | Corte absoluto aire |
| `SKIN_THERMAL_CUTOUT` | = `SKIN_TEMPERATURE_SET_MAX` = 37.5 ºC | Corte absoluto piel |
| `AIR/SKIN_THERMAL_CUTOUT_HYSTERESIS` | 0.2 ºC | Histéresis de reset |
| `MINIMUM_SUCCESSFULL_SENSOR_UPDATE` | 20000 ms | Timeout para alarmas de sensor |
| `ALARM_TIME_DELAY` | 30 min | Retardo silencioso post-corte térmico |

---

## 7. Alarmas de Fuente de Alimentación

Monitoreadas por `INA3221` (sensor de corriente/voltaje dual):
- Activadas solo si `HW_NUM >= 13`
- Periodo de verificación: `POWER_SUPPLY_CHECK_PERIOD`
- Condición: `system_voltage` fuera de `[MIN_SYSTEM_VOLTAGE_TRIGGER, MAX_SYSTEM_VOLTAGE_TRIGGER]`
