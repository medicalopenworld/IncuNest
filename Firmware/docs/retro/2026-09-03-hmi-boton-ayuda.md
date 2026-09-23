# Retro — botón de ayuda en el heading (2026-09-03)

Rama `feat/hmi-boton-ayuda`, worktree `Firmware/.worktrees/boton-ayuda`.
Modalidad `auto`. Cambio OpenSpec `hmi-boton-ayuda` (spec `hmi-help-center`).

## Qué se hizo

Botón `?` en el heading de `ui_ScreenMain` con menú modal de tres vías:
tutorial guiado sobre la UI real (overlay en `lv_layer_top()`, 19 pasos por
Main y Ajustes), QR del vídeo tutorial (`lv_qrcode`, que estaba habilitado y
sin usar) y contacto con soporte (informe de depuración + mensaje, publicado
como telemetría ThingsBoard cuando hay servidor y siempre disponible como QR
`mailto:` para el móvil). Solo Display_HMI; sin librerías nuevas; +65 KB de
flash. ADR-0001 fija por qué no SMTP.

Loop completo: propose → ADR → 3 commits `feat` → verify + review (calidad y
seguridad) + docs en paralelo → `fix: review feedback` → este retro. El
archive queda para después del banco (sección 6 de `tasks.md` sin marcar) y
el merge espera la aprobación del gate.

## Aprendizajes con coste observable

### 1. `UI_IsCriticalAlarmActive()` es una lista fija de ids (SEGUNDA ocurrencia)

El diseño copió el contrato de `TimeDialog_Poll()` ("una alarma crítica se
lleva la pantalla por delante") y la revisión de seguridad lo marcó como
bloqueante: esa función enumera siete ids y deja fuera cuatro condiciones de
prioridad ALTA (`ALARM_AIR_SENSOR_FAULT`, `ALARM_SKIN_SENSOR_FAULT_SKIN_MODE`,
`ALARM_AIR_OUTLET_BLOCKED`, `ALARM_MAINS_INTERRUPTION`). Un overlay que se
traga los toques no debe depender de ella.

Es la segunda vez que un overlay de solo lectura tropieza aquí: el comentario
de `TelemetryHistory::mustYield()` ya lo explica y usa
`UI_IsAnyAlarmActive() || Display_IsBoardLinkLost()`. Coste: un ciclo de
review + fix, y una spec que hubo que corregir.

**Aplicado**: regla en `Firmware/.claude/rules/embedded-display-hmi.md` — los
overlays sin información de alarma propia ceden con el criterio de
`TelemetryHistory::mustYield()`, no con `UI_IsCriticalAlarmActive()`, que
solo vale donde ya se usa y con su justificación clínica.

### 2. Eximir del auto-bloqueo sin tope tapa el banner de alarma

El banner de alarma solo se pinta en `ui_ScreenLock` (`alarm_banner_update`).
Eximir un overlay del auto-bloqueo indefinidamente (como hacía la primera
versión con la ayuda) puede impedir llegar a él durante horas. Ningún otro
modal del proyecto se eximía; era una excepción nueva. Se acotó con
`HELP_IDLE_TIMEOUT_MS` (3 min).

**Aplicado**: misma regla que el punto 1 (van juntas: quien exime del
auto-bloqueo tiene que saber dónde vive el banner). Primera ocurrencia, pero
con coste (bloqueante en review) y claramente generalizable.

### 3. ArduinoJson 6 copia `char[]` y `char*`, no `const char*`

`StaticJsonDocument<JSON_OBJECT_SIZE(4)>` con cuatro `char[]` estáticos habría
publicado un JSON truncado en silencio (solo la primera clave) y
`sendTelemetryJson()` habría devuelto `true`: la pantalla diría "registrada"
con una petición vacía. El comentario del código afirmaba justo lo contrario.
Lo cazó la revisión de seguridad; no habría salido en el build ni en el banco
sin mirar la consola de ThingsBoard.

**Aplicado**: nota en `embedded-display-hmi.md` (el SDK de ThingsBoard vive
en las dos placas, pero el HMI es donde se ha visto). Primera ocurrencia con
coste: un bug funcional invisible.

### 4. `tooling.md` afirmaba algo ya falso

La regla decía "hoy un clon limpio no compila" por `TBPubSubClient@2.9.4`. Ya
estaba arreglado en `dev` (`platformio.ini` apunta al git de pubsubclient) y
el build de referencia del worktree limpio salió verde a la primera. Una
regla obsoleta cuesta tiempo cada vez que alguien la cree.

**Aplicado**: corregida en `Firmware/.claude/rules/tooling.md`.

### 5. `docs/adr/0000-template.md` no existía

El agente `scribe` referencia esa plantilla y la serie `Firmware/docs/adr/`;
ni el directorio ni la plantilla existían (la única serie estaba en
`SensorBoard_v2/docs/adr/`). Se crearon en este cambio junto con ADR-0001.
Sin más acción: ya está resuelto en el repo.

### 6. La vía ThingsBoard se implementó y se retiró (alcance, no defecto)

Tras la revisión, el usuario simplificó el contacto a "un QR que lleve al
correo con el SN en el asunto". La vía ThingsBoard (petición pendiente,
publicación en la tarea WiFi, regla de correo en el servidor) y el formulario
con teclado se retiraron en un `refactor(hmi)`. Coste: dos fases de
implementación y su documentación, más los hallazgos de review que se
gastaron en esa vía (B3, R5). El diseño ya la señalaba como la que exigía
trabajo en el servidor.

