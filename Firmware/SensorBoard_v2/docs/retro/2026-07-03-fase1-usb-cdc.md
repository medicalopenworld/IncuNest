# Retro — Fase 1 · Transporte USB CDC (`usb_comm`)

**Fecha de cierre:** 2026-07-03
**Change OpenSpec:** `openspec/changes/archive/2026-07-03-sb-phase1-usb-cdc/`
**Rama:** `feat/sb-phase1-usb-cdc` (encadenada; merge a `dev` pendiente de aprobación humana)

## Qué se hizo

Proyecto ESP-IDF v6 desde cero + componente `usb_comm` completo (framing binario con CRC16-CCITT, tareas RX/TX, dispatcher de comandos, logs como frames JSON, heartbeat), con 26 `TEST_CASE` Unity derivados de los escenarios OpenSpec. Loop completo: propose → design (ADR-0001) → red → green → verify → review (2 subagentes) → docs → archive.

## Aprendizajes

1. **El orden de publicación de recursos compartidos frente a `xTaskCreate` es una trampa sistemática.** Crear una tarea de prioridad mayor que la del creador (5 vs 1) significa que puede ejecutarse *antes* de la siguiente línea del creador; una global asignada "al final para protegerla" produjo una race real (`xQueueReceive(NULL)`). Regla práctica: todo lo que una tarea nueva lee debe estar publicado antes del `xTaskCreate` que la arranca.
2. **El patrón `snprintf`+`%s` con datos del enlace es el bug que las fases futuras heredarían.** La review de seguridad lo marcó como bloqueante no por su impacto inmediato sino porque la Fase 2-5 copiará la plantilla de emisión de eventos. Se cortó en Fase 1 con `sb_json_escape` + builders puros, y quedó como regla en `embedded.md`.
3. **Separar construcción de envío (`sb_cmd_build_*` vs `send_json`) convirtió escenarios "solo verificables con hardware" en tests de contenido exacto.** El acoplamiento a la cola TX era artificial; extraer la parte pura costó ~30 líneas y ganó 4 tests de spec.
4. **`double → uint32` con valores del emisor es UB, no wrap.** `{"id": -1}` o `{"id": 1e20}` bastan. Validar rango y NaN antes de convertir cualquier número JSON externo.
5. **Toolchain:** un ICE de GCC en `esp_lcd` (componente no usado) se resolvió con `set(COMPONENTS main)` — que además acelera cada build. La API de `esp_tinyusb` 2.x difiere del plan escrito en junio (1.x); leer los headers del managed component resuelto antes de codificar evitó un ciclo de errores.

## Diferido (seguimiento para fases futuras)

- Pool de allocador estático para cJSON (churn de heap con uptime de semanas) — evaluar en Fase 2, que añade más tráfico de parseo.
- Contador de frames TX descartados por DTR bajo (observabilidad).
- Confirmar en el firmware de la motherboard el contrato "sin heartbeat >90 s ⇒ SensorBoard no disponible" (documentado en README; fuera del alcance de este repo).
