## Context

`POST /config` (WiFi web server) and `/config,<param>,<value>` (USB / inter-board console) each carry their own copy of the same eight `if` blocks: parse, assign to `in3`, write to NVS, run the side effect. They disagree in small ways (the console accepts upper-case names and `fan_pid_en` as `value != 0`; the web uses `toInt()` and happily stores 0 for `abc`), and neither range-checks PWM or periods. The boot path (`recapVariables()` in `EEPROM.cpp`) already has a notion of "sane": PWM in 1..255, `heater_amps > 0`, tmax clamped to 34..40. This change makes that notion a single rule set that every origin (web, USB, ThingsBoard over GPRS, ThingsBoard over WiFi) goes through.

The ThingsBoard SDK is v0.13.0 (pinned; not to be bumped, see `platformio.ini`), built with `THINGSBOARD_ENABLE_DYNAMIC=1`. Facts verified in the vendored sources (`.pio/libdeps/IncuNest_V18/ThingsBoard/src/ThingsBoard.h`):

- `Shared_Attribute_Callback(function cb, Args... keys)` + `Shared_Attributes_Subscribe(cb)` register a permanent subscription on `v1/devices/me/attributes`. `process_shared_attribute_update_message()` calls the callback only when the update contains at least one of the subscribed keys, passing the whole `data` object (already unwrapped from `"shared"` when present).
- `Attribute_Request_Callback(function cb, Args... keys)` + `Shared_Attributes_Request(cb)` send one request with `sharedKeys:"k1,k2,..."` and are one-shot (removed on response). The response passes the unwrapped `shared` object, containing only the keys that exist.
- `connect()` installs `Resubscribe_Topics()` as the MQTT connect callback and keeps the subscription vectors, so a permanent subscription must be registered **once per client**, not after every reconnect. (The existing code re-adds the RPC callbacks on every reconnect, which duplicates them; that is pre-existing and left alone, but this change must not copy it.) One-shot requests are dropped on reconnect (`Attributes_Request_Unsubscribe()`), so the request **is** repeated on every connection.
- `sendAttributeJson()` publishes to the client-scope attribute topic. It is the "echo" channel; no fallback to `cfg_` telemetry is needed.
- Callbacks run inside `tb.loop()`, in the task that owns the client (GPRS task, WiFi/OTA task). They are not ISRs. They must nevertheless not publish: PubSubClient shares one buffer between the incoming message and `publish()`, so publishing from inside the callback can corrupt the payload being read. The callback therefore only applies and marks the echo as pending; the owning task publishes after `tb.loop()` returns.

## Goals / Non-Goals

**Goals**
- One validation and application point for the nine parameters, used by all four origins.
- Remote correction of the safety cut-outs and the GPRS cadence without a site visit, including for units that were off when the change was made.
- ThingsBoard shows the value in force, not the value requested.
- No dynamic allocation of our own; no growth of the heap pressure documented for WiFi+PPP.

**Non-Goals**
- Remote `serial`, `reference_temp`, `set_time`, `BABY_WIPE`.
- Authenticating attribute writers beyond ThingsBoard's own device authority.
- Touching the HMI, the protocol or the SDK version.

## Decisions

### D1. Module layout

```
src/modules/config/runtime_config_rules.{h,cpp}   pure; no Arduino.h; in [env:native]
src/modules/config/runtime_config.{h,cpp}         Arduino side; NOT in [env:native]
```

Pure API (`runtime_config_rules.h`):

```c
typedef enum { RCFG_FAN_SUPPLY_PWM, RCFG_FAN_CTL_PWM, RCFG_FAN_PID_EN,
               RCFG_HEATER_AMPS, RCFG_AIR_TMAX, RCFG_SKIN_TMAX,
               RCFG_GPRS_ACT, RCFG_GPRS_PHOTO, RCFG_GPRS_STBY,
               RCFG_COUNT } RuntimeConfigId;
typedef enum { RCFG_OK, RCFG_OK_CLAMPED, RCFG_REJ_NOT_FINITE, RCFG_REJ_RANGE,
               RCFG_REJ_NOT_INTEGER, RCFG_REJ_NOT_BOOL } RuntimeConfigStatus;

const char *runtime_config_name(RuntimeConfigId id);
bool        runtime_config_lookup(const char *name, RuntimeConfigId *id); // case-insensitive
RuntimeConfigStatus runtime_config_validate(RuntimeConfigId id, float in, float *out);
bool        runtime_config_parse_number(const char *text, float *out);    // strict
bool        runtime_config_json_number(JsonVariantConst v, float *out);   // number or bool only
const char *runtime_config_status_text(RuntimeConfigStatus s);
size_t      runtime_config_format_echo(const float values[RCFG_COUNT], uint16_t mask,
                                       char *buf, size_t len);            // 0 if it does not fit
```

