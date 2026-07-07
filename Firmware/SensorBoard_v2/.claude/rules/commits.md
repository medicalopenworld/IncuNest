---
paths:
  - "**/*"
---

# Reglas de commits y ramas

- **Atribución**: el único autor de TODO commit/merge/release es **Pablo Sánchez Bergasa** (GitHub: `pablo18393`). Nunca incluyas `Co-Authored-By: Claude/Anthropic` ni `Claude-Session` en el mensaje.
- **Conventional Commits**: `tipo(scope): descripción`. No hay `commitlint` ni ninguna herramienta de Node instalada (no es un proyecto npm, no hay `package.json`) — el formato se exige por convención y lo verifica el agente `code-reviewer` en el stage de review, no un hook `commit-msg` automatizado.
- Tipos permitidos: `feat`, `fix`, `refactor`, `test`, `docs`, `chore`, `perf`, `build`, `ci`, `style`, `revert`.
- **Un commit por stage del loop**, atómico (una sola cosa coherente). Ver el mapeo stage→tipo en el skill `git-flow`.
- El mensaje explica el porqué cuando no es obvio; la descripción en imperativo.
- Ramas: `feat/<slug>` (producto) o `meta/<slug>` (capa agéntica) desde `dev`; integración a `dev` con `merge --no-ff`; releases vía `release/<ver>` a `main` con tag semver.
- Nunca `git push` directo a `main`/`dev` (lo bloquea un hook): usa `/git-feature-finish` y `/git-release`.
- No incluyas en un commit cambios no relacionados con su stage.
- En un stage con reintentos/fricción, añade al cuerpo del commit una línea **"Señales de reintento: …"** (input barato para el paso 7).
- **Merge commits** (`merge: <rama> -> dev`, `Merge branch ...`) están exentos de Conventional Commits. El resto de commits sí deben cumplirlo.
