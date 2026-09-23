## Context

Board affected: **motherBoard**. The producer half is `Firmware/flasher_tool/`, outside this OpenSpec root.

See `proposal.md` for why. The short version: the Onomondo key is fleet-wide, it currently reaches the board as a compile-time macro, and `Credentials_public.h`'s `#if __has_include("Credentials.h")` means every build on the credentials machine embeds it — including the one that becomes a public release asset.

What the design has to fit into:

- **The operator constraint decides the architecture.** Activation runs on the board because making the operator flash, reboot, read the CCID and then activate is unworkable on a line. Confirmed by the user on 2026-09-10; an earlier PC-flasher variant was rejected on 2026-09-06 for this reason. Any design that adds an operator step is out.
- `flasher_tool/flasher/nvs_gen.py` already **generates and flashes an NVS partition image** carrying `mb_cfg`/`serial`. The mechanism is proven in production; what it lacks is string entries. `_make_entry()` packs the value into `e[24:32]` — eight bytes — and declares `span = 1`, `chunk_index = 0xFF`, with only `_T_U8` (0x01) and `_T_I32` (0x14) defined.
- The firmware reads the serial from the same namespace: `NS_CFG = "mb_cfg"`, `KEY_SERIAL = "serial"` in `include/config/preferences_keys.h`. So producer and consumer already agree on a namespace.
- `ONOMONDO_API_KEY` is referenced in exactly two places: `httpRequest()` in `ftest_sim_activation.cpp` (the `authorization:` header) and the dummy comparison in `ftest_sim_activation_start()`. Nothing else in either board uses it.
- The existing spec requirement *Activación de la SIM Onomondo* (`openspec/specs/mb-factory-test/spec.md`) already lists `build sin clave` / `sin key` among the WARN reasons, so the "no key" path exists and is specified — only its cause changes.

## Goals / Non-Goals

**Goals:**

- No firmware image contains the Onomondo key, in any environment, on any build machine — provable with `strings`.
- The operator's flow is byte-for-byte the same: one flash action, the board activates its own SIM.
- One binary per hardware revision again; the `*_factory` split disappears.
- A missing key degrades exactly as a missing network does today: WARN with a reason, never a silent PASS and never a FAIL blamed on the board.
- The key never appears in a log, a test `detail` field, a telemetry key or a crash dump.

**Non-Goals:**

- Making the key unreadable from a board in someone's hands. That needs flash encryption and is a separate proposal.
- Any compile-time fallback. It is the single thing that would undo the change.
- Rotating the current key (operational), or moving activation off the board (rejected by the operator constraint).
- Provisioning other credentials this way, however tempting.

## Decisions

### 1. NVS, in `mb_cfg`, as a string

New key in the namespace the flasher already writes and the firmware already reads. `constexpr char KEY_ONOMONDO_KEY[] = "onomondo_key";` — 12 characters, inside NVS's 15-character limit.

*Why `mb_cfg` and not a namespace of its own:* the flasher writes one NVS image and the firmware opens `mb_cfg` at boot already. A second namespace buys isolation that nothing here needs and doubles the producer's work.

*Why a string and not a blob:* it is text, `Preferences::getString()` reads it directly, and a string entry is what ESP-IDF's own generator emits for this.

### 2. No compile-time fallback, and the macros are deleted

`ONOMONDO_API_KEY` and `ONOMONDO_API_KEY_DUMMY` come out of `Credentials_public.h`. A leftover definition in someone's untracked `Credentials.h` is then dead code that nothing reads.

*Why deleting beats keeping a fallback:* a fallback would mean the key can still enter a binary whenever the macro happens to be defined — which is precisely today's failure, reintroduced as a convenience. The dummy-key comparison also stops being needed: "no key" is now an empty string, which is simpler to reason about than a sentinel value that must match a constant.

*Consequence to accept:* a developer cannot run `sim_act` against the live API from a bench build without provisioning a key into their board's NVS. That is the correct amount of friction for a credential that controls the fleet.

### 3. Validated at the read, and the value never leaves the module

A getter in `ftest_sim_activation.cpp` reads the key once, on first use, into a static buffer, and validates it: non-empty after trimming, length within bounds, printable ASCII only. Anything else counts as "no key".

*Why validate at all:* NVS content is not a compile-time constant any more. A truncated write, a half-flashed image or a wrong config field would otherwise be sent to `api.onomondo.com` as an `authorization` header and produce a confusing 401 instead of an honest WARN. The validation is pure logic and belongs in `[env:native]`.

*Bounds:* accept up to 96 characters. The observed format is `onok_` plus two dot-separated segments (~40 characters); 96 leaves room for a longer key without inviting a blob.