`runtime_config_validate()` never writes `*out` on a rejection. Lookup is case-insensitive so the console keeps accepting `AIR_TMAX`; the ThingsBoard path never uses lookup: it walks the table and asks the JSON object for each exact name, so the whitelist cannot be widened by the attacker's spelling.

Arduino API (`runtime_config.h`):

```c
typedef enum { RCFG_ORIGIN_USB, RCFG_ORIGIN_WEB,
               RCFG_ORIGIN_TB_GPRS, RCFG_ORIGIN_TB_WIFI } RuntimeConfigOrigin;

bool   runtime_config_apply(RuntimeConfigOrigin o, const char *name, float value,
                            char *msg, size_t msg_len);
bool   runtime_config_apply_text(RuntimeConfigOrigin o, const char *name,
                                 const char *text, char *msg, size_t msg_len);
size_t runtime_config_apply_json(RuntimeConfigOrigin o, JsonObjectConst data);
void   runtime_config_echo_all(void);                    // mark the nine pending
size_t runtime_config_echo_build(char *json, size_t len, uint16_t *mask);
void   runtime_config_echo_done(uint16_t mask);          // clear after a successful publish
const char *const *runtime_config_keys(size_t *count);   // for the SDK key list
```

The first parameter `RuntimeConfigOrigin` is an addition to the signature sketched in the task: the origin has to reach the log line, and the apply function is the one that logs.

### D2. Rule per parameter (clamp versus reject)

A value is **clamped** only where the repo already clamps today and where a nearby value is a safe reading of the intent (the two temperature cut-outs, whose clamp range is a regulatory property: `shared/alarm_policy.h`). Everything else is **rejected**: a typo that silently becomes a different PWM or period is worse than no change. A rejection leaves `in3`, NVS and the hardware untouched.

| Parameter | Type | Rule | NVS (namespace / key) | Side effect |
|---|---|---|---|---|
| `fan_supply_pwm` | int | integer, 1..255, else reject | `NS_CFG` / `KEY_FAN_PWR_SUPPLY_PWM` (`putInt`) | none direct: `turnFans()` reads `in3.fanPwrSupplyPWM` on every HMI frame |
| `fan_ctl_pwm` | int | integer, 1..255, else reject | `NS_CFG` / `KEY_FAN_CTL_PWM` (`putInt`) | `ledcWrite(FAN_CTL_PWM_CHANNEL, v)` only if fan commanded on, PID not in AUTOMATIC, factory test not active |
| `fan_pid_en` | bool | exactly 0 or 1 (JSON `true`/`false` accepted), else reject | `NS_CFG` / `KEY_FAN_PID_EN` (`putUChar`, written by `setFanPidEnabled()`) | `setFanPidEnabled(v)` |
| `heater_amps` | float | finite, 1.0..12.0 A, else reject | `NS_CFG` / `KEY_HEAT_MAX_A` (`putFloat`) | none direct: `PID.cpp` reads `in3.heaterMaxPowerAmps` |
| `air_tmax` | float | finite, then `alarm_clamp_air_cutout()` (34.0..40.0 C) -> `RCFG_OK_CLAMPED` when changed | `NS_CFG` / `KEY_AIR_T_MAX` (`putFloat`) | `maxDesiredTemp[CONTROL_AIR] = v` |
| `skin_tmax` | float | finite, then `alarm_clamp_skin_cutout()` (34.0..40.0 C) | `NS_CFG` / `KEY_SKIN_T_MAX` (`putFloat`) | `maxDesiredTemp[CONTROL_SKIN] = v` |
| `gprs_act` | int | integer, 10..86400 s, else reject | `NS_GPRS` / `KEY_ACT_PERIOD` (`putInt`) | none direct: `GPRSSetPostPeriod()` reads it each `GPRS_Handler()` pass |
| `gprs_photo` | int | integer, 10..86400 s, else reject | `NS_GPRS` / `KEY_PHOTO_PERIOD` (`putInt`) | idem |
| `gprs_stby` | int | integer, 10..86400 s, else reject | `NS_GPRS` / `KEY_STBY_PERIOD` (`putInt`) | idem |

