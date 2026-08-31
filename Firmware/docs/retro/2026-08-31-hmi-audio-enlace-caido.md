# Retro — señal acústica del display al perder el enlace (2026-08-31)

Rama `feat/hmi-audio-enlace-caido`, worktree `Firmware/.worktrees/audio-enlace`.
Modalidad `auto`.

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
recupera.

Coste en esta iteración: ~6 ciclos de build perdidos antes de identificarlo, más
un rodeo entero intentando copiar `libdeps` entre worktrees (no funciona:
PlatformIO ejecuta "Removing unused dependencies" y los borra en cada
invocación).

Se verificó fijando el equivalente de git
(`https://github.com/thingsboard/pubsubclient.git#v2.9.4`) y revirtiendo
`platformio.ini` antes de commitear. **No es parte de este cambio y sigue
abierto** — merece su propia rama, porque hoy un clon limpio no compila.

Nota adicional para futuros worktrees: el fichero `shared.pio-link` de
`.pio/libdeps/` lleva dentro un `"cwd"` **absoluto**. Copiarlo de otro worktree
hace que `symlink://../shared` resuelva al `shared/` equivocado, y el síntoma es
desconcertante: los headers viejos se encuentran y solo falla el nuevo.

### 3. Sesiones en paralelo sobre la misma rama (SEGUNDA ocurrencia)

Ya estaba anotado que HEAD y el índice se mueven bajo los pies. Esta vez fue más
lejos: apareció en la rama un commit que esta sesión no hizo, con el trabajo del
working tree, y `tasks.md` se reescribió dos veces por debajo — una de ellas
introduciendo una **afirmación falsa** (que `IncuNest_V18` lleva un `lib_deps`
recortado que no hereda de `[common]`; en realidad hace `extends = common` y
compila en verde). Se detectó al releerlo y se corrigió en `7f5f8b7`.

**Regla que sale de aquí**: antes de `git add` de un artefacto que no acabas de
escribir tú en esa misma vuelta, reléelo y verifica sus afirmaciones contra el
código. Aplicado en `Firmware/.claude/rules/tooling.md`.

## Descartado, con motivo

- **Compartir el motor de audio entre las dos placas.** Tentador al ver dos
  generadores del mismo patrón, pero el de la motherBoard tiene estado, rampas
  de amplitud por PWM y `static_assert` que lo blindan; el del display es on/off
  puro. Compartir la *tabla* y la *aritmética* captura todo el valor; compartir
  el motor habría sido reescribir código que funciona.
- **Distinguir acústicamente la señal del display de la de la placa.** Se
  descartó en diseño: inventar un ritmo fuera de la Tabla 3 no es una opción, y
  la vía correcta si hiciera falta es la señal visual.

## Pendiente

- Verificación manual en banco: caso 17 de `docs/alarms_bench_verification.md`,
  todavía `⬜`. Es la única parte del cambio sin verificar.
- Medida acústica de los dos transductores y su nivel relativo (`alarms.md` §9).
- El fallo de construcción desde cero del punto 2.
