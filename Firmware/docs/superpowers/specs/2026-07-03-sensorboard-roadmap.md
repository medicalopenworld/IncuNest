# SensorBoard — Roadmap de fases (SDD)

**Fecha:** 2026-07-03
**Estado:** Borrador — pendiente de revisión
**Hardware:** ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM Octal)

---

## Propósito de este documento

Vista de conjunto de las 5 fases planificadas para el firmware del SensorBoard. Cada fase se diseñará e implementará por separado (brainstorming → spec → plan → subagent-driven-development), pero este documento fija el orden, el alcance de cada una y cómo encajan sobre la base de comunicación construida en la Fase 1. No sustituye a los specs de fase — es el mapa que los conecta.

**Principio de diseño transversal:** la Fase 1 dejó una capa de transporte (`usb_comm`) agnóstica al contenido. Ninguna fase futura debería tocar el framing, el CRC ni las tareas RX/TX — solo añaden nuevas tareas FreeRTOS que llaman a `sensorBoard_comm_send_json()` (o `send_binary()` en Fase 5) con su propio payload. Si una fase necesita tocar `usb_comm`, es una señal de que el diseño de esa fase se está saliendo de su capa.

---

## Estado de las fases

| Fase | Nombre | Estado | Sensor / hardware |
|---|---|---|---|
| 1 | Comunicación USB CDC | ✅ Completada | — (capa de transporte) |
| 2 | Sensores ambientales | ⏳ Pendiente | SHT40 (temp/humedad), ALS (luz) |
| 3 | Micrófono / nivel de sonido | ⏳ Pendiente | Micrófono I2S (dBA) |
| 4 | Sensor de puerta | ⏳ Pendiente | GPIO + interrupción |
| 5 | Cámara | ⏳ Pendiente | Módulo cámara (JPEG) |

---

## Fase 1 — Comunicación USB CDC (completada)

**Qué construyó:**
- Framing binario `Magic(2B) + Type(1B) + Length(4B LE) + Payload(NB) + CRC16(2B)`, con `TYPE=0x00` para JSON (fases 1–4) y `TYPE=0x01` reservado para JPEG binario (Fase 5) — sin necesidad de cambiar el protocolo más adelante.
- CRC16-CCITT FALSE (poly 0x1021, init 0xFFFF) calculado sobre Type+Length+Payload.
- Dos tareas FreeRTOS (`usb_rx_task`, `usb_tx_task`, prioridad 5) + `tx_queue` (8 items) como único punto de entrada para enviar datos a la motherboard.
- `esp_log_set_vprintf()` redirige todo `ESP_LOG` a frames JSON `{"type":"log",...}`.
- Comando `status` → responde con `device`, `fw`, `uptime`. Comando desconocido → `status:"error"`.
- API pública mínima para todo lo que viene después: `sensorBoard_comm_init()`, `sensorBoard_comm_send_json()`, `sensorBoard_comm_send_binary()` (stub).

**Por qué importa para las fases siguientes:** ninguna fase 2-5 necesita reabrir `usb_comm.c`. Solo consumen `send_json()`/`send_binary()`.

---

## Fase 2 — Sensores ambientales (SHT40 + ALS)

**Objetivo:** lectura periódica de temperatura, humedad relativa y luz ambiental, publicada como eventos JSON.

**Hardware:** sensor SHT40 (I2C, temperatura + %RH) y sensor ALS (luz ambiental, I2C o ADC — a confirmar en el diseño de fase).

**Qué añade:**
- Driver I2C compartido (bus init una sola vez, ambos sensores lo comparten si están en el mismo bus).
- Nueva tarea FreeRTOS `sensor_task`: polling cada N segundos (valor a decidir en diseño, ej. 5-10s), lee ambos sensores, publica:
  ```json
  {"type":"event","cmd":"sensor_data","data":{"temp":37.5,"hum":60.2,"lux":320},"ts":5200}
  ```
- Extiende la respuesta de `status` (ya previsto en el spec de Fase 1) para reportar disponibilidad de sensores:
  ```json
  {"type":"resp","cmd":"status", ..., "sensors":{"sht40":true,"als":true,"mic":false,"door":false,"cam":false}}
  ```
- Manejo de fallo de sensor (I2C NACK / timeout): reportar `sensors.sht40:false` en lugar de bloquear la tarea o crashear.

**No toca:** `usb_comm`, framing, CRC.

**Dependencias:** ninguna de fases posteriores depende de esta, pero el campo `sensors` en `status` es el patrón que Fases 3-5 seguirán (cada sensor nuevo añade su clave).

---

## Fase 3 — Micrófono / nivel de sonido (dBA)

**Objetivo:** monitorización de ruido ambiental (relevante en un contexto de incubadora: dBA dentro de rango seguro).

**Hardware:** micrófono MEMS I2S (pinout y modelo a confirmar en diseño de fase).

**Qué añade:**
- Driver I2S (lectura de audio en modo streaming).
- Nueva tarea `audio_task`: captura muestras, calcula nivel RMS → conversión a dBA (requiere calibración/referencia — punto a decidir en diseño).
- Publicación periódica o basada en umbral:
  ```json
  {"type":"event","cmd":"sound_level","data":{"dba":42.3},"ts":8100}
  ```
- Decisión pendiente de diseño: ¿muestreo periódico simple, o detección de eventos (pico de ruido) con reporte inmediato? Afecta el diseño de la tarea y el uso de CPU/DMA.

