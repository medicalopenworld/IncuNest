# Brief — Tarea 1: revisar el "autozoom" del plot PPG de la pantalla de bloqueo

Fecha: 2026-11-05 · Para: la sesión que haga la tarea · Origen: sesión "Bug HR 152/135 · ADR-0003 · organización de sesiones" (fork de "IncuNest – Pulsioximetría en HMI")

## Reglas de esta tarea

- Repo `C:\PRJ\MOW\IncuNest`, rama **`feat/hmi-pulse-oximetry`** (ya existe; `dev` + 7 commits, todos en GitHub, sin PR). Trabajar en esa rama, en la misma carpeta, sin worktree. Lo que salga de aquí entra en el mismo PR que la pantalla de pulsioximetría.
- Solo **V18** (`IncuNest_V18` en `platformio.ini`); no compilar V17 ni V19.
- **No tocar** `incunest_afe4490` (la librería es de Alex y se cambia en su propia sesión), PulseNest ni IncuTwin. Si la conclusión es "el zoom va en la librería", se escribe un brief en `Firmware/docs/notes/`, no código.
- Comentarios y documentación en el código en **inglés**; chat en español. Cambios mínimos.
- Commit, push y PR **solo cuando Alex lo pida**, enseñando antes qué se sube (está aprendiendo Git). Nunca leer ni pegar `Credentials.h` / `secrets.h`. El repo es público.
- Al terminar: entrada en `conversation_log.md` (raíz, ignorado por Git, siempre añadir al final) y actualizar este brief con la conclusión.

## Qué hay hoy (verificado 2026-11-05)

No existe ningún autozoom. La cadena es:

1. La librería entrega `AFE4490Data::ppg_disp` (float, A/A, dominio OT, media cero, salida del BPF). Magnitud típica ~1e-6 a 1e-5; es invariante a la ganancia (no salta cuando HGAC cambia RF).
2. `Firmware/motherBoard/src/tasks/CommTask.cpp:68-82`: escala de transporte **fija** `PPG_DISP_UNITS_PER_LSB = 1.6e-8f`. Se envía `CTRL,PPG,<byte>` a 25 Hz con `byte = 128 + round(ppg_disp / 1.6e-8)`, recortado a 0–255 (`CommTask.cpp:1600-1607`). El comentario dice que el valor está **derivado de la spec, no medido** ("TODO: re-validate on the bench with a real probe") y que es "Fixed on purpose — no adaptive scaling outside the library" (commit e3a71567 de Pablo, 2026-08-15, que quitó el adaptativo).
3. Ayuda de banco ya incluida (`CommTask.cpp:1608-1625`): cuenta recortes por segundo y, si hay, escribe en el log `[PPG] recorte N/M muestras en 1 s: revisar PPG_DISP_UNITS_PER_LSB` (solo con `LOG_PULSIOXIMETRY`; comprobar en `main.h` si está activo).
4. HMI: `ui_LockPPGChart` (`Display_HMI/src/ui/ElementsCreation.cpp:4386-4402`), 460×105 px, línea, rango Y fijo **0–255**; se alimenta en `UITask.cpp:~5319` desde `ctrl_ppg_msg`. Traza en cian (commit 8ccba040).
5. En la librería hay un candidato en backlog: `ppg_disp_norm`, ponderado por PI, rango [0..1] (`incunest_afe4490_spec.md:171` y `:4015`, `incunest_afe4490.h:224`). Si hace falta zoom, el sitio previsto es ese, no IncuNest.

Conclusión previa de la sesión madre (antes del fork): medir en banco la escala fija; si hace falta zoom, hacerlo en la librería.

## Qué hay que hacer

1. **Medir en banco con la escala actual** (V18 con 18.56.0; buscar la IP con `arp -a` por la MAC de la placa; OTA: `curl -u incunest:changeme -F file=@.pio/build/IncuNest_V18/firmware.bin http://<ip>/update`):
   - Simulador de paciente a varias amplitudes / PI (p. ej. PI 0,5 %, 2 %, 5 %, 10 %) y a 135 lpm, y un dedo real de adulto.
   - Para cada caso: ¿la traza llena bien el chart, queda plana o se recorta en los raíles? Fotografiar la pantalla o anotar la amplitud aproximada en px. Mirar si aparece el log `[PPG] recorte`.
   - Si hace falta un dato numérico, un contador temporal en `CommTask` del valor máximo/mínimo de `ppg_byte` por ventana de 10 s, expuesto en `/debug/state` o en el log, **que se retira antes de commitear**.
2. **Decidir** con los datos:
   - (a) La escala fija vale para el rango de PI neonatal esperado → solo ajustar `PPG_DISP_UNITS_PER_LSB` si hace falta y actualizar el comentario (quitar el TODO, dejar la medida y la fecha). Cambio mínimo en IncuNest.
   - (b) Hace falta zoom → **no** implementarlo en IncuNest (Pablo lo quitó a propósito y tiene razones: una escala adaptativa oculta cambios de PI y hace el trazo no comparable entre pacientes). Escribir `Firmware/docs/notes/<fecha>-brief-libreria-ppg-disp-norm.md` para la sesión de la librería con la propuesta (`ppg_disp_norm` ponderado por PI, qué rango, cómo se comporta en la aplicación/retirada de sonda) y cómo lo consumiría IncuNest (byte 0–255 a partir de [0..1]).
   - (c) Opción intermedia, solo si Alex la quiere: dos escalas fijas seleccionables (neonato / adulto) o una escala fija distinta. Discutirlo antes de tocar código.
