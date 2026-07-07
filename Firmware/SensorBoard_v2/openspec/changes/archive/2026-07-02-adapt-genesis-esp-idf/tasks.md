## 1. OpenSpec foundation (already done, tracked for completeness)

- [x] 1.1 Install `@fission-ai/openspec` CLI globally, verify with `openspec doctor`
- [x] 1.2 Purge the leftover `cart`/`validate-cart-quantity` e-commerce example from `openspec/`
- [x] 1.3 Run `openspec init .`, confirm the 5 `openspec-*` skills refresh to the installed CLI version
- [x] 1.4 Create this change (`adapt-genesis-esp-idf`) with proposal/design/specs/tasks

## 2. openspec/config.yaml

- [x] 2.1 Fill in `context:` with the real project stack (ESP32-S3-WROOM-1-N16R8, ESP-IDF v6, C, FreeRTOS, Unity/idf.py, USB-CDC framed JSON+CRC16 protocol) so future OpenSpec-generated artifacts are grounded without re-explaining the stack each time

## 3. Rules (.claude/rules/)

- [x] 3.1 Delete `astro.md`, `nextjs.md`, `typescript.md`
- [x] 3.2 Rewrite `testing.md` for Unity + `idf.py build/flash/monitor`, TDD red-before-green, tests live in `test_apps/<comp>_test/main/test_main.c`
- [x] 3.3 Rename `domain.md` → `embedded.md`: ESP-IDF component boundaries, `usb_comm` transport-agnostic principle (no phase after Phase 1 reopens it), ISR does hand-off only (queue/semaphore) never logic/logging, static allocation preferred over heap
- [x] 3.4 Rewrite `security.md` for firmware/medical-device threats (frame CRC/length validation, integer overflow in the length field, buffer overflow, malformed payloads, sensor-fault handling) — keep the existing generic rule about hooks emitting control JSON via `jq -nc`, never string interpolation
- [x] 3.5 Rewrite `commits.md`: authorship = Pablo Sánchez Bergasa (pablo18393), remove all "Rodrigo Esgueva" references, drop the per-loop-stage-commit line, keep Conventional Commits + the feat/meta/dev/release/main branch model, note there is no commitlint (convention + code-reviewer enforced instead)

## 4. Agents (.claude/agents/)

