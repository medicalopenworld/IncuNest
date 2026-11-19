Implementation already landed in commits `7905dd8f`, `18274697` and `fb675112`; this change records it in the spec.

## 1. Spec

- [x] 1.1 Delta spec: modified whitelist, value interpretation, subscription and echo; added remote rules, deferral and rate, local override and reapply, audit, SDK patches.
- [x] 1.2 `openspec validate mb-remote-config-restrict --strict` green.

## 2. Verification

- [x] 2.1 `pio test -e native`: 539 cases green (56 in `test_runtime_config`).
- [x] 2.2 `pio run -e IncuNest_V18` green; a clean-libdeps build applies the six patches.
- [ ] 2.3 MANUAL on a unit and a real ThingsBoard: periods change remotely, a safety attribute is ignored with `cfg_rejected`, a local change survives a reconnect, `cfg_change` arrives.