**No toca:** `usb_comm`. Reutiliza `sensors.mic` en `status`.

**Riesgo a validar en diseño:** uso de CPU/DMA del bus I2S en paralelo con USB CDC y sensores I2C — confirmar que no hay contención de prioridades entre `audio_task` y `usb_rx_task`/`usb_tx_task` (ambas a prioridad 5).

---

## Fase 4 — Sensor de puerta

**Objetivo:** detectar apertura/cierre de puerta en tiempo real (evento discreto, no polling).

**Hardware:** sensor de puerta por GPIO (switch magnético o similar — a confirmar en diseño), con interrupción por flanco.

**Qué añade:**
- ISR en el GPIO del sensor, con debounce (software o hardware, a decidir).
- La ISR encola el evento (no puede llamar directamente a `send_json` desde contexto de interrupción — necesita una tarea o cola intermedia, igual que ya usa `usb_rx_task` con su semáforo).
- Eventos publicados:
  ```json
  {"type":"event","cmd":"door_open","ts":5100}
  {"type":"event","cmd":"door_closed","ts":5300}
  ```

**No toca:** `usb_comm`. Reutiliza `sensors.door` en `status`.

**Punto de diseño clave:** decidir el mecanismo ISR→tarea (semáforo binario dedicado, o reusar patrón de cola tipo `tx_queue`) para no bloquear la ISR con operaciones no seguras en ese contexto (logging, JSON, etc. deben ocurrir fuera de la ISR).

---

## Fase 5 — Cámara (captura y transferencia JPEG)

**Objetivo:** capturar imágenes JPEG y transferirlas a la motherboard vía USB, para inspección visual remota del interior de la incubadora.

**Hardware:** módulo de cámara (a confirmar: probablemente OV2640/OV5640 vía `esp32-camera`/`esp_camera` IDF-native). Justifica la elección de PSRAM 8MB OPI hecha en Fase 1 — los frame buffers de imagen viven en PSRAM.

**Qué añade (única fase que sí toca `usb_comm`, de forma acotada):**
- Activar `TYPE=0x01` en `usb_tx_task` — el framing ya lo soporta desde Fase 1, solo falta que el tx_task sepa manejar payloads grandes (un JPEG QVGA puede ser 10-30KB, muy por encima de `SB_PROTO_MAX_JSON_PAYLOAD`=256B usado para JSON).
- Implementar `sensorBoard_comm_send_binary()` (hoy stub, devuelve `ESP_ERR_NOT_SUPPORTED`): recibe puntero a buffer en heap/PSRAM + longitud, transfiere ownership a la cola de TX (no copia por valor como los JSON — el `tx_queue` actual asume payload fijo ≤256B, por lo que un item de tipo JPEG necesita su propio tipo de entrada con puntero + `free()`/liberación tras el envío).
- Nueva tarea `camera_task`: captura bajo demanda (comando `capture`) o continua (a decidir en diseño — probablemente bajo demanda para no saturar el enlace USB).
- Nuevo comando: `{"type":"cmd","cmd":"capture","id":N}` → SensorBoard responde `resp` de confirmación y luego envía el frame `TYPE=0x01` con los bytes JPEG.

**Importante — dirección del flujo:** el JPEG solo viaja SensorBoard → motherboard (TX). El decoder de RX (`usb_rx_task`) sigue procesando solo comandos JSON pequeños entrantes; su buffer fijo de 256B no necesita crecer. Solo el lado TX necesita manejar payloads grandes.

**Riesgo a validar en diseño:** tamaño máximo de imagen vs. tiempo de transferencia sobre USB CDC (throughput real), y qué pasa si `camera_task` pide un buffer más grande del que hay disponible en PSRAM en ese momento (backpressure / rechazo del comando `capture`).

---

## Orden de implementación y por qué

1. **Fase 2 antes que 3 y 4**: reutiliza el mismo patrón (I2C, polling periódico, extensión de `status`) que servirá de plantilla para las siguientes fases sensor-a-sensor.
2. **Fase 4 antes que 3** es defendible si la puerta es más simple (GPIO+ISR) que el pipeline de audio (I2S+DMA+cálculo dBA) — a decidir con el usuario si se quiere priorizar por simplicidad o por importancia clínica.
3. **Fase 5 al final**: es la única que toca `usb_comm` y la que más carga de diseño tiene (PSRAM, tamaño de payload variable, nuevo tipo de item en cola TX). Tiene sentido dejarla para cuando el resto de la base de sensores ya esté estable.

Este orden es una propuesta, no un compromiso — cada fase se confirma en su propio brainstorming antes de escribir el plan.

---

## Qué NO cubre este documento

- Pinout exacto de cada sensor (SHT40, ALS, micrófono, puerta, cámara): se captura en el brainstorming de cada fase, no aquí.
- Detalles de calibración (dBA, lux, etc.): decisión de diseño de fase, no de roadmap.
- Cambios en el firmware de la motherboard (parser del lado receptor): cada fase solo describe el lado SensorBoard; la motherboard debe extender su parser para los nuevos `cmd`/`event` según se implementen.

---

## Siguiente paso

Elegir qué fase se brainstorm-ea primero (Fase 2 por orden natural, o reordenar según prioridad) e iniciar `superpowers:brainstorming` para esa fase específica.