Why these bounds:

- **PWM 1..255**: the LEDC channels are 8-bit (`DEFAULT_PWM_RESOLUTION 8`) and `recapVariables()` already treats `<= 0` or `> 255` as corrupt and restores the default at boot. Accepting 0 would be applied now and silently reverted at the next reboot, which is the worst of both. Accepting 0 would also stop the fan supply.
- **`heater_amps` 1.0..12.0**: lower bound well under the 7.5/9.5 A defaults but above "effectively off" (the PID would never heat); upper bound above the largest default with margin and below what the supply and wiring are specified for. It is a current limit protecting hardware, so a remote writer must not be able to lift it indefinitely; out of range is rejected, not clamped. Documented here so changing it is a deliberate edit of one constant.
- **Periods 10..86400 s**: the shortest default is 60 s; 10 s leaves bench use possible while preventing a `0` (publication every pass, which floods the modem and the SIM budget) and a value so large that the unit appears dead (> 1 day). The upper bound also keeps `secsToMillis()` far from overflow.
- **tmax clamp, not reject**: this is today's web behaviour and the existing contract of `alarm_clamp_*_cutout()` (known_issues #19). A value of 45 becomes 40 and the echo tells the operator so.
- **Non-integer for integer parameters is rejected** (130.5 PWM): the old `toInt()`/`(int)` truncation hid typos.
- **NaN/Inf** are rejected for every parameter before any clamp (`clamp_range` has no defined behaviour for NaN).
- **JSON strings are rejected** (`"air_tmax": "38"`): a number typed as text in the ThingsBoard UI is far more often a mistake than intent, and refusing it keeps the type contract trivial.

Accepted risk, same as the web today: `air_tmax` may be set below `ALARM_AIR_SETPOINT_MAX_C` (39.0), which makes setpoints above it trip `ALARM_AIR_THERMAL_CUTOUT`. The rule does not forbid it because lowering a cut-out is a legitimate safety action; the log line makes it visible.

### D3. Apply is idempotent and spares NVS

If the validated value equals what `in3` already holds, nothing is written to NVS and no side effect runs; the call still succeeds and still schedules the echo. The post-connect request therefore costs no flash writes in the common case where TB and the board agree. Writes use the existing `Preferences p; p.begin(NS, false); p.putX(...); p.end();` idiom.

### D4. ThingsBoard wiring (per client, identical on both)

```
static bool s_subscribed;                 // one per client: GPRS task / WiFi task
on connect success:
    if (!s_subscribed) { tb.Shared_Attributes_Subscribe(updateCb); s_subscribed = true; }  // hot changes
    tb.Shared_Attributes_Request(requestCb);                                              // values changed while off
    runtime_config_echo_all();                                                           // publish what we hold
each pass, connected:
    n = runtime_config_echo_build(json, sizeof json, &mask);
    if (n && tb.sendAttributeJson(json)) runtime_config_echo_done(mask);                 // retry until the broker takes it
callbacks (both): runtime_config_apply_json(ORIGIN_TB_xxx, data);
```

- `updateCb` is a `Shared_Attribute_Callback` built from `runtime_config_keys()`; `requestCb` is an `Attribute_Request_Callback` from the same list. The key list is a static array of `const char *`, so the SDK copies nine pointers (36 B) per callback and never the strings.
- The callback body is the same on both clients apart from the origin tag; it is a plain function, not a `std::function` capturing state, to keep the object small.
- `s_subscribed` is reset only when the process restarts; `Cleanup_Subscriptions()` is never called by the firmware (verified by grep).
- Failure handling: if `Shared_Attributes_Subscribe` fails (topic subscribe refused) `s_subscribed` stays false and the next connection retries; if `Shared_Attributes_Request` fails it is logged and not retried until the next connection (the hot subscription still works).
- The echo is a retry-until-sent bitmask, like `s_wifiAttrsDirty`: a failed publish leaves the bits set and is retried on the next pass; a successful one clears exactly the bits that were in that message.
- Echo payload size: nine numeric keys, at most `{"fan_supply_pwm":255,"fan_ctl_pwm":255,"fan_pid_en":1,"heater_amps":12.00,"air_tmax":40.00,"skin_tmax":40.00,"gprs_act":86400,"gprs_photo":86400,"gprs_stby":86400}` = ~190 B. Built with a `StaticJsonDocument<JSON_OBJECT_SIZE(9)>` (144 B on the stack, keys by pointer) into a `char[224]` buffer, well under `MAX_MESSAGE_SIZE` (1024).
- `fan_pid_en` is echoed as JSON `true`/`false` (not 1/0), so it round-trips with the boolean shared attribute a ThingsBoard switch widget writes. The echo payload therefore reads `"fan_pid_en":false`, which is at most 5 B longer than the estimate above.

