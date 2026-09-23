## Why

Boards affected: **motherBoard** (reads the key from NVS instead of a compiled-in macro). `Display_HMI` and `shared/` are untouched. No area with a documented bug in `Firmware/docs/known_issues.md` is involved. The producer side lives in `Firmware/flasher_tool/` — outside this OpenSpec root, but a hard dependency; see Impact.

`ONOMONDO_API_KEY` is a **fleet-wide** credential: it can activate or deactivate every SIM the organization owns. Today it reaches the motherBoard as a compile-time macro from `Credentials.h`, and `Credentials_public.h` pulls that file in with `#if __has_include("Credentials.h")` — so on the machine that holds the real credentials, **every** build embeds it, including the `IncuNest_V18` that populates `flasher_tool/data/firmware/` and becomes a GitHub Release asset on a public repository. Verified on the real binary: `strings firmware.bin` finds the key.

The stopgap already on `fix/mb-onomondo-key-solo-fabrica` (`6de94fe`, `719b65d`) gates the activation behind `FTEST_SIM_ACT_ENABLED`, so only the new `*_factory` environments contain the key. It works — the field binary now has zero matches — but it buys that safety with a permanent split: two binaries per hardware revision, and a rule that the wrong one must never be packaged. Building that split immediately produced its own failure: the flasher's "update locals" ranks build directories by the revision number parsed out of the environment *name*, so `IncuNest_V18_factory` scored the same 18 as `IncuNest_V18` and the tiebreak was mtime — the factory binary would have been copied into the released folder.

The constraint that rules the design is the operator's, and it is not negotiable: activation happens **on the board** because making the operator flash, reboot and then read the CCID before activating is unworkable on a production line. Any solution has to keep flashing a one-step action.

This change removes the credential from every binary while leaving that flow exactly as it is: the flasher writes the key into NVS during flashing — the same mechanism that already writes the serial number — and the firmware reads it from there. The key ends up on the workshop PC and in each board's NVS, and in no firmware image. **The two-build split then becomes unnecessary and is removed**, which also removes the entire class of "packaged the wrong binary" mistakes.

This matters now because ~200 units are about to be onboarded, so the factory binary would otherwise circulate on several assembly machines.

## What Changes

- **The motherBoard reads the Onomondo key from NVS**, namespace `mb_cfg` (`NS_CFG`), alongside the serial number the flasher already writes there. Absent or empty key → the existing WARN path, unchanged in spirit: the unit's SIM state is unknown, which is not a board fault.
- **BREAKING (build inputs, not wire or behaviour): `ONOMONDO_API_KEY` and `ONOMONDO_API_KEY_DUMMY` are removed from `Credentials_public.h`** and are no longer read from `Credentials.h`. A stale definition in someone's untracked `Credentials.h` becomes dead and harmless. There is deliberately **no compile-time fallback**: a fallback is exactly what would reintroduce the leak.
- **`FTEST_SIM_ACT_ENABLED` and the `*_factory` environments are removed.** One binary per hardware revision again. `pick_pio_env_dir()`'s factory exclusion in the flasher stays as a cheap guard against the pattern returning, and its tests stay green.
- **The flasher gains string support in its NVS image generator.** `nvs_gen.py` today writes only 8-byte primitives (`_make_entry` packs the value into `e[24:32]`, types `_T_U8`/`_T_I32`); an API key is a ~40-character string, which in NVS v2 needs a string entry with its own data span. Either extend `nvs_gen.py` or delegate to ESP-IDF's `nvs_partition_gen.py`.
- **The key's home on the workshop PC is `flasher_config.json`**, which `load_config()` already reads and which is not versioned.
- **No change to the operator flow, to `sim_act`'s place in the battery, to its states, or to the `CTRL,FTEST` protocol.** Only the reason string for a missing key changes.

## Capabilities

### New Capabilities
- `onomondo-key-provisioning`: where the Onomondo API key lives and how it reaches the board — the NVS contract (namespace, key name, string type, length bounds, what an absent or malformed value means), the read path and its lifetime on the board, the prohibition on any compile-time fallback or logging of the value, the return to a single binary per hardware revision, and the invariant that no firmware image may contain the key. Also carries the **producer contract** the flasher must satisfy, so the two halves cannot drift apart silently.

### Modified Capabilities
- `mb-factory-test`: the requirement *Activación de la SIM Onomondo* changes where the key comes from. Its WARN reasons list `build sin clave` / `sin key`, which stop existing as such: the case becomes "no key provisioned in NVS". The states, the ICCID prefix gate, the FAIL condition and the timeout are all unchanged.

## Impact

- **Affected code — motherBoard**: `src/modules/factory_test/ftest_sim_activation.{h,cpp}` (the key comes from a getter over NVS instead of a macro; the `FTEST_SIM_ACT_ENABLED` guard added by `6de94fe` is removed), `include/config/preferences_keys.h` (new `mb_cfg` key), `include/protocol/Credentials_public.h` (both Onomondo macros removed), `platformio.ini` (the two `*_factory` environments removed).
- **Affected code — flasher_tool (outside this OpenSpec root, hard dependency)**: `flasher/nvs_gen.py` (string entries), `flasher/main.py` (read the key from `flasher_config.json` and feed it to the NVS image; never log it), and its pytest suite — which is the only automated test environment either half of this change has for the producer side.
- **Affected docs**: `Firmware/PROTOCOL.md` (row 28's note, reverting the `*_factory` wording added by `6de94fe`), `Firmware/docs/thingsboard_dashboards.md` §9 (the section on the key not travelling in the distributed firmware, rewritten around NVS), and the flasher's own documentation of what it writes to NVS.
- **Depends on the stopgap being in place or superseded**: `6de94fe` and `719b65d` are unmerged at the time of writing. If they land first, this change removes the `FTEST_SIM_ACT_ENABLED` half of them and keeps the flasher guard. If they do not, this change closes the leak on its own and the two-build split never exists.
- **Security posture, stated plainly**: the key stops being distributable but does not become secret. Anyone holding a board can dump its flash and read it. That is a per-unit attack on the organization's own hardware, not a publication — an acceptable trade for removing it from a public release asset. **A restricted Onomondo key (activate-only, or scoped to a SIM range), if the operator offers one, would shrink the blast radius under any design and should be requested independently of this change.**
- **Testing**: the pure logic this adds on the firmware side is the validation of a key read from NVS (empty, whitespace-only, over-length, non-printable bytes) — host-testable, and if it lands as its own function it must be added to `[env:native]`'s `build_src_filter` with Unity tests in the same commit per `.claude/rules/testing.md`. The NVS image generation is testable in the flasher's pytest suite, including a round-trip against the entry layout. Everything else is **manual verification only** and must be documented as such: a real board flashed with a provisioned key activating a real Onomondo SIM, a board flashed without one reporting WARN, and `strings firmware.bin` finding no key in either environment.
- **Out of scope (Non-goals)**:
  - Encrypting the NVS partition or enabling flash encryption / secure boot. That is the answer to "a board in someone's hands can be dumped", it affects every secret on the device rather than this one, and it is a separate proposal.
  - Rotating the current key. It should be rotated once this lands, but the rotation is an operational task, not a code change.
  - Moving activation to the PC flasher. Explicitly rejected by the operator constraint above.
  - Provisioning any other credential through NVS (WiFi, ThingsBoard). The mechanism generalizes and that is welcome, but each one is its own decision.