3. Al terminar, **no abrir el PR**: Alex quiere abrirlo cuando esta tarea esté cerrada. Dejar escrito aquí el resultado y la recomendación.

## Contexto que puede hacer falta

- Pérdida de muestras ya arreglada (ADR-0003, `Firmware/docs/adr/0003-sin-csv-de-pulsioximetria-en-flash.md`): la V18 mide 500,0 muestras/s y 135,0 lpm con el simulador. Si durante esta tarea la HR vuelve a salir alta, algo escribe en flash: avisar.
- `PpgSnapshot` (`motherBoard/src/tasks/PpgSnapshot.cpp`) guarda 8 s a 50 Hz en RAM y lo publica por ThingsBoard cada 15 min con la misma escala; si se cambia `PPG_DISP_UNITS_PER_LSB`, revisar que siga coherente (hay un `static_assert` en `SPO2.h` que lo recuerda).
- Tarea 2 (pendiente, no es de esta sesión): política ante valores no fiables (SQI, congelación, tiempo hasta "--").

## Resultado

Cerrada el 2026-11-08 por la vía (b), sin la medida en banco del paso 1: dejó de hacer falta.

- El 7-nov Alex implementó en la librería (`incunest_afe4490` v0.116/v0.117, spec §5.9) lo que el brief pedía para (b): tres modos de `ppg_disp` (`NO_NORM` = el A/A de siempre, `NORM`, `NORM_PI`) con un seguidor de envolvente "SLEW" (ventana 1 s, subida ≤ 50 %/s, bajada τ = 1 s, margen 15 %, salida 0 sin sonda o en calentamiento). `NORM_PI` es el modo de producto: amplitud completa con PI ≥ 2 %, mitad con PI ≤ 0,5 %, logarítmico entre medias, así que la traza sigue avisando de una perfusión baja. Elegido en PulseNest (PPGDISPLAB) entre SLEW / INSTANT / MAXMIN / RMS.
- IncuNest 18.57.0 (esta rama, sin commitear aún): pin `#v0.117`, `afe.setPPGDispMode(NORM_PI)` en `initSPO2()`, `CommTask` pasa de `128 + ppg_disp / 1.6e-8` a `128 + ppg_disp × 127`; fuera `PPG_DISP_UNITS_PER_LSB` y el contador de recortes (nada que recortar ni medir). HMI sin cambios. `PROTOCOL.md` (CTRL,PPG), `SPO2.h`, `PpgSnapshot.h/.cpp` y docs actualizados. La regla de Pablo se mantiene: el zoom está en la librería, no en IncuNest.
- Verificado: `pio run -e IncuNest_V18` OK (lib 0.117.0 instalada), `pio test -e native` 820/820. Flasheada en la V18 por OTA el 2026-11-08 (`/get_fw_version` = 18.57.0).
- **Reabierta el 2026-11-08 en banco:** con el simulador, la traza tarda > 5 s en llenar el chart al aplicar la sonda. Causas en la librería (BPF de `ppg_disp` sin precargar al aplicar → el escalón de OT, ~400× el latido, entra en el seguidor y tarda ~7 s en caer con τ = 1 s; además PI NaN 6 s → A = 0,5). Alex lo lleva a la librería: brief `2026-11-09-brief-libreria-ppg-disp-arranque-lento.md`.
- **2026-11-09: librería v0.118 publicada** (alternativa A en todos los modos: el BPF de `ppg_disp` se reinicia sin sonda y se precarga en la primera muestra aplicada; traza completa ~1,2 s tras la aplicación; E descartada a propósito: A = 0,5 hasta que PI publica a los 6 s; sin API ni claves nuevas; test 10 en `test_ppgdisp`). Pin subido a `#v0.118` en `platformio.ini` (+ nota), referencias en `PROTOCOL.md`, `hw19.md`, `known_issues.md` y `PpgSnapshot.h`; `FWversion` se queda en 18.57.0 (nunca commiteada). `pio run -e IncuNest_V18` OK con 0.118.0+sha.c2a9d5c. Pendiente: OTA y repetir la prueba (PI bajo/alto; tiempo hasta la traza completa, y el salto a tamaño completo a los 6 s). Aparte, de la librería: compatibilidad del diezmado a 25 Hz (sample-and-hold) con el paso banda 0,5–20 Hz (Nyquist 12,5 Hz), que Alex deja para después.
- **Nuevo pendiente, tarea propia:** el snapshot de ThingsBoard (`PPG_snapshot_v1`) ahora lleva [-1, +1] en vez de A/A bajo la misma clave; decidir `_v2` o no, y qué quiere el dashboard. Alex quiere analizar ese subsistema antes de decidir.
- Aparte (ya anotado en la sesión del 5-nov): el muestreo a 25 Hz de un BPF de 0,5–20 Hz sin promediar puede dar aliasing; no lo cambia este trabajo.
