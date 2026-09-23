# Retro — señal acústica del display al perder el enlace (2026-08-31)

Rama `feat/hmi-audio-enlace-caido`, worktree `Firmware/.worktrees/audio-enlace`.
Modalidad `auto`.

> Este cambio se trabajó en más de una vuelta de sesión (reset de límite de
> uso, cambio de modelo por el usuario) y, según se documenta en el punto 3,
> con al menos una sesión en paralelo sobre la misma rama. Este documento junta
> los aprendizajes de ambas pasadas en un solo sitio en vez de dejar dos retros
> parciales.

## Qué se hizo

El display detectaba la pérdida de enlace pero delegaba la mitad audible en la
motherBoard — justo el extremo que puede estar muerto. Ahora emite su propio
patrón MEDIA de la Tabla 3, con pausa de audio local. Las constantes del patrón
y `ALARM_AUDIO_PAUSE_MS` se mudaron a `shared/`, y la temporización se extrajo
como función pura (`alarm_audio_pulse_on`) para que sí tuviera tests pese a que
Display_HMI no tiene entorno.

## Aprendizajes con coste observable

### 1. Los heredocs vuelven a corromper ficheros (SEGUNDA ocurrencia)

Ya estaba anotado como lección personal ("no generar código C con heredocs de
Python"). Esta iteración volvió a pasar, y esta vez con coste medible:

- `cat > shared.pio-link <<'EOF'` con un JSON que lleva rutas Windows escapadas
  (`c:\\Users\\...`) escribió `\Users` en vez de `\\Users` → `InvalidJSONFile`
  → un ciclo de build perdido diagnosticándolo.
- Además se usó un heredoc de Python para editar `motherBoard/include/main.h`.
  Salió bien —se leyó y escribió con `newline=''`— pero fue suerte, no diseño.

**Regla que sale de aquí**: cualquier fichero cuyo contenido lleve `\`, `$`,
backticks o `!` se escribe con `Write`/`Edit`, nunca por heredoc ni por `echo`.
Aplicado en `Firmware/.claude/rules/tooling.md`.

Se consideró un hook `PreToolUse` que bloqueara `cat > … <<` sobre ficheros de
código y se **descartó por ahora**: la regex tendría falsos positivos con los
usos legítimos (mensajes de commit multilínea, scripts de un solo uso en el
scratchpad) y un hook que estorba se acaba desactivando. Queda como propuesta si
hay una tercera ocurrencia.

### 2. El proyecto no se puede construir desde cero (hallazgo, no regresión)

En un worktree limpio, `pio run` falla al resolver
`thingsboard/TBPubSubClient@2.9.4`: ya no existe en el registro de PlatformIO
(`UnknownPackageError`). El worktree principal solo compila porque lo tiene
instalado desde antes; en cuanto PlatformIO decide reinstalar, lo borra y no lo
recupera — "Removing unused dependencies" corre en cada invocación y se lleva
por delante cualquier `.pio/libdeps/<env>` copiado de otro worktree para
intentar aislar la verificación, que además arrastra su propio problema:
`shared.pio-link` lleva dentro un `"cwd"` **absoluto**, así que
`symlink://../shared` resuelve contra el `shared/` del worktree de origen, no
el destino, y el síntoma es desconcertante — los headers viejos se encuentran
y solo falla el nuevo.

Coste en esta iteración: varios ciclos de build perdidos entre las dos pasadas
de sesión, reconstruyendo cachés de `libdeps` a mano antes de identificar la
causa real. Se verificó igualmente fijando el equivalente de git
(`https://github.com/thingsboard/pubsubclient.git#v2.9.4`) y `pio run -e
IncuNest_V17` en el worktree principal (que sí conserva la caché), revirtiendo
`platformio.ini` antes de commitear en ambos casos. **No es parte de este
cambio y sigue abierto** — merece su propia rama, porque hoy un clon limpio no
compila.

Ambas notas (heredocs sobre `shared.pio-link`, no copiar `libdeps` entre
worktrees) están en `Firmware/.claude/rules/tooling.md`.

### 3. Sesiones en paralelo sobre la misma rama (SEGUNDA ocurrencia)

Ya estaba anotado que HEAD y el índice se mueven bajo los pies. Esta vez fue
más lejos: apareció en la rama un commit que una sesión no hizo, con el trabajo
del working tree, y `tasks.md` se reescribió varias veces por debajo — en un
momento introduciendo una **afirmación falsa** (que `IncuNest_V18` lleva un
`lib_deps` recortado que no hereda de `[common]`; en realidad hace `extends =
common` y compila en verde). Se detectó al releerlo y se corrigió en `7f5f8b7`.
Más tarde, un `Write` sin releer primero el fichero en disco sobrescribió por
completo un retro ya commiteado por la otra pasada (`26f5516`) — el contenido
de ese commit se recuperó de `git show` y se fusiona aquí; nada se perdió
porque el historial de git lo conservaba, pero el riesgo era real.

**Regla que sale de aquí**: antes de `git add` de un artefacto que no acabas de
escribir tú en esa misma vuelta, reléelo y verifica sus afirmaciones contra el
código — y para un fichero potencialmente ya trackeado por otra sesión,
comprobar `git log --oneline -3` antes de asumir que el árbol de trabajo
refleja solo lo propio. Aplicado en `Firmware/.claude/rules/tooling.md`.

### 4. Un guardarraíl determinista llevaba roto para motherBoard desde que existe

`Firmware/.claude/hooks/run-affected-tests.sh` (Stop hook, compila la placa
afectada al cerrar sesión) llamaba `pio run -e main` para motherBoard —
`motherBoard/platformio.ini` no tiene ni ha tenido nunca ese entorno de forma
estable; sus entornos son `IncuNest_V16/V17/V18` y `native`. Confirmado en el
worktree principal, no es artefacto de esta rama:
`UnknownEnvNamesError: Unknown environment names 'main'` siempre.

La causa raíz estaba documentada de forma incorrecta en
`Firmware/.claude/rules/embedded-motherboard.md`, que afirmaba la existencia
de un alias `main` "estable" para motherBoard (`extends = env:IncuNest_V17`).
Existió en algún momento (`build(motherboard): añadir env "main" como alias de
la revisión de HW vigente`) y se retiró después sin que la regla se
actualizara — la desactualización de una regla llevó a codificar mal un hook.

**Por qué no lo disparó nadie antes**: `Firmware/.claude/` está en
`.gitignore` — el framework, hooks incluidos, es local al worktree principal y
no existe en ningún otro. Todo este cambio se trabajó en un worktree aislado
sin `.claude/` propio, así que el Stop hook nunca corrió contra este diff. El
bug es real y anterior a este cambio, solo se hizo visible al reconstruir a
mano lo que el hook debía automatizar.

**Corregido directamente** (guardarraíl determinista, no queda como
propuesta): `run_build()` en el hook recibe el entorno como parámetro
(motherBoard → `IncuNest_V18`); `embedded-motherboard.md` ya no afirma que
existe `main`.

## Descartado, con motivo

- **Compartir el motor de audio entre las dos placas.** Tentador al ver dos
  generadores del mismo patrón, pero el de la motherBoard tiene estado, rampas
  de amplitud por PWM y `static_assert` que lo blindan; el del display es on/off
  puro. Compartir la *tabla* y la *aritmética* captura todo el valor; compartir
  el motor habría sido reescribir código que funciona.
- **Distinguir acústicamente la señal del display de la de la placa.** Se
  descartó en diseño: inventar un ritmo fuera de la Tabla 3 no es una opción, y
  la vía correcta si hiciera falta es la señal visual.
- **Un hook `PreToolUse` contra heredocs** — ver punto 1.

## Pendiente

- Verificación manual en banco: caso 17 de `docs/alarms_bench_verification.md`,
  todavía `⬜`. Es la única parte del cambio sin verificar, y por eso el change
  se archivó (`openspec archive`) con esa tarea explícitamente abierta.
- Medida acústica de los dos transductores y su nivel relativo (`alarms.md` §9).
- El fallo de construcción desde cero del punto 2 — `thingsboard/TBPubSubClient@2.9.4`
  merece su propia rama.

## Cambios aplicados fuera de este commit (`Firmware/.claude/` no está versionado)

- `Firmware/.claude/rules/tooling.md` (nuevo): heredocs, "releer antes de
  confiar en un artefacto ajeno", y las tres notas de PlatformIO/worktree
  (no copiar `libdeps`, `shared.pio-link` con `cwd` absoluto, `TBPubSubClient`,
  y que motherBoard no tiene `-e main`).
- `Firmware/.claude/hooks/run-affected-tests.sh`: `run_build()` acepta el
  entorno de PlatformIO como parámetro; motherBoard pasa a `IncuNest_V18`.
- `Firmware/.claude/rules/embedded-motherboard.md`: ya no afirma que existe un
  entorno `main`.
