## Context

See `docs/thingsboard_dashboards.md` section 10 and `motherBoard/src/modules/config/runtime_config_policy.h` for the long rationale. Summary: unencrypted MQTT means shared attributes are unauthenticated, so remote authority is limited to what is recoverable (reporting periods).

## Decisions

- **One switch**: `RCFG_REMOTE_SAFETY_PARAMS` (0 today). The remote rules live in `runtime_config_validate_remote()` and are tested regardless of the switch.
- **Per-key persistent state** (`mb_rcfg`, blob `k0..k8`, 12 B): last remote value seen, local override flag, origin and epoch of the last change. A deferred value is not recorded as seen, so it is retried after a reboot.
- **Callbacks do not publish**; the owning task publishes `cfg_rejected`, `cfg_change` and the echo, retrying after 5 s.
- **Mutex** around apply, echo, audit and state.
- **SDK patches** are text patches applied at build time, idempotent.

## Risks

- `RCFG_FAN_PWM_REMOTE_MIN = 80` and the 0.3 A "heater acting" threshold are not measured.
- Whether ThingsBoard resends the notification when the same value is written again is unverified.
