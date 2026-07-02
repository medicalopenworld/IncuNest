# git-workflow-guard Specification

## Purpose
TBD - created by archiving change adapt-genesis-esp-idf. Update Purpose after archive.
## Requirements
### Requirement: Direct push to protected branches is denied
The system SHALL deny any `git push` targeting `main` or `dev` (locally or via `origin`) issued directly from a Claude Code session, regardless of loop-mode.

#### Scenario: Push directly to dev
- **WHEN** a Bash tool call runs `git push origin dev`
- **THEN** the command is denied with a reason explaining that `dev` is only updated via `/git-feature-finish` (`merge --no-ff`)

#### Scenario: Push directly to main
- **WHEN** a Bash tool call runs `git push main` or `git push origin main`
- **THEN** the command is denied with a reason explaining that `main` is only updated via `/git-release`

#### Scenario: Push to a feature or meta branch is allowed
- **WHEN** a Bash tool call runs `git push origin feat/some-slug` or `git push origin meta/some-slug`
- **THEN** the command proceeds without interference from this guard

### Requirement: Merge and release-tag commands always require human approval
The system SHALL block `git merge` and `git tag v<semver>` commands unless a one-time approval marker file (`.claude/.merge-approved`) exists, **independent of the current `.claude/.loop-mode` value** (this differs from the original template, where the gate only applied in `human` mode).

#### Scenario: Merge attempted without approval marker
- **WHEN** a Bash tool call runs a command containing `git merge` and `.claude/.merge-approved` does not exist
- **THEN** the command is denied with a reason instructing the agent to show the user the current state (tasks done, `idf.py build` green, diff summary) and wait for explicit approval before creating the marker

#### Scenario: Merge attempted with approval marker present
- **WHEN** a Bash tool call runs a command containing `git merge` and `.claude/.merge-approved` exists
- **THEN** the command proceeds and the marker file is deleted (one-time use)

#### Scenario: Release tag attempted without approval marker
- **WHEN** a Bash tool call runs `git tag v1.2.0` (or any `v<semver>` pattern) and `.claude/.merge-approved` does not exist
- **THEN** the command is denied with the same reasoning as the merge case

#### Scenario: Non-merge, non-tag git commands are unaffected
- **WHEN** a Bash tool call runs `git status`, `git diff`, `git commit`, or `git checkout`
- **THEN** this guard does not intervene

