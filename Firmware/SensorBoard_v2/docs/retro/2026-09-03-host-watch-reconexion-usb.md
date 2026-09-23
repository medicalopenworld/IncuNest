# Retro — Vigilante de host: reconexión USB sin cortar VBUS (`usb_comm`)

**Fecha:** 2026-09-03
**Rama:** `feat/sensorboard-usb-comm` (commits `cbb3faf` red · `d563483` green · `7a854e2` docs)
**Origen:** informe de banco — "cuando desconecto la SensorBoard y reconecto, no recupera la conexión".

## Qué se hizo

La primera versión del reinicio para re-enumerar (`719073a`, misma mañana) usaba el DTR como señal de host. Se comprobó en el código de TinyUSB que el DTR **solo** cambia ante `SET_CONTROL_LINE_STATE` y que, sin sensado de VBUS (`self_powered = false`), el DCD del ESP32-S3 no reporta `UNPLUGGED` sino `SUSPEND`. Es decir, el watchdog no podía dispararse en el caso que lo motivaba. Se cambió la señal a `DTR && tud_ready()`, se extrajo la política a `sb_host_watch` (función pura, 5 TEST_CASE `[hostwatch]`), se añadió el callback de eventos de bus de TinyUSB y un heartbeat inmediato al volver el host.

## Aprendizajes

1. **Una señal de control del protocolo no es una señal de presencia física.** El DTR dice "el host abrió el puerto", no "hay host". Cualquier detector de pérdida de enlace debe apoyarse en la capa que realmente ve el bus (SOF/suspend, montaje) y tratar el resto como pista adicional. Generalizable a cualquier enlace: "última vez que el otro lado me habló" vale más que "última vez que me dijo que se iba".
2. **Antes de dar por bueno un mecanismo de fail-safe, recorrer el camino del evento en el código del vendor, no en la intuición.** Diez minutos en `cdc_device.c` y `dcd_dwc2.c` bastaron para ver que el callback nunca se invocaría. Mismo patrón que la Fase 5 (leer `sccb-ng.c` evitó un rediseño): el coste de leer el managed component es siempre menor que el de una prueba fallida en placa.
3. **Un fix "compila pero no se ha probado en placa" debe llevar escrita su prueba de banco.** El commit anterior lo decía honestamente pero no describía cómo verificarlo; ESTADO.md ahora incluye la prueba concreta (alimentado, desenchufar ≥15 s, re-enchufar, qué log esperar).
4. **Política separada de transporte = testeable sin hardware.** La lógica de 15 s / "solo si hubo host" / rearme / vuelco de uint32 no necesita USB para probarse; extraerla a una función pura permitió TDD real en un componente que hasta ahora solo tenía tests de framing.

## Filtro de segunda ocurrencia

El aprendizaje 2 ya es segunda ocurrencia (Fase 5 + esta). Candidato a regla en `.claude/rules/embedded.md`: "antes de apoyar un fail-safe en un callback de un componente externo, localizar en su código quién lo invoca y cuándo". Se deja anotado para que `retro-improver` lo aplique en la próxima pasada sobre `.claude/` (no versionado en este worktree).

## Diferido

- Verificación en placa del vigilante y del heartbeat inmediato (bloqueante, ver ESTADO.md).
- Si la placa se alimenta del propio cable USB de la motherboard, este fix no aplica a ese caso y el fallo estaría en el host: comprobar con el log de la motherboard ("SensorBoard desconectado del USB" → "SensorBoard conectado por USB").
