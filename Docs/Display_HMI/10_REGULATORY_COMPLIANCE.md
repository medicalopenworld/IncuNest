# Análisis de Cumplimiento Normativo — Display HMI

> Dispositivo médico Clase IIb — Incubadora Neonatal IncuNest  
> Análisis basado en el código real de `Firmware/Display_HMI` (rama `dev`, 2026-04-28)

---

## BLOQUE A — IEC 62304: Clasificación del Software y Ciclo de Vida

### A.1 Clasificación de Seguridad del Software (§4.3)

#### Análisis de contribución a situaciones peligrosas

El Display HMI de IncuNest realiza las siguientes funciones con impacto directo en la seguridad del neonato:

| Función HMI | Contribución a situación peligrosa | Resultado potencial |
|---|---|---|
| Mostrar temperatura del compartimento (INCUBATOR TEMPERATURE) | Si el display falla o muestra valor incorrecto, el operador no detecta hipertermia/hipotermia | Lesión grave irreversible / muerte |
| Mostrar alarma de sobretemperatura (TEMPERATURE_ALARM, ID:2) | Si la alarma no se muestra, el operador no responde | Lesión grave irreversible / muerte |
| Mostrar alarma de thermal cutout (AIR_THERMAL_CUTOUT, ID:3) | Si no se visualiza, compartimento puede seguir activo en modo emergencia | Lesión grave irreversible / muerte |
| Mostrar alarma de fallo de sonda de piel (SKIN_SENSOR_ISSUE, ID:6) | En BABY CONTROLLED, si el HMI no indica fallo de sonda, el PID pierde referencia | Lesión grave irreversible |
| Mostrar alarma de fallo del ventilador (FAN_ISSUE, ID:7) | Sin ventilador, la temperatura del compartimento puede dispararse | Lesión grave irreversible |
| Permitir ajuste de CONTROL TEMPERATURE | Un ajuste erróneo puede causar hipertermia (>38.5ºC) o hipotermia (<33ºC) | Lesión grave irreversible / muerte |
| Cambio de modo AIR→BABY CONTROLLED | Si se activa sin sonda válida, el PID de piel no tiene referencia | Lesión grave irreversible |
| Silenciar alarma (MUTE) | Si se silencia indefinidamente sin solucionar la causa | Lesión grave irreversible |
| Mostrar modo de control activo (AIR/BABY) | Si el modo no es visible, el operador puede ajustar el parámetro equivocado | Lesión grave irreversible |

**Conclusión de clasificación**:

Dado que el Display HMI puede contribuir a situaciones peligrosas con resultado potencial de **lesión grave irreversible o muerte**:

> **CLASIFICACIÓN: CLASE C** (IEC 62304 §4.3.c)

Esta es la clasificación más alta posible. Aplican todos los requisitos del ciclo de vida del software definidos en la norma (§5.1 a §5.8 y capítulo 7).

**Nota sobre medidas externas de reducción de riesgo**:
La Motherboard implementa controles de seguridad independientes del HMI (thermal cutout hardware, límites de PWM, PID watchdog). Estas medidas reducen la consecuencia de un fallo del HMI, pero no la eliminan completamente. La clasificación **no puede reducirse** a Clase B o A porque el HMI es el único medio de notificación de alarmas visible para el operador durante la operación normal.

---

### A.2 Análisis de Carencias para Software Heredado (§4.4)

El código actual fue desarrollado sin seguir formalmente IEC 62304.

| Entregable IEC 62304 | Requerido por | ¿Existe? | Válido | Acción necesaria |
|---|---|---|---|---|
| Especificación de requisitos software | §5.2 | **Parcial** (este documento RF-xxx) | No (informal) | Formalizar como documento SRS antes del inicio del desarrollo |
| Arquitectura software documentada | §5.3 | **Parcial** (`Firmware/docs/architecture.md`) | Parcial | Completar con arquitectura ESP-IDF nueva, diagramas de componentes |
| Diseño detallado de unidades software | §5.4 | **No** | — | Crear diseño detallado por módulo (comm, ui, drivers, storage) |
| Implementación de unidades software | §5.5 | **Sí** (código fuente existe) | Sí | Añadir control de versiones formal + revisión de código |
| Verificación de unidades software | §5.6 | **No** | — | Crear plan de pruebas unitarias + ejecutar |
| Pruebas de integración del sistema | §5.7.5 | **No** | — | Crear y ejecutar suite de pruebas de integración |
| Análisis de riesgo software (HARA) | Cap. 7 | **No** | — | Crear FMEA de software antes de iniciar nueva implementación |
| Trazabilidad requisitos → pruebas | §5.7.4 | **No** | — | Implementar traceability matrix (req → test case → resultado) |
| Plan de gestión de configuración | §8.1 | **Parcial** (git + ramas) | Parcial | Documentar política de ramas, versiones, releases |
| Identificación de SOUP | §8.1.2 | **No** | — | Crear inventario SOUP formal (ver A.3) |
| Plan de mantenimiento | §6.1 | **No** | — | Documentar proceso de gestión de problemas post-release |
| Gestión de anomalías | §9 | **Parcial** (GitHub issues) | Parcial | Formalizar proceso de reporte, clasificación y resolución de defectos |

