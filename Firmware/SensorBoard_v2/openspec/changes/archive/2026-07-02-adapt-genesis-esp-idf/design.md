## Context

`SensorBoard_v2/.claude/` and `CLAUDE.md` were copied from "Genesis", a framework built for a pnpm/Turborepo monorepo (Astro marketing site + Next.js app, `packages/domain|application|infrastructure`, Vitest/Playwright, OpenSpec via a custom script, commitlint-enforced Conventional Commits, and a distribution mechanism to publish Genesis itself as an installable Claude Code plugin for other repos).

This project is a single ESP-IDF v6 firmware target (`SensorBoard`, ESP32-S3-WROOM-1-N16R8) for the IncuNest incubator's sensor peripheral board. It communicates with a separate "motherboard" firmware over a custom USB-CDC binary-framed protocol (`Magic(2B)+Type(1B)+Length(4B LE)+Payload+CRC16-CCITT`) carrying JSON commands/events. Phase 1 (USB CDC transport + `status` command) is already planned in `Firmware/docs/superpowers/plans/2026-06-08-sensorboard-phase1.md`; Phases 2-5 (sensors, microphone, door sensor, camera) are roadmapped in `Firmware/docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md`. There is no `package.json`, no Node build, no commitlint, no PlatformIO — pure CMake/`idf.py`.

The rest of the `Firmware/` monorepo (`motherBoard/`, `Display_HMI/`) uses a lighter workflow (plain `superpowers` skills, specs under `docs/superpowers/`) — deliberately NOT mirrored here. This adaptation is scoped to `SensorBoard_v2/` only; Claude Code resolves `.claude/`/`CLAUDE.md` hierarchically from the working directory, so nothing here affects sibling board folders.

The user (Pablo Sánchez Bergasa, github `pablo18393`) explicitly chose, across a multi-turn brainstorming session, to port the **full** Genesis framework (all 11 agents, OpenSpec, the 12-stage loop, full gitflow) rather than a lighter subset — this is a deliberate experiment on this one board's firmware, done on a dedicated branch (`claude_agents_tests`, created off `dev`).

## Goals / Non-Goals

**Goals:**
- Every agent prompt, rule, hook, and skill references tools/paths/conventions that actually exist in this repo (ESP-IDF, Unity, `idf.py`, CMake — not Vitest/pnpm/Next).
- The 12-stage loop and gitflow mechanics are preserved (the user wants to trial the full process), only their stack-specific tool calls change.
- OpenSpec works end-to-end using the real `@fission-ai/openspec` CLI (confirmed installed, v1.5.0) — no invented CLI-less fallback, no reference to the nonexistent `scripts/openspec-sync.mjs`.
- Every file the framework references (`ESTADO.md`, `docs/adr/`, etc.) exists, even if minimal.

**Non-Goals:**
- Not changing the lighter workflow used by `motherBoard/`, `Display_HMI/`, or `Firmware/docs/superpowers/` — those are out of scope and untouched.
- Not implementing any actual firmware (Phase 1/2 source code) as part of this change — that follows afterward, using the now-adapted framework.
- Not building/publishing Genesis as an installable plugin for other repos — the "Distribución como plugin" section is removed, not adapted.
- Not adding E2E-equivalent or deploy/PR-automation plugin integrations for embedded — no direct analog exists; `genesis-stack-plugins` will say so explicitly rather than inventing one.

## Decisions

**1. OpenSpec: real CLI, not a hand-written-file fallback.**
Initially planned to have `product-manager`/`architect` write `proposal.md`/`design.md`/`tasks.md`/spec-deltas by hand (no CLI), because the `openspec` package squatted on the public npm registry is unrelated (a 2019 placeholder, `openspec@0.0.0`). Research found the real project is **Fission-AI/OpenSpec**, installed via `npm install -g @fission-ai/openspec@latest`. Installed and verified (`openspec doctor` → `ok`, v1.5.0). `openspec init` was run against the existing `.claude/skills/openspec-*` (which turned out to already be CLI-generated artifacts — `generatedBy` metadata bumped from an older version to `1.5.0`) and against `openspec/` (after purging the leftover e-commerce `cart` example). This change (`adapt-genesis-esp-idf` itself) is the first real exercise of that CLI. Alternative considered: skip the CLI and use manual file convention — rejected once the real CLI was confirmed installable, since it adds validation (`openspec validate`) and archiving automation (`openspec archive`) for free.

**2. Merge/tag gate is always-on, decoupled from `.loop-mode`.**
The original `guard-merge.sh` only blocks `git merge`/`git tag v*` when `.claude/.loop-mode` contains `"human"`; in `auto`/`oneshot` it lets merges through unchecked. For this medical-device firmware, the user wants irreversible integration (merge to `dev`/`main`, release tags) to **always** require the one-time `.claude/.merge-approved` marker, regardless of loop-mode. Loop-mode still governs other stage-gating behavior (e.g. whether `loop-engineering` pauses for plan approval) — only the merge/tag gate is unconditional now. `.claude/.loop-mode` already exists in the copied files, preset to `auto`.

