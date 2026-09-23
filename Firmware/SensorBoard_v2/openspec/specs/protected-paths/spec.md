# protected-paths Specification

## Purpose
TBD - created by archiving change adapt-genesis-esp-idf. Update Purpose after archive.
## Requirements
### Requirement: Secret and environment files are protected from edits
The system SHALL deny `Edit`/`Write` tool calls targeting `.env` or `.env.*` files, except `.env.example`.

#### Scenario: Editing a real .env file
- **WHEN** an Edit or Write tool call targets a path ending in `.env` or matching `.env.*`
- **AND** the filename is not exactly `.env.example`
- **THEN** the call is denied with a reason pointing to `.env.example` or the environment manager

### Requirement: Dependency lockfiles are protected from direct edits
The system SHALL deny `Edit`/`Write` tool calls targeting `dependencies.lock` or files under `managed_components/`, since these are only meant to change via the ESP-IDF component manager, not manual edits.

#### Scenario: Editing dependencies.lock directly
- **WHEN** an Edit or Write tool call targets `dependencies.lock` (at any path depth)
- **THEN** the call is denied with a reason explaining it changes only via `idf.py add-dependency` / the component manager

#### Scenario: Editing a file under managed_components
- **WHEN** an Edit or Write tool call targets a path under `managed_components/`
- **THEN** the call is denied with the same reasoning

### Requirement: Git internals are protected
The system SHALL deny `Edit`/`Write` tool calls targeting any path under `.git/`.

#### Scenario: Editing a file inside .git
- **WHEN** an Edit or Write tool call targets a path containing `/.git/` or starting with `.git/`
- **THEN** the call is denied

### Requirement: OpenSpec's archived specs are protected from direct edits
The system SHALL deny `Edit`/`Write` tool calls targeting files under `openspec/specs/`, since these are the source of truth updated only via `openspec archive`.

#### Scenario: Editing an archived spec directly
- **WHEN** an Edit or Write tool call targets a path under `openspec/specs/`
- **THEN** the call is denied with a reason pointing to creating a change with a delta spec and running `openspec archive` instead

#### Scenario: Editing a spec delta inside an open change
- **WHEN** an Edit or Write tool call targets a path under `openspec/changes/<name>/specs/`
- **THEN** the call is allowed (delta specs inside an open change are exactly what should be edited)