---

### A.3 Identificación de SOUP (Software of Unknown Provenance) (§8.1.2)

| Librería / Componente | Versión | Función | Fabricante/Fuente | ¿Documentada? | Anomalías conocidas relevantes |
|---|---|---|---|---|---|
| LVGL | 8.3.11 | Motor gráfico UI (widgets, rendering, touch) | LVGL LLC (MIT license) | **No** formalmente | Posibles race conditions sin mutex en multithread; `while(1)` en assert handler — documentado en este análisis |
| FreeRTOS | v10.x (vía IDF) | RTOS — tareas, semáforos, colas | Amazon / Espressif (MIT) | **No** formalmente | Priority inversion sin mutex con herencia de prioridad — mitigar con recursive mutex |
| ESP-IDF | 5.x (vía arduino-espressif32 53.03.10) | HAL, drivers RGB, USB CDC, NVS, WiFi | Espressif Systems (Apache 2.0) | **No** formalmente | Bugs conocidos en `esp_lcd_panel_rgb` bounce buffers < IDF 5.1 — usar IDF ≥ 5.1 |
| Arduino-ESP32 | 53.03.10 (pioarduino) | Capa de abstracción Arduino | Pioarduino / Espressif | **No** | Overhead no cuantificado; bugs en `millis()` bajo carga FreeRTOS alta |
| TAMC_GT911_Fixed | fork custom (commit desconocido) | Driver táctil GT911 | Fork de tamc-electronics | **No** | Sin changelog formal; el fork indica que la versión original tenía bugs no documentados |
| PCA9557-arduino | ^1.0.0 | Driver I2C expansor IO | maxpromer (MIT) | **No** | El dispositivo no fue encontrado en I2C scan del hardware v1.3/1.4 |
| thingsboard-arduino-sdk | v0.13.0 | SDK IoT ThingsBoard | ThingsBoard Inc. (Apache 2.0) | **No** | Varios CVEs en versiones anteriores de dependencias MQTT |
| ArduinoJson | 6.21.5 | Serialización JSON | bblanchon (MIT) | **No** | Sin anomalías críticas conocidas en 6.x |

**Para cada SOUP, requisitos impuestos según §5.3.3 y §5.3.4**:

- **LVGL 8.3.11**: Requisito funcional — render de UI ≤30ms/frame, manejo de touch ≤200ms latencia. Hardware requerido: ESP32-S3 ≥240 MHz + PSRAM ≥4MB. Historial: `LV_ASSERT_HANDLER=while(1)` causa WDT reset — **mitigación crítica requerida**.
- **FreeRTOS**: Requisito funcional — scheduling determinístico a 1000 Hz, mutex con timeout. Hardware: ESP32-S3 dual core. Historial: Priority inversion documentada si no se usan mutexes con herencia.
- **ESP-IDF esp_lcd**: Requisito funcional — DMA RGB sin tearing, bounce buffers estables. Requiere IDF ≥5.1.0 y `CONFIG_LCD_RGB_RESTART_IN_VSYNC=y`.

---

### A.4 Procedimiento para Identificación de Defectos Comunes (§5.1.12)

Para la nueva implementación ESP-IDF, el plan de desarrollo DEBE incluir:

**Categorías de defectos comunes en C embebido a verificar**:

1. **Buffer overflow**: Todos los `sprintf`/`snprintf` deben usar versiones bounded. Verificar `rxBuffer[512]` y parseo de cadenas de alarma.
2. **Race conditions en FreeRTOS**: Verificar que ninguna variable compartida entre UITask y CommTask se accede sin sincronización. Usar la herramienta ESP-IDF Thread Analyzer.
3. **Malloc en contexto de tarea de alta prioridad**: Verificar que `lv_obj_create()` no se llama desde CommTask (prio 4) — debe ser solo desde UITask.
4. **Uso de `volatile` sin barrera de memoria**: Reemplazar variables `volatile` compartidas entre tareas por accesos via cola FreeRTOS.
5. **Stack overflow de tareas**: Monitorizar con `uxTaskGetStackHighWaterMark()`. UITask actual usa 16 KB — verificar en campo con 8 pantallas.
6. **Punteros NULL en objetos LVGL**: Todos los accesos a `lv_obj_t*` deben verificar NULL antes de `lv_label_set_text()` y similares. El código actual tiene muchos checks `if (ui_XXX)` — mantenerlos todos.

---

## BLOQUE B — IEC 60601-1-8: Sistema de Alarmas

### B.1 Inventario de Condiciones de Alarma del Display HMI

