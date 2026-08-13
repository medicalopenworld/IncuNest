#pragma once
// ThingsBoard payload builders for baby-profile data.
//
// Split by lifetime, because the two need different ThingsBoard mechanisms:
//   - "who is in the incubator right now" -> client ATTRIBUTES (overwritten,
//     no history, drives the status cards)
//   - "what happened" -> TELEMETRY events carrying their own timestamp, so a
//     point lands at the time it actually occurred rather than the time the
//     modem managed to publish it.
//
// Every payload carries baby_seq. Without it the cloud cannot tell one baby
// from the next, and cumulative per-baby counters read as nonsense when a
// new baby resets them.
//
// Pure string building only — no MQTT, no globals — so it is unit-testable.
#include <stddef.h>
#include <stdint.h>

#include "baby_profile_core.h"

enum BabyCloudEventType : uint8_t {
  BABY_EVT_NONE = 0,
  BABY_EVT_WEIGHT = 1,     // one growth-curve point
  BABY_EVT_KANGAROO = 2,   // baby went out with the mother
  BABY_EVT_DISCHARGE = 3,  // full end-of-stay record, one row per baby
};

struct BabyCloudEvent {
  uint8_t type;
  uint32_t ts;          // epoch seconds; 0 = clock was not synced
  uint16_t weightGrams; // BABY_EVT_WEIGHT only
  BabyProfile profile;  // snapshot taken at event time
};

// {"ts":<ms>,"values":{...}} when ts is known, else a plain {...} object so
// ThingsBoard stamps it on arrival. Returns chars written, 0 on overflow.
int babyCloud_buildEventJson(const BabyCloudEvent *e, char *buf, size_t len);

// Flat object of client attributes describing the current occupant.
// Returns chars written, 0 on overflow.
int babyCloud_buildAttributesJson(const BabyProfile *p, char *buf, size_t len);

// Attribute payload that clears the occupancy cards when the incubator is
// empty (all keys nulled rather than left showing a discharged baby).
int babyCloud_buildEmptyAttributesJson(char *buf, size_t len);
