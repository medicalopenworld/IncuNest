## Why

The `.claude/` agentic framework and `CLAUDE.md` in this project were copied verbatim from a different project (a pnpm/Astro/Next.js/TypeScript SaaS monorepo). Every agent prompt, rule, skill, and hook currently assumes that stack (Vitest, Playwright, hexagonal domain/application/infrastructure packages, `pnpm verify`, commitlint) and none of it applies to what this repository actually is: ESP32-S3-WROOM-1-N16R8 firmware built with ESP-IDF v6 (CMake, C, FreeRTOS), tested with Unity via `idf.py build/flash/monitor`. Left as-is, every agent invocation and hook would reference tools, paths, and conventions that don't exist here. This must be fixed before the framework can be used for real firmware work (starting with Phase 2 of the SensorBoard roadmap).

## What Changes

- Rewrite all 11 agent prompts (`architect`, `senior-developer`, `code-reviewer`, `security-reviewer`, `product-manager`, `qa-engineer`, `test-writer`, `researcher`, `scribe`, `doc-keeper`, `retro-improver`) for C/ESP-IDF/FreeRTOS instead of TypeScript/Astro/Next.
- Rewrite `.claude/rules/testing.md`, `domain.md` (renamed `embedded.md`), `security.md`, `commits.md` for the firmware stack and correct authorship (Pablo Sánchez Bergasa / pablo18393, not Rodrigo Esgueva). **Delete** `astro.md`, `nextjs.md`, `typescript.md` (zero transferable content).
- Rewrite `.claude/hooks/protect-files.sh`, `format-file.sh`, `run-affected-tests.sh`, `session-context.sh`, `tdd-reminder.sh`, `guard-merge.sh` for ESP-IDF tooling and file conventions. `require-retro.sh`, `guard-push.sh`, `subagent-log.sh` stay mechanically as-is (already stack-agnostic).
- Rewrite `.claude/skills/genesis-stack-plugins` (anchored on the already-installed `clangd-lsp` plugin instead of TS/Astro/Vercel plugins), `arch-clean-architecture` (renamed `arch-embedded-layering`), `tdd-cycle`, `spec-driven-development` (drop the now-false claim that no `openspec` CLI exists — it is installed and in use). `git-flow`, `git-feature-start`, `git-feature-finish`, `git-release`, `loop-engineering`, `loop-mode`, `loop-modes`, `loop-run`, `meta-retro`, `meta-self-improvement`, `web-research-safety`, and the 5 CLI-managed `openspec-*` skills need only small wording/example adjustments, not structural rewrites.
- Create missing documentation the framework already references but that doesn't exist: `ESTADO.md`, `CHANGELOG.md`, `docs/adr/README.md` + `0000-template.md`, `docs/retro/README.md`, `docs/epics/README.md`, `docs/architecture.md`, `docs/blueprint/README.md` — all kept short (roughly one page each), scaled to a solo embedded project rather than the original multi-team SaaS scope.
- Fully rewrite root `CLAUDE.md`: project identity (SensorBoard firmware, ESP-IDF v6, USB-CDC framed protocol), commands (`idf.py build/flash/monitor`, not `pnpm`), the 11-agent roster, gitflow, loop-engineering, corrected authorship. **Remove** the "Distribución como plugin" section entirely (`.claude-plugin/marketplace.json`, `plugins/genesis/`, `scripts/sync-genesis-plugin.sh`) — this project is not published as an installable plugin for other repos.
- Populate `openspec/config.yaml` with real project context (tech stack, conventions) so OpenSpec-generated artifacts are grounded correctly.
- Add a `.clang-format` at the project root (none exists anywhere in the repo today) so `format-file.sh` has something to run against `.c`/`.h` files.

## Capabilities

### New Capabilities
- `git-workflow-guard`: enforced gitflow behavior — direct `git push` to `main`/`dev` is denied; `git merge`/`git tag v*` require a one-time human-approval marker regardless of loop-mode (medical-device firmware always gates irreversible integration, unlike the original template where this only applied in "human" loop-mode).
- `build-verification-gate`: the Stop hook runs `idf.py build` (compile-only) when `.c`/`.h` files are uncommitted, and blocks closing the turn if the build fails. It does not flash or run on-device Unity tests — those remain manual since they require physical hardware.
- `protected-paths`: PreToolUse denial of edits to `.env*` (except `.env.example`), `dependencies.lock`/`managed_components` lockfiles, `.git/` internals, and `openspec/specs/**` (source of truth, changed only via `openspec archive`).

### Modified Capabilities
(none — `openspec/specs/` is currently empty; nothing pre-existing changes requirements)

## Impact

- **Affected paths**: `CLAUDE.md`, `.claude/agents/**`, `.claude/rules/**`, `.claude/hooks/**`, `.claude/skills/**`, `openspec/config.yaml`, new `docs/**` tree, new `ESTADO.md`/`CHANGELOG.md`/`.clang-format` at repo root.
- **Not affected**: no firmware source code changes (no `main/`, `components/` yet — those start with the actual Phase 2 work, after this meta-change).
- **Dependencies**: `@fission-ai/openspec` CLI (installed globally, v1.5.0), `clangd-lsp` plugin (already installed), `idf.py`/ESP-IDF v6 toolchain (already installed, confirmed via `idf.py`-adjacent tools on `PATH`).
- **Risk**: low — this is tooling/documentation only, no runtime firmware behavior changes. Main risk is inconsistency if a reference to a since-renamed/deleted file (e.g. `domain.md` → `embedded.md`) is missed; task list includes a final grep-for-stale-references pass.
- **Branch**: work happens on `claude_agents_tests` (already created off `dev`), classified as a `meta/*`-style change per the gitflow rules being introduced by this very change.