| ID Alarma | Descripción | Prioridad IEC 60601-1-8 | Justificación §6.1.2 | Visual | Auditiva | Implementado |
|---|---|---|---|---|---|---|
| ALM-001 (ID:2) | Temperatura compartimento desviada ±1ºC del setpoint | **ALTA** | Inicio inmediato de daño (hipotermia/hipertermia), resultado grave irreversible | **Sí** (panel rojo, texto) | **Sí** (buzzer via STC8) | **Parcial** — visual sí, auditiva vía buzzer, sin símbolo IEC 60417 |
| ALM-002 (ID:3) | Sobretemperatura compartimento >38.5ºC (AIR CUTOUT) | **ALTA** | Inicio inmediato, resultado muerte | **Sí** | **Sí** | **Parcial** |
| ALM-003 (ID:4) | Sobretemperatura piel >37.5ºC (SKIN CUTOUT) | **ALTA** | Inicio inmediato, resultado grave irreversible | **Sí** | **Sí** | **Parcial** |
| ALM-004 (ID:5) | Fallo sensor de temperatura del aire | **ALTA** | Pérdida de control térmico | **Sí** | **Sí** | **Parcial** |
| ALM-005 (ID:6) | Fallo sonda de piel (desconectada o fuera de rango) | **ALTA** | Pérdida de control en BABY CONTROLLED | **Sí** | **Sí** | **Parcial** |
| ALM-006 (ID:7) | Fallo ventilador de circulación (pérdida de pulso RPM) | **ALTA** | §201.12.3.101 IEC 60601-2-19 | **Sí** | **Sí** | **Parcial** |
| ALM-007 (ID:8) | Fallo del calentador (caída de corriente) | **ALTA** | Pérdida de control térmico | **Sí** | **Sí** | **Parcial** |
| ALM-008 (ID:9) | Anomalía en alimentación eléctrica (INA3221) | **MEDIA** | §201.12.3.103 IEC 60601-2-19 | **Sí** | **Sí** | **Parcial** |
| ALM-009 (ID:1) | Desviación de humedad ±12% del setpoint | **BAJA** | Inicio retardado, resultado leve reversible | **Sí** | **Sí** | **Parcial** |

**Justificación de prioridades según Tabla 1 de IEC 60601-1-8**:

La Tabla 1 cruza "Inicio del daño potencial" × "Resultado potencial del fallo":
- ALM-001 a ALM-007: Inicio **inmediato** (minutos) × Resultado **muerte o lesión irreversible** → **ALTA**
- ALM-008: Inicio **retardado** (minutos-horas) × Resultado **grave** → **MEDIA**
- ALM-009: Inicio **retardado** × Resultado **reversible** → **BAJA**

**Estado "Parcial"**: Las alarmas están implementadas visualmente y con buzzer, pero **no usan los símbolos normalizados IEC 60417** (ver B.4) ni tienen prioridad diferenciada visualmente (todas se muestran igual independientemente de si son ALTA o MEDIA/BAJA).

---

### B.2 Requisitos de Señales de Alarma Visuales (§6.3.2)

| Requisito | Cláusula | ¿Cumple actual? | Carencia |
|---|---|---|---|
| Señales visuales visibles a 1m del operador | §6.3.2.2.2 | **Sí** — pantalla 7" es visible a 1m | OK |
| Alarmas ALTA distinguibles de MEDIA y BAJA | §6.3.2.3 | **No** — todas usan el mismo estilo visual | Implementar diferenciación visual por prioridad |
| Señales visuales NO desactivadas al silenciar audio | §6.8.1 | **Sí** — el código mantiene `alarmList[id].state` aunque `alarmsMuted=true` | OK |
| Estado RECONOCIDO indicado visualmente | Tabla 5, §6.8.5 | **No** — no hay estado "acknowledged" implementado | Implementar |
| Color: rojo para ALTA, amarillo para MEDIA | §6.3.2.3.a | **Parcial** — rojo usado, pero sin diferenciación MEDIA | Añadir amarillo para MEDIA |
| Frecuencia de destellos: ALTA >2 Hz | §6.3.2.3 | **TBD** — revisar código de animación de alarmas | Verificar frecuencia de parpadeo |

---

### B.3 Requisitos de Nivel Sonoro de Alarmas (§201.9.6.2.1 de IEC 60601-2-19)

| Requisito | Cláusula | Estado actual | Acción |
|---|---|---|---|
| Nivel sonoro dentro del compartimento ≤ 60 dB(A) en uso normal | §201.9.6.2.1.101 | **TBD** — el buzzer del CrowPanel genera ~80 dB a 10cm. Medir en el compartimento real | Medir y documentar con posicionamiento real de la placa |
| Alarmas auditivas ≥65 dB(A) a 3m (ajustable ≥50 dB(A)) | §201.9.6.2.1.102 | **TBD** — el nivel no es ajustable actualmente (solo ON/OFF via I2C) | Implementar control de volumen del buzzer |
| Cuando suena alarma, nivel dentro compartimento ≤80 dB(A) | §201.9.6.2.1.103 | **TBD** | Medir |

**Nota sobre control de volumen**: En el código actual hay definidas constantes de volumen (`AUDIO_VOLUME_MIN=0`, `AUDIO_VOLUME_MAX=21`, `AUDIO_VOLUME_DEFAULT=15`) y los comandos I2C correspondientes, pero el `AudioManager.cpp` está **deshabilitado** en la build actual. El buzzer hardware del STC8H1K28 es controlado solo ON/OFF. No existe mecanismo de ajuste de volumen del buzzer por el operador.