**3. Versioning source: `SB_PROTO_FW_VERSION`, not `package.json`.**
`git-release` originally reads the version via `node -p "require('./package.json').version"`. No `package.json` exists or should exist in a CMake project. The firmware's own version constant, `SB_PROTO_FW_VERSION` (defined in `sensorBoard_comm_protocol.h` per the Phase 1 plan, not yet created), becomes the single source of truth — `git-release` will `grep` it out of that header once it exists. Until Phase 1 lands, `git-release` documents this dependency explicitly rather than silently failing.

**4. Verification gate is compile-only, never auto-flash.**
Unity tests require a physically connected ESP32-S3 on a known COM port (`idf.py -p COMx flash monitor`) — unsafe/impossible to run unattended in a Stop hook (no guarantee a device is attached, risk of flashing an unintended board). `run-affected-tests.sh` is rewritten to run `idf.py build` (and the `test_apps/*/`'s own build, when present) as a compile-only gate, blocking the turn close only on build failure. On-device Unity execution stays a manual step the developer runs themselves, exactly as documented in the Phase 1 plan's own verification steps.

**5. Docs scaffolding kept minimal.**
Genesis's original `docs/blueprint/`, `docs/adr/`, `docs/retro/`, `docs/epics/`, `docs/architecture.md` were sized for a multi-team SaaS product. This is a solo embedded project. Each created doc is capped at roughly one page: enough for `scribe`/`doc-keeper`/`retro-improver` to have somewhere real to write, without importing SaaS-scale ceremony that doesn't fit a single developer iterating on firmware.

**6. `arch-clean-architecture` → `arch-embedded-layering`.**
Hexagonal domain/application/infrastructure packages don't exist here. The roadmap doc already states the load-bearing architectural principle for this firmware: `usb_comm` is a transport-agnostic layer that no phase after Phase 1 should need to reopen; each phase adds a new ESP-IDF *component* with its own FreeRTOS task(s) that only ever call `sensorBoard_comm_send_json()`/`send_binary()`. The renamed skill codifies exactly that boundary (component public headers = the "port", `usb_comm` internals never leak upward) instead of TypeScript layer diagrams.

**7. `genesis-stack-plugins` anchors on `clangd-lsp`, admits the gap elsewhere.**
Confirmed `clangd-lsp` is already installed (`~/.claude/plugins/cache/claude-plugins-official/clangd-lsp/1.0.0`) — a direct analog to the original `typescript-lsp` mapping, usable in explore/design/green stages for cross-references and diagnostics. No embedded analog exists for the original `playwright` (E2E) or `vercel`/`github` (deploy/PR) plugin mappings; the rewritten skill states this rather than fabricating a substitute.

## Risks / Trade-offs

- **[Risk]** Missed stale reference to a renamed/deleted file (e.g. `domain.md`, `.env` pattern, `pnpm-lock.yaml`) after the bulk rewrite → **Mitigation**: final task is a repo-wide grep for `pnpm|Vitest|Playwright|Astro|Next\.js|domain.md|Rodrigo Esgueva|packages/(domain|application|infrastructure)` across `.claude/` and `CLAUDE.md`, fixing any hit.
- **[Risk]** `openspec` CLI is a fast-moving external dependency (v1.5.0 today); a future CLI upgrade could change flags/output shape the skills rely on → **Mitigation**: the 5 `openspec-*` skills are CLI-managed (`openspec update` regenerates them) — not hand-maintained, so drift is the CLI author's problem to keep in sync, not ours.
- **[Risk]** Always-on merge/tag gate (decision 2) could feel heavy for a solo developer moving fast → **Mitigation**: it's a single `printf ok > .claude/.merge-approved` before the blocked command, consistent with the user's explicit choice given the medical-device context; can be revisited later if it proves too much friction (would be its own future change, logged via `retro-improver` if so).
- **[Trade-off]** No automated on-device test execution (decision 4) means the Stop-hook safety net is weaker than the original template's (which ran the full test suite). Accepted because the alternative (auto-flashing on every Stop) is actively dangerous with real hardware attached.

## Migration Plan

1. Land all rewritten files on `claude_agents_tests` (current branch).
2. No data/runtime migration — this is tooling/doc content only.
3. Rollback: `claude_agents_tests` is disposable; if the experiment doesn't work out, `dev` was never touched (the only commit on `dev` is the unrelated pre-existing motherboard fix committed earlier in this session) and the branch can simply be abandoned.

## Open Questions

- Whether to eventually add a lightweight git `commit-msg` hook enforcing Conventional Commits (no `commitlint`/Node project here) — deferred; not blocking, current plan relies on `code-reviewer` + convention only.
- Whether the always-on merge/tag gate (decision 2) should also apply to `feat/*` → `dev` merges done via `/git-feature-finish` in `auto` mode, or only to `dev`→`main`/release — resolved as **all** merges/tags per the user's stated reasoning (medical device, always gate), but worth revisiting once real usage friction is observed.