- [x] 4.1 Rewrite `architect.md`: ESP-IDF components, FreeRTOS tasks/queues/semaphores, the `usb_comm`-stays-agnostic boundary, ADRs still delegated to `scribe`
- [x] 4.2 Rewrite `senior-developer.md`: TDD with Unity/`idf.py build`, C conventions, no domain/application package references
- [x] 4.3 Rewrite `code-reviewer.md`: static allocation vs heap, FreeRTOS task priority/stack-size correctness, ISR safety, replace `any`/`@ts-ignore`/`noUncheckedIndexedAccess` checks with C equivalents (no implicit int/pointer casts, no undefined behavior)
- [x] 4.4 Rewrite `security-reviewer.md`: replace the OWASP web checklist (SQLi/XSS/CSRF/Server Actions) with firmware threats (framing/CRC bypass, integer overflow in length field, buffer overflows, malformed USB payloads, sensor-fault/fail-safe handling)
- [x] 4.5 Rewrite `product-manager.md`: OpenSpec proposal/design/tasks/spec-delta authoring using the real `openspec` CLI (`openspec new change`, `openspec instructions`), Given/When/Then examples swapped to firmware scenarios
- [x] 4.6 Rewrite `qa-engineer.md`: test matrix = Unity unit tests (host or on-target) + on-device integration tests via `test_apps/`, no E2E/Playwright; verification uses `idf.py build` plus manual flash/monitor steps
- [x] 4.7 Rewrite `test-writer.md`: Unity tests in `test_apps/<comp>_test/main/test_main.c`, red before green, no `.test.ts`/`@repo/test-utils` references
- [x] 4.8 Rewrite `researcher.md`: swap the domain allowlist to espressif.com/docs.espressif.com/freertos.org (keep the anti-prompt-injection block as-is, it's stack-agnostic)
- [x] 4.9 Rewrite `scribe.md`: ADRs written to `docs/adr/` using the template created in task 6.3
- [x] 4.10 Rewrite `doc-keeper.md`: keeps `README.md`/`CHANGELOG.md`/`docs/` in sync (both created in section 6)
- [x] 4.11 Rewrite `retro-improver.md`: same analyze-iteration→`docs/retro/`→apply-improvement mechanism, pointed at the `ESTADO.md`/`docs/epics/` created in section 6 (reviewed, already stack-agnostic — no edit needed)

## 5. Hooks (.claude/hooks/)

- [x] 5.1 Rewrite `protect-files.sh`: swap `pnpm-lock.yaml` protection for `dependencies.lock`/`managed_components/**`, keep `.env*`, `.git/`, and `openspec/specs/**` protection (allow edits under `openspec/changes/*/specs/**`)
- [x] 5.2 Rewrite `format-file.sh`: run `clang-format` on `.c`/`.h` files (best-effort, never blocks, matching current philosophy); leave non-C files alone (no Node/prettier toolchain in this repo)
- [x] 5.3 Add `.clang-format` at the project root (no config exists anywhere in the repo today) — sensible embedded C style, ~100 column limit
- [x] 5.4 Rewrite `run-affected-tests.sh`: Stop hook runs `idf.py build` (compile-only) when `.c`/`.h` files are uncommitted, blocks close on build failure, never invokes `flash`/`monitor`
- [x] 5.5 Rewrite `guard-merge.sh`: remove the `.loop-mode == "human"` early-exit so the `.claude/.merge-approved` gate applies to `git merge`/`git tag v*` unconditionally
- [x] 5.6 Rewrite `session-context.sh`: report branch/dirty-count, active OpenSpec changes via `openspec list --json` (instead of raw `ls openspec/changes`), keep loop-mode reporting and the `ESTADO.md` momentum line (both still apply)
- [x] 5.7 Rewrite `tdd-reminder.sh`: match `.c`/`.h` files instead of `.ts`/`.tsx`; heuristic checks whether any `test_apps/**/test_main.c` references the new file's module/function names, since Unity tests are grouped rather than colocated 1:1
- [x] 5.8 Verify `require-retro.sh`, `guard-push.sh`, `subagent-log.sh` need no content changes — confirmed stack-agnostic, no edits needed

## 6. Missing documentation

- [x] 6.1 Create `ESTADO.md` (momentum witness: active epic/task, next step)
- [x] 6.2 Create `CHANGELOG.md` (Keep a Changelog + SemVer, `[Unreleased]` section)
- [x] 6.3 Create `docs/adr/README.md` + `docs/adr/0000-template.md`
- [x] 6.4 Create `docs/retro/README.md`
- [x] 6.5 Create `docs/epics/README.md`
- [x] 6.6 Create `docs/architecture.md` (brief: `usb_comm` transport layer + per-phase sensor components, referencing the roadmap doc)
- [x] 6.7 Create `docs/blueprint/README.md` (short version — process overview, not full SaaS-scale ceremony)

## 7. Skills (.claude/skills/)

- [x] 7.1 Rewrite `genesis-stack-plugins/SKILL.md` anchored on `clangd-lsp`; explicit "no analog" for E2E/deploy/PR
- [x] 7.2 Rename `arch-clean-architecture/` → `arch-embedded-layering/`, ESP-IDF component-boundary model
- [x] 7.3 Rewrite `tdd-cycle/SKILL.md`: Unity/`idf.py build/flash/monitor -p COMx` table
- [x] 7.4 Update `spec-driven-development/SKILL.md`: real CLI commands, removed `scripts/openspec-sync.mjs`/`pnpm openspec:sync` references
- [x] 7.5 Reviewed `git-flow`, `git-feature-start` (no changes needed), `git-feature-finish`, `git-release` (critical fix: version source is `SB_PROTO_FW_VERSION`, not `package.json`) — authorship fixed, `idf.py build` swapped in
- [x] 7.6 Reviewed `loop-engineering`, `loop-mode` (no changes needed), `loop-modes`, `loop-run` — stack-specific mentions swapped, dead doc link removed; `loop-modes` additionally updated post-hoc to resolve the always-on-merge-gate vs. per-mode-gates tension (see note below)
- [x] 7.7 Reviewed `meta-retro` (no changes needed), `meta-self-improvement` — fixed the corrupted ADR-0001 markdown link
- [x] 7.8 Verified `openspec-apply-change`, `openspec-archive-change`, `openspec-explore`, `openspec-propose`, `openspec-sync-specs`, `web-research-safety` — no changes needed (CLI-managed / already stack-agnostic)

## 8. CLAUDE.md

- [x] 8.1 Rewrite project identity section: SensorBoard firmware, ESP32-S3-WROOM-1-N16R8, ESP-IDF v6, USB-CDC framed JSON+CRC16 protocol
- [x] 8.2 Rewrite "Stack" and "Comandos" sections: `idf.py build/flash/monitor`, CMake, no pnpm/Turborepo
- [x] 8.3 Rewrite the agent roster (all 11, one-line purpose each) and rule-zone list (`embedded` instead of `domain`, no astro/nextjs/typescript)
- [x] 8.4 Update gitflow section: branch model text now states the merge/tag gate applies in all three loop modes, including `auto`
- [x] 8.5 Fix "No hacer" section authorship line to Pablo Sánchez Bergasa / pablo18393
- [x] 8.6 Remove the entire "Distribución como plugin" section
- [x] 8.7 Update "Mapa de conocimiento" table to point at the real `docs/` tree, plus the historical Phase 1/roadmap docs in `Firmware/docs/superpowers/`

## 9. Final verification

- [x] 9.1 Repo-wide grep across `.claude/` and `CLAUDE.md` for stale references — all remaining hits are intentional ("no Playwright equivalent here", "no Vitest/Node test env", historical mentions of the original template). Additionally found and fixed two cross-cutting issues the grep didn't target directly: `settings.json` still denied `pnpm-lock.yaml` (fixed → `dependencies.lock`), `unattended-loop.sh` still said `pnpm verify` and referenced a nonexistent `ADR-0001` (fixed → `idf.py build`, pointed at `docs/adr/0000-template.md` convention instead)
- [x] 9.2 Confirmed every referenced directory/file exists on disk (docs tree, all rules/agents/hooks/skills, `.clang-format`)
- [x] 9.3 `openspec validate --all` → change `adapt-genesis-esp-idf` valid, 0 issues
- [x] 9.4 Re-read `CLAUDE.md` end-to-end — internally consistent

### Additional fixes found during verification (not in original task list)

- Repo-root `.gitignore` had a bare `.claude` pattern excluding every `.claude/` folder in the monorepo from git — meaning this entire adaptation could never be committed. Added a scoped exception (`!Firmware/SensorBoard_v2/.claude/` + `/**`) so this board's agentic config is versioned while `motherBoard/`/`Display_HMI/` keep their current (unversioned) behavior — confirmed via `git check-ignore` on both.
- `.claude/settings.local.json` (personal permissions file) had stale content from the original author's machine: `Bash(pnpm/npm/npx/node *)` allow-rules, a hardcoded absolute path (`/Users/esgueva/Dev/sanpedros/...`), and macOS-only tmp-dir grants. Cleaned up to `idf.py`/`openspec`/`clang-format`/`git`/`jq`. Also added `settings.local.json` to `.claude/.gitignore` so it stays personal/untracked even though the rest of `.claude/` is now versioned — consistent with how the framework's own docs describe it ("personal, gitignored").
- `loop-modes/SKILL.md` said `auto` mode has "sin gates humanos" (no human gates) for the merge step, which became false once `guard-merge.sh` (task 5.5) made the merge/tag gate unconditional across all three modes. Rewrote the `auto` and `human` sections to state the merge/release gate is shared and always-on, while the plan/épica gate remains `human`-only.