---

### B.4 Estados de Desactivación de Alarmas (§6.8)

| Estado | Símbolo IEC 60417 | ¿Implementado? | ¿Correcto? | Carencia identificada |
|---|---|---|---|---|
| AUDIO PAUSADO (tiempo limitado) | IEC 60417-5576 variant | **No** — solo hay flag `alarmsMuted` | **No** | Implementar con indicador visual explícito y countdown de 30min |
| ALARMA PAUSADA | — | **No** | **No** | TBD según aplicabilidad |
| AUDIO DESCONECTADO (permanente) | IEC 60417-5576 | **No** | **No** | Implementar si se permite silencio permanente (requiere justificación §6.8.3) |
| RECONOCIDO temporal | Campana + líneas discontinuas | **No** | **No** | Implementar estado acknowledge |
| RECONOCIDO indefinido | Campana + cruz | **No** | **No** | Implementar según §6.8.5 |

**Código actual del botón Mute** (`src/CommTask.cpp:453-455`):
```cpp
if (alarm.state && !wasActive) {
    alarmsMuted = false;      // Auto-desmutar al llegar nueva alarma
    hmi_msg.muteAlarm = 0;
}
```

El sistema actualmente desactiva el silencio cuando llega una nueva alarma — lo cual es correcto según §6.8.1. Sin embargo:
- No hay límite de tiempo en el silencio manual (debe ser máximo **30 min** según §201.12.3.104 para el período de warm-up)
- No hay indicación visual del estado "audio pausado" con símbolo normalizado IEC 60417

---

### B.5 Función de Test de Alarmas (§201.12.3.105)

**Estado actual**: **No implementada**.

No existe en el código del HMI ninguna función que permita al operador verificar el funcionamiento de las alarmas auditivas y visuales. 

**Requisito para nueva implementación**: En ScreenSettings o en el menú de servicio, añadir botón "TEST ALARMAS" que active visualmente y acústicamente una alarma de prueba con indicación explícita "MODO TEST — NO ES UNA ALARMA REAL".

---

### B.6 Registro de Alarmas (§6.12)

**Estado actual**: **No implementado**.

No existe ningún mecanismo de registro de ocurrencia de alarmas en la NVS/EEPROM del HMI. La Motherboard puede registrar eventos en flash via `DriveUpload.cpp` (Google Drive), pero no específicamente el HMI.

**Requisito para nueva implementación**: Implementar en la NVS del HMI un buffer circular de últimas 50 alarmas con: ID, tipo, descripción, estado (activa/resuelta), timestamp relativo (segundos desde boot). Los registros NO deben ser editables por el operador.

---

## BLOQUE C — IEC 60601-2-19: Requisitos Específicos de Incubadora

### C.1 Indicaciones Obligatorias en Pantalla (§201.12.2)

| Indicación | Requisito | ¿Implementado? | Observaciones |
|---|---|---|---|
| Temperatura del compartimento (INCUBATOR TEMPERATURE) | §201.12.1.105 — termómetro independiente del control | **Sí** — `ui_TempAirDetected` muestra `airTempValueDetected` | No es de mercurio (digital) — conforme |
| Temperatura de piel del bebé (SKIN TEMPERATURE) | §201.12.2.101 — continua, rango mín 33ºC-38ºC | **Sí** — `ui_TempSkinDetected` muestra `skinTempValueDetected` | Solo visible si `skinPanelEnabled=true` |
| Modo de operación activo (AIR CONTROLLED / BABY CONTROLLED) | §201.12.2.102 — indicación clara del modo | **Sí** — `ui_Label31`/`ui_Label30` muestran AIR/SKIN | Debe ser inequívoco — **verificar legibilidad a 1m** |
| CONTROL TEMPERATURE (setpoint) | §201.7.4.2 — intervalos ≤0.5ºC (aire) / ≤0.25ºC (piel) | **Parcial** — `TEMP_INCREMENT = 0.2ºC` para ambos modos | **Incumplimiento**: el incremento de 0.2ºC cumple ≤0.5ºC para AIR pero cumple también ≤0.25ºC para SKIN (0.2 < 0.25). OK. |
| Humedad relativa (si aplica) | §201.12.1.109 — precisión ±10% HR | **Sí** — `ui_HumDetected` muestra `humValueDetected` | Solo visible si `humidityEnabled=true` |
| Concentración O₂ (si aplica) | §201.12.1.110 | **No aplicable** — IncuNest no tiene control de O₂ | OK — funcionalidad no presente |

---

### C.2 Marcado y Advertencias en Pantalla (§201.7.2)

| Advertencia | Requisito | Estado |
|---|---|---|
| Monitor de oxígeno cuando se administra O₂ | §201.7.2.101 | **No aplicable** — sin control de O₂ |
| Advertencia de superficie caliente | §201.7.2.102 | **TBD** — la pantalla no muestra advertencia de superficie caliente del calentador |
| Indicación cuando CONTROL TEMP > 37ºC (override hasta 39ºC) | §201.15.4.2.2.101 | **TBD** — `AIR_TEMP_MAX = 38.5ºC` en el código. Verificar si hay indicación visual especial sobre 37ºC |