### D5. Local paths

`POST /config`: for each of the nine names present, `runtime_config_apply_text(RCFG_ORIGIN_WEB, ...)`; a rejection stops processing and answers `400 text/plain` with the module's message, without applying the remaining arguments. Names `serial`, `reference_temp`, `set_time` keep their inline code. The success body stays `Saved. Settings applied immediately.`

Console: `FAN_SUPPLY_PWM`, `HEATER_AMPS`, `SKIN_TMAX`, `AIR_TMAX`, `GPRS_*`, `FAN_CTL_PWM`, `FAN_PID_EN` map onto `runtime_config_apply(RCFG_ORIGIN_USB, ...)`; `BABY_WIPE` and the "unknown parameter" branch stay in `CommTask.cpp`. The console logs `Config updated: NAME = value` as before on success and the rejection reason otherwise.

### D6. Logging

One line per application, through `logI(...)` (the project's logger; note `LOG_INFORMATION` may be compiled out, so rejections of safety parameters are additionally emitted with `ESP_LOGW` to be visible on the bench, the same trade-off documented in `EEPROM.cpp`):

```
[CFG] origin=tb-gprs name=air_tmax requested=45.00 applied=40.00 (clamped)
[CFG] origin=web name=fan_ctl_pwm requested=300.00 rejected: out of range 1..255
```

### D7. Threading

Callbacks and the web handler run in different tasks (GPRS task, WiFi/OTA task, CommTask). `in3` fields are plain ints/floats written by one task and read by others already today (the web handler and the console write the same fields); NVS is thread-safe. The pending-echo mask is a `volatile uint16_t`, set by the applying task and cleared by the publishing task; a lost update would at worst cause one redundant echo. Only one ThingsBoard client owns the connection at a time (`WIFIIsConnected()` gates the GPRS client), so concurrent remote applications from both transports are limited to the hand-over instant. No mutex is added.

## Risks / Trade-offs

- **A wrong remote value on a unit in clinical use.** Mitigation: whitelist, type check, range rule, log with origin, effective-value echo. The cut-outs cannot leave 34..40 C regardless of the writer. Nine keys can still be mis-set inside their range (e.g. `gprs_stby` to 10 s costs data); that is operational, not a safety property.
- **A shared attribute that ThingsBoard keeps re-sending** (the request response on every connection) re-applies the same values. Idempotence (D3) makes that free.
- **SDK dynamic allocation.** The SDK allocates the incoming JSON document and its callback vectors on the heap. The cost is one small vector per client for the lifetime of the process plus a transient ~500 B document per received attribute message, comparable to an RPC. Nothing in the module allocates.
- **GPRS data.** See the proposal; a few hundred bytes per connection. The post-connect request could be restricted to WiFi if a plan with metered data makes that matter; it is written once and shared, so it is one line to drop for GPRS.
- **Duplicate RPC subscription on reconnect** in both clients is pre-existing technical debt and out of scope; the new subscription deliberately does not repeat it.
- **`fan_ctl_pwm` side effect is more conservative than the old web handler** (D2 table). A change made while the fan is stopped takes effect at the next `turnFans()`, which is how the value was always consumed.

## Open Questions

- Whether the request after connecting should be skipped on GPRS to save data; it is kept because correcting a unit that was off is the main use case (#19).
- The 12.0 A and 10 s limits are the author's judgement from the code (defaults 7.5/9.5 A and 60 s; PWM resolution), not from a hardware datasheet; they should be confirmed by whoever owns the heater/PSU specification.
