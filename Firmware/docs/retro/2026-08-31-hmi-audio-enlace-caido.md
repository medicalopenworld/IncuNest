# Retro — hmi-audio-enlace-caido (2026-08-31)

## Qué se construyó

El display gana señal acústica propia (patrón MEDIA de Tabla 3) para la
pérdida de enlace con la motherBoard, con pausa de audio local de 10 min.
Antes, esa condición era la única cuyo detector vive en el display pero cuya
mitad audible se delegaba entera en la motherBoard — el extremo que puede
estar muerto. Detalle completo en `proposal.md`/`design.md` de este cambio.

## Fricciones observadas

### 1. `pio pkg install --library <lib> -e <env>` puede reescribir `platformio.ini`

Al intentar reparar la resolución de `symlink://../shared` en un worktree
aislado, ejecuté `pio pkg install --library "symlink://../shared" -e
IncuNest_V18`. El comando no solo instaló esa librería: reformateó el
`platformio.ini` entero (CRLF→LF, espacios→tabs) y, en un momento de la
sesión, ese fichero llegó a mostrar `[env:IncuNest_V18]` con un `lib_deps`
recortado a solo `symlink://../shared` — cuando el fichero real (verificado
después en el worktree principal) usa `extends = common` sin override, y
hereda la lista completa. Sin `git diff` de por medio, ese estado transitorio
casi queda documentado como "V18 tiene un `lib_deps` roto que no hereda de
`[common]`" — una entrada de `docs/` **falsa** sobre un fichero de
configuración de un dispositivo médico.

**Coste real**: casi dos horas de sesión reconstruyendo cachés de `libdeps` a
mano para perseguir un síntoma que era, en parte, autoinfligido por mis
propios comandos de PlatformIO. Se corrigió (commit `7f5f8b7`) solo porque
antes de escribir la conclusión final volví a leer el `platformio.ini` real y
lo contrasté.

**Regla aplicada** (`.claude/rules/embedded-motherboard.md` — ver más abajo):
tras cualquier `pio pkg install`/`pio run` en un `platformio.ini` versionado,
comprobar `git diff -- platformio.ini` antes de fiarse de su contenido para
diagnosticar nada, y revertir si el único cambio es de formato.

### 2. La caché de `libdeps` de un `symlink://` no es portable entre worktrees

`shared.pio-link` (dentro de `.pio/libdeps/<env>/`) guarda una ruta `cwd`
**absoluta**, congelada en el momento de la instalación. Copiar
`.pio/libdeps/<env>` de un worktree a otro dentro de este proyecto (algo que
`superpowers:using-git-worktrees` invita a hacer para aislar una verificación
de build) hace que PlatformIO siga resolviendo `../shared` contra el `shared/`
del worktree **origen**, no el del worktree destino — silencioso: los headers
que ya existían en el origen siguen resolviendo bien, y solo falla el fichero
nuevo que solo existe en el destino, con un mensaje de error que apunta a
"falta una librería", no a "estás mirando el worktree equivocado".

**Coste real**: la mayor parte del tiempo perdido en el punto 1 viene de aquí
— cada copia de caché exigía además editar `shared.pio-link` a mano.

**Regla aplicada**: nueva nota en `.claude/rules/embedded-shared.md` (ver
más abajo) — es la zona del repo afectada (`shared/`, consumida vía
`symlink://` por las dos placas) y el patrón de worktrees paralelos es
recurrente en este proyecto (10+ worktrees activos en `Firmware/.worktrees/`
al momento de escribir esto).

### 3. `run-affected-tests.sh` (Stop hook) compila motherBoard con un entorno que no existe

`motherBoard/platformio.ini` no tiene `[env:main]` — sus entornos son
`IncuNest_V16/V17/V18` y `native`. El hook automático que debía proteger cada
cierre de sesión que tocara `motherBoard/**` o `shared/**` llevaba
`pio run -e main` para motherBoard, que **siempre** falla con
`UnknownEnvNamesError` — verificado en el worktree principal, no es un
artefacto de esta sesión. El guardarraíl determinista que la capa
`embedded-shared.md` cita como motivo para no compilar dos placas a mano
nunca pudo cumplir su función para motherBoard.

**Por qué no lo disparó nadie antes**: `.claude/` está en `.gitignore`
(línea 39) — el framework entero, hooks incluidos, es local al worktree
principal y no existe en ningún otro worktree. Todo el trabajo de este cambio
se hizo en un worktree aislado (`Firmware/.worktrees/audio-enlace/`) sin
`.claude/` propio, así que el Stop hook nunca tuvo ocasión de correr contra
mi diff. El bug es real y anterior a este cambio, pero solo se hizo visible
al construir manualmente lo que el hook debería haber automatizado.

**Corrección aplicada** (hook, guardarraíl determinista — corregido
directamente, no queda como propuesta): `run_build()` ahora recibe el entorno
de PlatformIO como parámetro explícito; motherBoard usa `IncuNest_V18`
(revisión de hardware vigente), Display_HMI mantiene `main`. Verificado con
`bash -n` y releyendo el fichero completo tras el cambio.

## Qué NO se sistematiza

- **`thingsboard/TBPubSubClient@2.9.4` ya no existe en el registro de
  PlatformIO** — impide una compilación limpia de motherBoard/Display_HMI
  desde un checkout sin caché. Es real y confirmado (`UnknownPackageError`),
  pero es un problema externo de deriva de versión de un paquete de terceros,
  no una convención de este framework ni algo que un hook pueda prevenir sin
  fijar una versión concreta — que es una decisión de dependencias del
  proyecto, ajena a este cambio. Queda anotado en
  `openspec/changes/hmi-audio-enlace-caido/tasks.md` (tarea 4.2) para que
  quien decida el pin lo tenga documentado; no se toca aquí.

## Cambios aplicados a `Firmware/.claude/`

1. **`Firmware/.claude/hooks/run-affected-tests.sh`** — `run_build()` acepta
   el entorno de PlatformIO como parámetro; motherBoard pasa a
   `IncuNest_V18` en vez del inexistente `main`. *Por qué*: guardarraíl
   determinista roto de forma silenciosa para una de las dos placas desde
   que existe.
2. **`Firmware/.claude/rules/embedded-motherboard.md`** — nota sobre
   verificar `git diff -- platformio.ini` tras cualquier comando de
   PlatformIO antes de diagnosticar nada a partir de su contenido.
3. **`Firmware/.claude/rules/embedded-shared.md`** — nota sobre la
   no-portabilidad de `.pio/libdeps/<env>/shared.pio-link` entre worktrees
   y cómo repararla sin perder la caché.