---

### C.3 Control de Temperatura en Pantalla (§201.7.4.2)

| Requisito | Estado | Evidencia en código |
|---|---|---|
| Rotación horaria → incremento de temperatura | **TBD** — el HMI usa flechas arriba/abajo, no control rotatorio | Ver `TEMP_INCREMENT = 0.2ºC` en `include/main.h:134` |
| Marcado en intervalos ≤0.5ºC (AIR) | **Sí** — 0.2ºC < 0.5ºC | `TEMP_INCREMENT = 0.2` |
| Marcado en intervalos ≤0.25ºC (BABY) | **Sí** — 0.2ºC < 0.25ºC | `TEMP_INCREMENT = 0.2` |
| Rango AIR: mín ≥30ºC, máx ≤38ºC | **Sí** | `AIR_TEMP_MIN=30.0`, `AIR_TEMP_MAX=38.5` |
| Rango BABY (piel): mín ≥33ºC, máx ≤38ºC | **Parcial** | `SKIN_TEMP_MIN=35.0` (>33ºC), `SKIN_TEMP_MAX=37.5` |

---

### C.4 Persistencia de Ajustes ante Corte de Alimentación (§201.11.8)

**Requisito**: Una interrupción y restauración de alimentación de hasta 10 min NO debe cambiar el CONTROL TEMPERATURE ni otros valores preajustados.

**Estado actual**:
- **Sí implementado**: Los setpoints se almacenan en NVS/EEPROM (`EEPROM_DESIRED_AIR_TEMP`, `EEPROM_DESIRED_SKIN_TEMP`).
- El HMI detecta warm resets via `g_hmiRestoreState = (rst != ESP_RST_POWERON && rst != ESP_RST_BROWNOUT)` y restaura el estado anterior.
- **Riesgo**: Si se produce un corte y el NVS no ha hecho commit de los últimos cambios (delay 20s en `INACTIVITY_TIMEOUT_MS`), los cambios del operador en los últimos 20s se perderían. **Carencia menor**.

---

### C.5 Indicación del Tiempo de Calentamiento Warm-Up (§201.7.9.2.8)

**Estado actual**: **TBD** — No hay pantalla explícita de "warm-up en progreso" con cuenta regresiva. La Motherboard gestiona el período de warm-up y durante ese tiempo puede suprimir ciertas alarmas durante 30 min. El HMI no indica visualmente que el sistema está en warm-up.

**Requisito para nueva implementación**: Añadir indicador de estado "CALENTAMIENTO EN PROGRESO" con timer regresivo visible en el dashboard principal durante el período de warm-up (recibido de la Motherboard como campo adicional en `CTRL,STATE`).

---

## BLOQUE D — IEC 62366-1: Ingeniería de Usabilidad

### D.1 Especificación de Uso (§5.1)

| Elemento | Descripción derivada del código actual |
|---|---|
| **Uso previsto** | Monitorización y ajuste de parámetros de la incubadora neonatal por personal sanitario en UCI neonatal. Funciones: ajuste de temperatura objetivo, selección de modo AIR/BABY, activación de fototerapia, gestión de alarmas. |
| **Usuarios previstos** | Personal sanitario (enfermeras UCI neonatal, médicos neonatólogos). Nivel de formación esperado: profesional sanitario titulado con formación en uso de incubadoras. Puede usar guantes médicos al tocar la pantalla. |
| **Entorno de uso** | UCI neonatal: iluminación variable (potencialmente alta con fotos de diagnóstico), ruido ambiente elevado (>60 dB), estrés operativo, turnos de 12h, situaciones de urgencia frecuentes. |
| **Partes de la interfaz** | Pantalla táctil capacitiva 7" (800×480), buzzer integrado, indicadores visuales en pantalla. Sin botones físicos en el HMI (solo botones en el display CrowPanel — sin usar en la UI actual). |

---

### D.2 Características de la Interfaz Relacionadas con Seguridad (§5.2)

| Característica UI | Riesgo de USE ERROR | Gravedad potencial | Medida de control en UI actual |
|---|---|---|---|
| Ajuste de CONTROL TEMPERATURE (flechas arriba/abajo) | Tocar accidentalmente múltiples veces, confundir AIR vs SKIN setpoint | Grave — hipertermia/hipotermia | Flechas requieren toque en zona específica; no auto-repeat sin confirmación — **TBD verificar** |
| Cambio de modo AIR→BABY CONTROLLED (Switch4) | Activar sin sonda válida presente | Grave — pérdida de control PID | Código verifica `SKIN_PROBE_VALID` antes de permitir. **Sí implementado** |
| Silenciar alarma (botón MUTE) | Silenciar indefinidamente sin resolver causa | Grave — alarma no detectada | Auto-desmuteo cuando llega nueva alarma. **Sin límite temporal** — CARENCIA |
| Resetear THERMAL CUT-OUT | Hacerlo sin investigar causa del cutout | Grave — cutout puede repetirse | **TBD** — verificar si el HMI permite reset de cutout |
| Ajuste de límites de alarma | Poner límites fuera del rango seguro IEC 60601-2-19 | Grave | **No implementado** — no hay ajuste de límites de alarma en la UI actual |
| Pantalla ilegible por parpadeo/reinicio | No ver alarma activa durante parpadeo | Grave — alarma no detectada | **CARENCIA CRÍTICA** — los parpadeos son el problema principal reportado |
| Activar fototerapia sin indicación de tiempo | UV prolongado causa daño ocular/cutáneo | Medio | Temporizador de fototerapia implementado |