*Never logged:* the current code already keeps the key out of logs and out of the test `detail`. That property is preserved and stated as a requirement, because a getter is an easier thing to accidentally print than a macro.

### 4. The absent-key path reuses `FTEST_SIM_UNREACHABLE`

`ftest_sim_activation_start()` finishes with `FTEST_SIM_UNREACHABLE` and detail `sin key`, exactly as it does today for a dummy-key build. The runner paints it WARN.

*Why not a new state:* the semantics are identical — the SIM's state is unknown and the board is not at fault. Adding a state would change `factory_test_api.h` and the HMI's rendering for no gain. Keeping the detail string `sin key` also means the existing spec scenarios and `PROTOCOL.md` row 28 stay true.

### 5. `nvs_gen.py`: extend it rather than adopt ESP-IDF's generator

`nvs_gen.py` is ~200 lines, has its own pytest coverage, and is already frozen into the distributed `.exe` with no ESP-IDF dependency. Adding a string entry type means a second 32-byte entry carrying the data span plus the variable-length payload, and the CRC discipline the module already documents.

*Alternative considered — delegate to `nvs_partition_gen.py`:* correct by construction and battle-tested, but it drags an ESP-IDF dependency (or a vendored copy) into a PyInstaller bundle that today needs neither, and it changes how the whole NVS image is produced, including the serial number path that works. The risk of touching the working path outweighs the correctness argument at this size. **If the string entry turns out to need more of the format than expected, this decision flips** — the round-trip test in the tasks is what would reveal it, and it must run against a real board before this lands.

### 6. The key's home on the workshop PC

`flasher_config.json`, read by the existing `load_config()`, not versioned, next to the executable. Absent key → the flasher writes the NVS image without it and says so in its log, rather than failing the flash: a board flashed without a key is a board whose SIM needs activating another way, not a brick.

*Why not an environment variable or a prompt:* the workshop flashes in batches across three slots; a per-run prompt is an operator step, which the constraint forbids. A file that is set up once per machine is the right granularity.

## Risks / Trade-offs

- **A board in someone's hands can be dumped and the key read** → Accepted and stated in the proposal. It is a per-unit attack on the organization's own hardware rather than a publication to the internet, which is the trade this change is making. A restricted activate-only key from Onomondo would shrink the blast radius under any design and is worth requesting independently.
- **Every provisioned board carries the key, so rotating it means reflashing or an NVS write to each board** → Real, and worse than the macro world only in that there are more copies. Mitigated by the fact that a board only needs the key during factory test: a post-activation wipe of the NVS entry is a plausible follow-up, deliberately not in scope here because it interacts with re-running the battery on a returned unit.
- **`nvs_gen.py` writing a malformed string entry would be silent** — the board would read garbage or nothing and report WARN, which looks like a network problem → This is why the round-trip verification against a real board is a task and not an optional check, and why the read path validates instead of trusting.
- **The flasher overwrites the whole NVS partition**, so any key already there is replaced and other `mb_cfg` values are reset → Already true today for the serial number, and factory flashing is the moment when that is correct. Worth stating so nobody uses this path on a deployed unit.
- **Removing the `*_factory` environments un-does a change that is not merged yet** → If the stopgap lands first the removal is a small revert; if it does not, it never existed. Either way `pick_pio_env_dir()`'s exclusion and its tests stay, as a guard against the pattern coming back.

## Migration Plan

No protocol version bump and no NVS migration: the key is a new entry, absent on every existing board, and absent means WARN `sin key` — the same thing a build without credentials reports today.

Boards already in the field are unaffected: they have activated SIMs and will never run `sim_act` again. Boards on the bench lose the ability to run `sim_act` against the live API until their NVS carries a key, which is intended.

Order matters: **the flasher must be able to write the key before the firmware stops reading the macro**, or there is a window in which no board can be activated at all. So phase the flasher first, verify the round trip on one board, and only then remove the macro from the firmware.

Rollback is a firmware downgrade plus restoring the macro; the NVS entry is then ignored.

## Open Questions

- Does Onomondo issue **restricted** API keys (activate-only, or scoped to a SIM range)? It does not block this change, but it would materially reduce what a leaked or dumped key can do, and it is worth asking before 200 units carry one.
- Should the key be **wiped from NVS** once `sim_act` has passed, so a shipped unit does not carry it? It shrinks the exposure to the factory floor, at the cost of not being able to re-run the battery on a returned unit without re-provisioning. Deliberately out of scope until someone decides which matters more.
- Is `mb_cfg` the right namespace long-term, or should provisioned secrets get their own (`mb_prov`) so that a future "reset configuration" cannot wipe them along with the tunables? Cheap to decide now, expensive later.
