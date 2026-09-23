# build-verification-gate Specification

## Purpose
TBD - created by archiving change adapt-genesis-esp-idf. Update Purpose after archive.
## Requirements
### Requirement: Turn close is blocked on a failing compile
The system SHALL run `idf.py build` (and, when it exists, the build for the affected `test_apps/*` app) when the working tree has uncommitted changes to `.c` or `.h` files, and SHALL block closing the turn if the build fails.

#### Scenario: Uncommitted C changes that compile cleanly
- **WHEN** the Stop hook fires and `git diff` shows uncommitted changes to `*.c`/`*.h` files, and `idf.py build` exits 0
- **THEN** the turn is allowed to close with no blocking message

#### Scenario: Uncommitted C changes that fail to compile
- **WHEN** the Stop hook fires and `git diff` shows uncommitted changes to `*.c`/`*.h` files, and `idf.py build` exits non-zero
- **THEN** the turn close is blocked and the last lines of the build output are surfaced as the reason

#### Scenario: No uncommitted C changes
- **WHEN** the Stop hook fires and `git diff` shows no uncommitted `*.c`/`*.h` changes (e.g. a read-only or docs-only session)
- **THEN** `idf.py build` is not invoked and the turn closes normally

### Requirement: The gate never triggers on-device flashing or test execution
The system SHALL NOT invoke `idf.py flash`, `idf.py monitor`, or any command that communicates with a physical serial port, from any automated hook.

#### Scenario: Build succeeds but device is not connected
- **WHEN** `idf.py build` succeeds and no ESP32-S3 device is attached to the machine
- **THEN** the turn closes normally; on-device Unity verification remains a manual step for the developer