---

### D.3 Escenarios de Uso Relacionados con Peligro (§5.4)

**HUS-001: Ajuste inadvertido de temperatura**
- **Tarea**: Operador ajusta humedad en la pantalla principal
- **Use error**: Al tocar el área de humedad, roza el botón de temperatura por proximidad de controles en pantalla táctil (Fat Finger en pantalla de 7")
- **Situación peligrosa**: Temperatura objetivo cambiada a valor peligroso sin confirmación explícita
- **Severidad**: Alta (lesión grave irreversible — hipertermia neonatal)
- **Medida de control UI requerida**: Separación física de controles ≥10mm, confirmación en 2 pasos para cambios de temperatura, zona de toque diferenciada con feedback visual

**HUS-002: Falla del display durante alarma activa**
- **Tarea**: Monitorización rutinaria por parte de la enfermera
- **Use error**: La pantalla se reinicia (watchdog WDT) mientras ALM-001 (TEMPERATURE_ALARM) está activa
- **Situación peligrosa**: El display se reinicia ~2s, durante los cuales las indicaciones visuales de alarma no son visibles. La enfermera puede no darse cuenta
- **Severidad**: Alta (alarma alta perdida → hipotermia neonatal no detectada)
- **Medida de control UI requerida**: Eliminar los reinicios (ver documento 05); al reiniciar, restaurar estado de alarma en <2s y marcar visualmente que hubo un reinicio; buzzer debe continuar sonando (gestionado por Motherboard — independiente del HMI)

**HUS-003: Silencio indefinido de alarma**
- **Tarea**: Operador silencia alarma de temperatura durante ajuste de paciente
- **Use error**: Olvida reactivar el audio. La alarma sigue silenciada durante horas
- **Situación peligrosa**: Si la temperatura vuelve a desviarse, el operador no recibe alerta auditiva
- **Severidad**: Alta (alarma auditiva alta inactiva → hipotermia/hipertermia no detectada a tiempo)
- **Medida de control UI requerida**: AUDIO PAUSADO con límite de tiempo máximo 30 min (§201.12.3.104), indicación visual explícita de "AUDIO PAUSADO" con countdown visible, auto-reactivación del audio al expirar

**HUS-004: Activación de modo BABY CONTROLLED sin sonda**
- **Tarea**: Enfermera cambia de modo AIR a modo BABY para recién nacido trasladado
- **Use error**: La sonda NTC no está correctamente colocada en la piel pero el switch visual del HMI permite activar el modo
- **Situación peligrosa**: El PID de temperatura de piel usa lecturas inválidas (0ºC o lecturas espurias) como referencia, causando máxima potencia al calefactor
- **Severidad**: Alta (hipertermia grave → lesión irreversible)
- **Medida de control UI requerida**: El código actual verifica `SKIN_PROBE_VALID` antes de permitir activar el modo BABY — **verificar que el feedback visual es suficientemente claro e inequívoco**

**HUS-005: Confusión entre temperatura detectada y temperatura objetivo**
- **Tarea**: Enfermera verifica parámetros del neonato
- **Use error**: Lee el valor de temperatura objetivo (CONTROL TEMPERATURE = 36.5ºC) como el valor medido real (DETECTED = 34.2ºC)
- **Situación peligrosa**: Cree que el bebé está a 36.5ºC cuando en realidad está a 34.2ºC (hipotermia)
- **Severidad**: Alta (hipotermia no detectada)
- **Medida de control UI requerida**: Diferenciación visual inequívoca entre TEMPERATURE DETECTED y CONTROL TEMPERATURE: tamaño de fuente diferente, label identificativo ("MEDIDA" vs "OBJETIVO"), posición separada en pantalla

---

### D.4 Evaluación Formativa Identificada (§5.8)

Los problemas conocidos documentados en `Firmware/docs/known_issues.md` representan fallos de la evaluación formativa implícita:

| Problema | Fallo de USE SPECIFICATION | Referencia |
|---|---|---|
| Parpadeos del display | Pérdida temporal de visibilidad de alarmas activas → fallo §5.1 (entorno de uso: no debe perderse información crítica) | `known_issues.md §1`, `display_config.h:94` |
| Reinicios inesperados | Pérdida de estado de alarma visible → posible incumplimiento §201.12.3.104 (alarma power interruption) | `lv_conf.h:271` (LV_ASSERT_HANDLER) |
| No respuesta al toque | USE ERROR facilitado por diseño — operador toca botón y no hay respuesta, repite la acción sin saber si fue registrada | `UITask.cpp:2911` (COLOR_DIVISOR=16) |
| Sin límite temporal en silencio | Permite silencio indefinido de alarmas — incumplimiento directo de §201.12.3.104 | `CommTask.cpp:454` |

---

### D.5 Requisitos de la Interfaz de Usuario Relacionados con Usabilidad (§5.6)

| ID Req UI | Descripción | Norma origen | Verificación |
|---|---|---|---|
| UI-001 | La temperatura del compartimento debe mostrarse de forma continua sin interrupciones por reinicios o parpadeos. Tiempo máximo sin actualización visible: 5s | §201.12.1.105 + §62366 §5.1 | Test de estabilidad 24h con registro de logs de reinicio (NVS boot count) |
| UI-002 | El cambio de CONTROL TEMPERATURE requiere confirmación explícita del operador (segundo toque en "CONFIRMAR" o retención de flecha >500ms) | §62366 HUS-001 | Test de usabilidad con 5 enfermeras UCI |
| UI-003 | Las alarmas visuales de PRIORIDAD ALTA no deben poder desactivarse accidentalmente. El botón Mute solo desactiva el audio, no las indicaciones visuales | §60601-1-8 §6.8.1 | Test funcional: activar alarma ALTA + pulsar Mute + verificar visuales |
| UI-004 | El modo de operación activo (AIR CONTROLLED / BABY CONTROLLED) debe ser visible en todo momento en cualquier pantalla (HUD permanente) | §201.12.2.102 | Inspección visual en todas las pantallas + test de campo |
| UI-005 | La pantalla debe responder al toque en menos de 200ms (desde el toque hasta cambio visual visible) | §62366 USE ENVIRONMENT estrés + HUS-003 | Test de latencia con instrumento de medida (cámara a 120fps) |
| UI-006 | No debe producirse pérdida de información de alarma activa durante warm-up. Si el HMI se reinicia, debe mostrar el estado de alarma en <3s | §201.12.3.104 + §62366 | Test funcional: inducir reinicio con alarma activa, medir tiempo hasta restauración |
| UI-007 | Los símbolos de estado de alarma deben seguir IEC 60417 y Tabla C.1/C.2 de IEC 60601-1-8: campana para activa, campana+rayas para reconocida, símbolo 5576 para audio pausado | §60601-1-8 §6.8.5 | Inspección: verificar iconos en `src/ui_img_mute_icon_png.c` vs símbolo IEC 60417-5576 |
| UI-008 | El audio PAUSADO debe tener duración máxima configurable de 30 min con indicación visual de countdown visible. El audio se reactiva automáticamente al expirar | §201.12.3.104 | Test funcional: silenciar alarma + esperar 30 min + verificar reactivación |
| UI-009 | Debe existir función de test de alarmas accesible desde el menú de servicio para verificar alarmas auditivas y visuales | §201.12.3.105 | Test funcional: ejecutar test + verificar activación de todas las alarmas |
| UI-010 | La temperatura detectada y la temperatura objetivo deben ser visualmente diferenciables a 1m con iluminación variable de UCI. Fuente mínima: 24pt para detectada, etiqueta identificativa | §62366 §5.1 USE ENVIRONMENT | Test de campo con 5 operadores a 1m de distancia |
| UI-011 | El registro NVS de las últimas 50 alarmas debe ser inaccesible e inalterable por el operador | §60601-1-8 §6.12 | Inspección de código: verificar que no hay botón "borrar historial" en la UI |
| UI-012 | Los controles de temperatura deben tener zona de toque mínima de 20×20mm (considerando uso con guantes médicos) | §62366 §5.1 (guantes) | Medición de dimensiones en pantalla + test con guantes médicos |

---

## BLOQUE E — Resumen de Brechas Normativas y Plan de Acción

### E.1 Tabla de Brechas (GAP ANALYSIS) del Display HMI Actual

| ID Brecha | Norma | Cláusula | Descripción del incumplimiento | Severidad | Acción requerida en nueva implementación |
|---|---|---|---|---|---|
| GAP-001 | IEC 62304 | §4.3 | Sin clasificación formal de seguridad del software (ahora identificada: Clase C) | **Alta** | Documentar clasificación Clase C antes de inicio del proyecto |
| GAP-002 | IEC 62304 | §5.2 | Sin especificación formal de requisitos software (SRS) | **Alta** | Este documento 06_ESPIDF_MIGRATION_REQUIREMENTS.md es el punto de partida; formalizar como SRS |
| GAP-003 | IEC 62304 | §5.7.5 | Sin registros de prueba del sistema formales | **Alta** | Crear plan de pruebas y ejecutar antes del release |
| GAP-004 | IEC 62304 | §8.1.2 | SOUP (LVGL, FreeRTOS, ESP-IDF, TAMC_GT911) no inventariados formalmente | **Media** | Completar tabla A.3 con versiones exactas y anomalías conocidas |
| GAP-005 | IEC 62304 | §9 | Sin proceso formal de gestión de anomalías de software | **Media** | Establecer proceso de bug tracking, clasificación y resolución |
| GAP-006 | IEC 60601-1-8 | §6.1.2 | Prioridades de alarma no asignadas según Tabla 1 ni diferenciadas visualmente | **Alta** | Implementar diferenciación visual por prioridad (rojo/naranja/amarillo) |
| GAP-007 | IEC 60601-1-8 | §6.8.5 | Estados de desactivación sin indicación visual conforme (sin símbolos IEC 60417) | **Alta** | Implementar símbolos IEC 60417-5576 para audio pausado, campana+líneas para reconocido |
| GAP-008 | IEC 60601-1-8 | §6.8.1 | AUDIO PAUSADO sin límite temporal máximo (debe ser ≤30 min según §201.12.3.104) | **Alta** | Implementar timer de 30 min con auto-reactivación y countdown visible |
| GAP-009 | IEC 60601-1-8 | §6.12 | Sin registro de alarmas en almacenamiento no editable | **Media** | Implementar log circular en NVS (50 entradas) |
| GAP-010 | IEC 60601-1-8 | §201.12.3.105 | Sin función de test de alarmas para el operador | **Media** | Añadir "TEST ALARMAS" en menú de servicio |
| GAP-011 | IEC 60601-2-19 | §201.12.2.102 | El modo AIR/BABY CONTROLLED no está presente como HUD permanente en todas las pantallas | **Alta** | Añadir indicador de modo en todas las pantallas (header permanente) |
| GAP-012 | IEC 60601-2-19 | §201.12.3.104 | Sin indicación de período de warm-up en pantalla | **Media** | Añadir indicador de warm-up con timer visible en dashboard |
| GAP-013 | IEC 60601-2-19 | §201.9.6.2.1 | Nivel sonoro del buzzer no medido ni documentado | **Alta** | Medir y documentar nivel dB(A) del buzzer en el contexto del equipo completo |
| GAP-014 | IEC 62366-1 | §5.1 | Sin USE SPECIFICATION formal documentada | **Alta** | Completar USE SPECIFICATION antes de diseñar nueva UI (ver D.1) |
| GAP-015 | IEC 62366-1 | §5.4 | Sin HAZARD-RELATED USE SCENARIOS formales | **Alta** | Los HUS-001 a HUS-005 identificados deben incluirse en la evaluación sumativa |
| GAP-016 | IEC 62366-1 | §5.8 | Sin evaluación sumativa formal | **Alta** | Ejecutar evaluación sumativa con usuarios reales (≥5 enfermeras UCI) antes de release |
| GAP-017 | Estabilidad HW | — | Parpadeos y reinicios del display impiden visibilidad continua de alarmas | **Alta** | Eliminar mediante arquitectura ESP-IDF nativa (documentos 05 y 07) |
| GAP-018 | IEC 62304 | §4.4 | No existe análisis de gaps formal para el software heredado | **Alta** | Este documento es el análisis de gaps inicial — formalizar y archivar |

**Total de brechas identificadas**: 18  
**Brechas de Severidad Alta**: 11  
**Brechas de Severidad Media**: 7

---

### E.2 Orden de Prioridad para la Nueva Implementación

Los requisitos normativos deben abordarse en el siguiente orden de prioridad durante la nueva implementación ESP-IDF:

**FASE 1 — Antes de iniciar el desarrollo** (Prerrequisitos regulatorios):
1. GAP-001: Documentar clasificación software Clase C
2. GAP-002: Formalizar SRS (este documento como base)
3. GAP-014: Completar USE SPECIFICATION formal
4. GAP-015: Documentar HUS completos para evaluación sumativa
5. GAP-004: Completar inventario SOUP formal

**FASE 2 — Durante el desarrollo** (Implementar en el nuevo firmware):
6. GAP-017: Eliminar parpadeos y reinicios (arquitectura ESP-IDF nativa)
7. GAP-011: HUD permanente con modo AIR/BABY en todas las pantallas
8. GAP-006: Diferenciación visual por prioridad de alarma (rojo/amarillo)
9. GAP-007: Símbolos IEC 60417 para estados de silencio de alarma
10. GAP-008: AUDIO PAUSADO con límite 30 min y countdown
11. GAP-005: UI-001 a UI-012 (requisitos de interfaz)

**FASE 3 — Verificación y validación**:
12. GAP-003: Ejecutar suite de pruebas formal
13. GAP-009: Implementar registro de alarmas en NVS
14. GAP-010: Función de test de alarmas
15. GAP-012: Indicador de warm-up
16. GAP-013: Medir y documentar nivel sonoro del buzzer
17. GAP-016: Ejecutar evaluación sumativa con usuarios reales
18. GAP-018: Archivar análisis de gaps como evidencia documental

> **Nota importante**: Las fases 1 y 3 son actividades de documentación/validación que no pueden ser suplidas solo por el firmware. Requieren la participación activa del equipo de regulación, médicos colaboradores y usuarios finales (enfermeras UCI neonatal) en el proceso formal de usabilidad IEC 62366-1.
