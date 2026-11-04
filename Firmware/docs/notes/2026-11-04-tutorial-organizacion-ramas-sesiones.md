# Tutorial: cómo organizar ramas, worktrees y sesiones de Claude Code en IncuNest / PulseNest / incunest_afe4490

Fecha: 2026-11-04 · Para: Alex · Origen: sesión fork "Bug HR 152 vs 135"

Guía de trabajo, no norma del repo. Describe la organización que mejor encaja con la estructura actual de los tres repositorios y con las herramientas de la app de escritorio de Claude Code (sesiones, forks, sesiones enlazadas, worktrees).

---

## 1. El mapa: tres repos y una dependencia en cadena

```
incunest_afe4490  (librería, tuya; etiquetas v0.NNN)
      │  pin en platformio.ini: ...incunest_afe4490.git#v0.113
      ├──────────────► IncuNest   (repo padre; motherBoard + HMI; Pablo revisa)
      └──────────────► PulseNest  (hereda las políticas de IncuNest)
```

| Hecho | Consecuencia |
|---|---|
| La librería se consume por **etiqueta fija** (`#v0.113`), nunca por `#master` | Cambiar la librería nunca rompe IncuNest por sí solo. Rompe cuando **subes el pin**. |
| IncuNest es el repo padre | Las políticas (nombres de rama, formato de commit, PR contra `dev`) se definen aquí y se copian a los otros dos. |
| Cada repo tiene su historial, su `origin` y sus PR | **Un cambio = un repo = una rama = un PR.** No existe "un commit que toca dos repos". |
| Los tres están en `C:\PRJ\MOW\` uno al lado del otro | Una sesión de Claude abierta en una carpeta solo debe editar esa carpeta. |

---

## 2. Ramas: cuándo, cómo y dónde

### 2.1 Las ramas fijas

- `master`: lo que está en las placas de campo. Solo se toca fusionando `dev`.
- `dev`: integración. Todo PR va contra `dev`.
- Ramas de trabajo: `feat/...`, `fix/...`, `refactor/...`, `docs/...`. Nacen de `dev` y mueren al fusionarse.

### 2.2 Cuándo crear una rama nueva

Cuando empiezas **una unidad de trabajo que acabará en un PR**. Señales de que necesitas una rama nueva en vez de seguir en la actual:

- El cambio podría fusionarse por separado y tendría sentido solo. Ejemplo real: "SpO2 en la pantalla de bloqueo" y "eliminar el escritor CSV" son dos cosas; el bug de HR habría ido mejor en `fix/afe-samples-lost` que en `feat/hmi-pulse-oximetry`. Se dejó ahí porque el fork ya estaba sobre esa rama. Lección: **antes de empezar un fork que va a tocar código, decide la rama**.
- Es para otro revisor o tiene otra urgencia.
- Es de otro repo (la trampa más habitual: "ya que estoy, toco la librería").

### 2.3 Cómo (siempre desde `dev` actualizado)

```bash
git checkout dev
```
```bash
git pull
```
```bash
git checkout -b fix/afe-samples-lost
```

Al terminar: push, PR contra `dev`, y cuando se fusione, borrar la rama local:

```bash
git branch -d fix/afe-samples-lost
```

### 2.4 Ramas de la librería y el pin

Flujo completo de un cambio de contrato (como el paso A, `samples_missed`):

1. En `incunest_afe4490`: rama `feat/missed-samples` → PR → fusionar → **etiqueta** `v0.116` → push de la etiqueta.
2. En IncuNest: rama `feat/afe-v0.116` que solo sube el pin y usa los campos nuevos → PR.
3. En PulseNest: igual, su propia rama y PR.

Nunca al revés (no subas el pin a una etiqueta que no existe) y nunca "de paso" en una rama que hace otra cosa.

---

## 3. Worktrees: cuándo sí y cuándo no

Un worktree es una **segunda carpeta** del mismo repo con otra rama activa (`git worktree list` para verlos). Es una función de Git; Claude Code solo la usa.

### Cuándo SÍ

- Dos sesiones de Claude van a **editar código del mismo repo a la vez** (una hace la pantalla de alarmas en el HMI mientras otra arregla algo en motherBoard). Sin worktree se pisan los ficheros; con worktree cada una tiene su carpeta y su rama.
- Quieres mantener compilable y flasheable la rama que tienes en el banco mientras editas otra cosa en otra rama, sin hacer `checkout`.

### Cuándo NO

- Trabajo secuencial de una sola sesión: basta con `checkout`.
- Sesiones de **solo lectura** (preguntas, revisiones, investigación): comparten carpeta sin problema.
- Cuando el coste importa: cada worktree de IncuNest obliga a PlatformIO a compilar de cero (~10 min la primera vez) y a descargar la librería otra vez.

### Dónde

La app los crea en `C:\PRJ\MOW\IncuNest\.claude\worktrees\<nombre>`; no hace falta crearlos a mano. Si creas uno manualmente para el banco:

```bash
git worktree add ../IncuNest-banco feat/hmi-pulse-oximetry
```

y lo retiras con `git worktree remove ../IncuNest-banco` cuando acabes.

---

## 4. Sesiones de Claude Code: una por repo, una por tema

### 4.1 Tipos que existen en la app

| Tipo | Qué es | Cuándo |
|---|---|---|
| **Sesión principal** | Conversación larga sobre un tema en una carpeta | Una por tema: "Pulsioximetría en HMI", "Alarmas", "Porte a ESP-IDF". |
| **Fork** (= side chat) | Copia de la sesión en el punto actual, con **todo su contexto**, que sigue por su cuenta. La original no se entera de lo que pasa en el fork. | Una pregunta o desvío que necesita el contexto actual pero no debe ensuciar la sesión principal. Este documento nació en un fork. |
| **Sesión enlazada** (tarjeta "New task", o Claude la lanza) | Sesión nueva con un brief, cuyo resultado vuelve a la sesión madre como una nota | Trabajo paralelo con entregable propio (rama/PR). |
| **Subagente** | Proceso interno de una sesión; no lo ves como sesión | Búsquedas amplias o revisiones dentro de un turno. Lo decide Claude. |

### 4.2 Reglas prácticas

1. **Una sesión = una carpeta (un repo).** Si en medio de una sesión de IncuNest necesitas tocar la librería, no lo hagas ahí: pide un brief (fichero en `Firmware/docs/`), ábrelo en una sesión de `incunest_afe4490` y vuelve.
2. **Una sesión = un tema = (normalmente) una rama.** Si el tema cambia de rama, díselo a la sesión al principio del turno.
3. **Agrupa en la barra lateral** por repo o por tema transversal. Para "muestras perdidas del AFE" tiene sentido un grupo con las tres sesiones (librería, IncuNest, PulseNest), aunque cada una trabaje en su repo.
4. **Guarda el estado fuera de la sesión.** Las sesiones largas se resumen automáticamente y pierden detalle. Lo que importa va a `conversation_log.md` (incremental), a `Firmware/docs/` (notas, briefs) y a la memoria de Claude. Al retomar tras días: "léete el conversation_log y la memoria".
5. **Commit, push y PR siempre a petición tuya**, viendo antes qué se sube. Es la red de seguridad mientras aprendes Git.
6. **Secretos no cruzan sesiones ni repos.** `Credentials.h`/`secrets.h` no se leen, no se pegan y no se copian.

### 4.3 Cuándo hacer un fork (side chat)

Cuando quieras **preguntar o explorar sin comprometer la sesión principal**:

- "Explícame qué es IRAM" en medio de un arreglo.
- "¿Y si en vez de esto hiciéramos aquello?" para comparar sin tocar código.
- Diagnosticar un bug que aparece mientras haces otra cosa (como el HR 152).

Lo que **no** conviene hacer en un fork sin pensarlo: **cambios de código que acaben en commit**, porque el fork hereda la rama de la sesión madre. Si el desvío va a producir commits, antes del primer cambio decide: ¿rama nueva? ¿sesión nueva mejor que fork? En este fork se commitearon B y E sobre `feat/hmi-pulse-oximetry`; funciona, pero el PR de la pantalla llevará dentro un fix de muestreo.

Ventaja del fork sobre la sesión nueva: tiene todo el contexto (medidas, decisiones, ficheros leídos). Inconveniente: hereda también el tamaño del contexto y la rama.

### 4.4 Cuándo lanzar una sesión enlazada (New task)

- El trabajo tiene entregable propio (rama + PR) y puede correr **en paralelo** sin estorbar.
- Es **en otro repo** (la sesión de la librería para el paso A).
- Es algo aparcado que quieres retomar aparte (PANIC a 1,4 s del arranque; `WIFITBProvision` reintentando).

Si Claude te lo propone como tarjeta "New task", al pulsarla se crea con el brief ya escrito; cuando termine, la sesión madre recibe una nota con el resumen.

---

## 5. Recetas para los casos que tienes ahora

### Receta A: implementar el paso A en la librería y adoptarlo

1. Sesión nueva en `C:\PRJ\MOW\incunest_afe4490`, grupo "AFE4490 – muestras perdidas". Primer mensaje: "Lee `C:\PRJ\MOW\IncuNest\Firmware\docs\notes/2026-11-04-brief-libreria-muestras-perdidas.md` e implementa A en una rama `feat/missed-samples` desde `dev`".
2. Esa sesión: tests de host, PR, fusión, etiqueta `v0.116`.
3. Sesión de IncuNest: rama `feat/afe-v0.116` desde `dev`, subir el pin, publicar `samples_missed` en `/debug/state` y telemetría, PR.
4. Sesión de PulseNest: lo mismo en su repo.

### Receta B: dos cosas a la vez en IncuNest (p. ej. alarmas en HMI y C' en motherBoard)

- Sesión 1 en la carpeta normal, rama `feat/ppg-alarms`.
- Sesión 2 **en worktree** (marcar la opción al crearla), rama `feat/ppg-burst-capture`.
- Cada una su PR. Si una necesita la otra, se fusiona primero la que va antes.

### Receta C: retomar tras días sin tocar el proyecto

1. Abrir la sesión principal del tema (no un fork).
2. Primer mensaje: "Lee `conversation_log.md` y la memoria `estado_investigacion.md` y dime en qué estado están ramas, push y PR".
3. `git status` y `git worktree list` para confirmar que el árbol está como lo dejaste.

### Receta D: un bug aparece en el banco mientras haces otra cosa

1. Fork de la sesión actual (tiene el contexto del banco: IP, versión flasheada, simulador).
2. Antes del primer cambio de código: `git checkout -b fix/<nombre>` **si el fix es independiente**; si depende de lo que hay en la rama de trabajo, seguir en ella y decirlo en el commit (como con B y E).
3. Cerrar el fork con: commit(s), push, nota en `conversation_log.md`, y mensaje a la sesión madre si tiene que saberlo.

---

## 6. Chuleta de comandos (Git, local)

| Quiero | Comando |
|---|---|
| Ver dónde estoy | `git status` / `git branch` |
| Ver qué ramas hay en GitHub | `git branch -r` |
| Rama nueva desde dev | `git checkout dev && git pull && git checkout -b feat/x` |
| Ver qué subiría | `git log origin/feat/x..feat/x --oneline` |
| Subir por primera vez | `git push -u origin feat/x` |
| Ver worktrees | `git worktree list` |
| Ver etiquetas de la librería | `git -C ../incunest_afe4490 tag --sort=-v:refname` |

---

## 7. Resumen en seis líneas

1. Un cambio → un repo → una rama → un PR. Siempre.
2. La librería se cambia en su sesión, se etiqueta, y luego los consumidores suben el pin en sus propias ramas.
3. Sesión por repo y por tema; el estado importante va a ficheros, no solo a la conversación.
4. Fork para preguntar y explorar con contexto; si va a producir commits, decide la rama antes del primer cambio.
5. Sesión enlazada (New task) para trabajo paralelo con entregable propio, sobre todo en otro repo.
6. Worktree solo cuando dos sesiones van a editar el mismo repo a la vez.
