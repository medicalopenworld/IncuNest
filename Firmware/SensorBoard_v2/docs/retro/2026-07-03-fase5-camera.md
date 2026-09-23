# Retro — Fase 5 · Cámara OV2640 (`camera_sensor`) — cierre de EPIC-001

**Fecha de cierre:** 2026-07-03
**Change OpenSpec:** `openspec/changes/archive/2026-07-03-sb-phase5-camera/`
**Rama:** `feat/sb-phase5-camera` (encadenada sobre Fase 3; última del roadmap)

## Qué se hizo

`camera_sensor` (esp32-camera 2.1.7, SCCB compartiendo el bus I2C principal, captura bajo demanda con gate puro) + la única reapertura acotada de `usb_comm` prevista desde Fase 1: `send_binary` con ownership en PSRAM, cola binaria separada de profundidad 1, y registro de comandos por componente. 13 TEST_CASE nuevos.

## Aprendizajes

1. **Una cola FIFO única no puede dar garantías de latencia si mezcla tamaños dispares.** "El transporte tiene prio 5" no protegía a la telemetría de esperar 2 s detrás de un JPEG: la prioridad de tarea ordena CPU, no mensajes. Colas separadas por clase + drenado prioritario del JSON + cota de 1 binario en vuelo fue la solución barata; la limitación residual (un frame contiguo no es interrumpible) quedó documentada con su peor caso medible.
2. **Los fallback de memoria "por si acaso" son un riesgo, no una robustez**: caer de PSRAM a DRAM interna con 30-128 KB habría convertido la fragmentación de PSRAM (el fallo esperable a semanas de uptime) en fallos de malloc en código no relacionado. Fail-safe = descartar la captura, no competir por RAM crítica.
3. **"Busy" eterno es un fallo enmascarado.** Sin timeout, una cámara colgada en `fb_get` respondería busy para siempre — el gate con umbral de stall reporta el fallo real y baja `sensors.cam`. Patrón: todo estado transitorio necesita una salida temporal hacia un estado de fallo explícito.
4. **Verificar el vendor antes de diseñar evitó un rediseño**: leer `sccb-ng.c` del managed component confirmó que `sccb_i2c_port` + `pin_sccb_sda=-1` reutiliza el bus `i2c_master` sin adueñárselo (el `deinit` no destruye el bus compartido). La incógnita más arriesgada de la fase se resolvió en 10 minutos de lectura de código, sin prueba y error en hardware.
5. **Los límites de tablas estáticas deben derivarse en los tests** (`SB_CMD_REG_MAX` en bucle, no literales `c0..c3`): un test que codifica el límite a mano deja de medir lo que dice cuando el límite cambia.

## Diferido

- Verificación on-target de la Fase 5 completa (captura real, throughput, flood de capture, convivencia SCCB↔SHT40).
- Modelo de amenaza: re-evaluar la ausencia de autenticación en `capture` si el conector USB sale del chasis (riesgo aceptado documentado en README).
- Arrastrados: pool cJSON, recovery de bus I2C, calibraciones (ALS, dBA/ponderación A), contrato heartbeat/puerta en la motherboard.