**Descartado como regla**: en modo `auto` no hay gate de plan; la decisión
de transporte estaba explícita en proposal/design y el usuario la vio al
cerrar el loop, que es el momento previsto. Si se repite (una decisión de
alcance en el design que el usuario recorta al final), valdría la pena un
gate ligero de "confirmar decisiones de arquitectura del design.md" incluso
en `auto`. Primera ocurrencia: se anota, no se sistematiza.

## Descartado con motivo

- **`design.md` nombró una función que no existió en el código final**
  (`SupportRequest_Service()` frente a `supportRequestService()` en
  `Wifi_OTA.cpp`). Lo cazó `doc-keeper` al leer el código antes de escribir
  la doc, tal y como pide `tooling.md`. Primera ocurrencia, sin coste; no se
  sistematiza.
- **Tamaño del QR (300 vs 360 px del design)**: desviación del diseño al
  implementar, detectada por `qa-engineer` calculando px/módulo. Se corrigió a
  340. Primera ocurrencia; una regla "comprueba que el código respeta las
  cifras del design" es demasiado genérica para valer como convención.
- **Commits `feat` de las fases 3-5 juntos**: eran mutuamente dependientes
  (menú ↔ tutorial ↔ botón) y no compilaban por separado sin stubs. Se
  documentó en `tasks.md`. No hace falta regla: `commits.md` ya pide "una
  sola cosa coherente", y esto lo era.
- **Credenciales**: `Credentials.h` está protegido por hook y no se puede
  editar desde el agente; el patrón `#ifndef` en `Credentials_public.h` lo
  resolvió sin fricción. Ya lo documenta el propio `Credentials_public.h` y
  ahora el README de Display_HMI; no hace falta regla.

## Proceso

- Lanzar `qa-engineer`, `code-reviewer`, `security-reviewer` y `doc-keeper`
  a la vez tras el commit `feat` funcionó bien: cuatro informes en ~10 min,
  sin pisarse (los tres primeros son de solo lectura; `doc-keeper` solo toca
  `docs/`). La revisión de seguridad fue la que más valor aportó (tres
  bloqueantes reales); conviene mantenerla siempre, no solo cuando el cambio
  toca protocolo o alarmas.
- El agente `Explore` con un cuestionario de 11 puntos produjo un mapa que
  bastó para diseñar sin volver a leer ficheros grandes (UITask.cpp tiene
  5100 líneas). Repetible como patrón de arranque para features de HMI.
- La propuesta OpenSpec la escribió el orquestador con el contexto del mapa
  en vez de delegar a `product-manager`; se evitó re-explorar. Válido cuando
  la exploración ya está hecha en la misma sesión.
