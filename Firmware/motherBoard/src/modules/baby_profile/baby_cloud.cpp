#include "baby_cloud.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "config/telemetry_keys.h"

namespace {

// Bounded append; flags overflow instead of writing past len.
void appendf(char *buf, size_t len, int *pos, bool *ovf, const char *fmt, ...) {
  if (*ovf || *pos < 0 || (size_t)*pos >= len) {
    *ovf = true;
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  int w = vsnprintf(buf + *pos, len - (size_t)*pos, fmt, ap);
  va_end(ap);
  if (w < 0 || (size_t)(*pos + w) >= len) {
    *ovf = true;
    return;
  }
  *pos += w;
}

// Names come from a touchscreen and land inside a JSON string: escape the
// two characters that would otherwise break the payload. The on-screen
// keyboard is letters-only today, but the cloud contract must not depend on
// that staying true.
void appendJsonString(char *buf, size_t len, int *pos, bool *ovf,
                      const char *s) {
  appendf(buf, len, pos, ovf, "\"");
  for (const char *p = s; *p; p++) {
    if (*p == '"' || *p == '\\') {
      appendf(buf, len, pos, ovf, "\\%c", *p);
    } else if ((unsigned char)*p < 0x20) {
      continue;  // drop control chars outright
    } else {
      appendf(buf, len, pos, ovf, "%c", *p);
    }
  }
  appendf(buf, len, pos, ovf, "\"");
}

void appendCommonProfileKeys(char *buf, size_t len, int *pos, bool *ovf,
                             const BabyProfile *p) {
  appendf(buf, len, pos, ovf, "\"" BABY_SEQ_KEY "\":%u", (unsigned)p->seq);
  appendf(buf, len, pos, ovf, ",\"" BABY_NAME_KEY "\":");
  appendJsonString(buf, len, pos, ovf, p->name);
  appendf(buf, len, pos, ovf, ",\"" BABY_GEST_AGE_KEY "\":%u",
          (unsigned)p->gestWeeks);
  appendf(buf, len, pos, ovf, ",\"" BABY_WEIGHT_KEY "\":%u",
          (unsigned)p->weightGrams);
  appendf(buf, len, pos, ovf, ",\"" BABY_KANGAROO_COUNT_KEY "\":%u",
          (unsigned)p->kangarooCount);
  appendf(buf, len, pos, ovf, ",\"" BABY_PHOTO_MINUTES_KEY "\":%u",
          (unsigned)p->phototherapyMinutes);
  appendf(buf, len, pos, ovf, ",\"" BABY_THERMO_MINUTES_KEY "\":%u",
          (unsigned)p->thermoMinutes);
  appendf(buf, len, pos, ovf, ",\"" BABY_HUMIDITY_MINUTES_KEY "\":%u",
          (unsigned)p->humidityMinutes);
}

}  // namespace

int babyCloud_buildEventJson(const BabyCloudEvent *e, char *buf, size_t len) {
  if (!e || e->type == BABY_EVT_NONE) return 0;
  int pos = 0;
  bool ovf = false;

  // Explicit ts only when the clock was actually synced; otherwise let the
  // server stamp it rather than claim 1970.
  bool haveTs = (e->ts != 0);
  if (haveTs) {
    appendf(buf, len, &pos, &ovf, "{\"ts\":%llu,\"values\":{",
            (unsigned long long)e->ts * 1000ull);
  } else {
    appendf(buf, len, &pos, &ovf, "{");
  }

  switch (e->type) {
    case BABY_EVT_WEIGHT:
      appendf(buf, len, &pos, &ovf, "\"" BABY_SEQ_KEY "\":%u",
              (unsigned)e->profile.seq);
      appendf(buf, len, &pos, &ovf, ",\"" BABY_WEIGHT_KEY "\":%u",
              (unsigned)e->weightGrams);
      break;

    case BABY_EVT_KANGAROO:
      appendf(buf, len, &pos, &ovf, "\"" BABY_SEQ_KEY "\":%u",
              (unsigned)e->profile.seq);
      appendf(buf, len, &pos, &ovf, ",\"" BABY_KANGAROO_EVENT_KEY "\":1");
      appendf(buf, len, &pos, &ovf, ",\"" BABY_KANGAROO_COUNT_KEY "\":%u",
              (unsigned)e->profile.kangarooCount);
      break;

    case BABY_EVT_DISCHARGE:
      // One self-contained row per baby: this is what the history table and
      // the ministry export read, so it must stand alone without joining
      // against anything else.
      appendCommonProfileKeys(buf, len, &pos, &ovf, &e->profile);
      appendf(buf, len, &pos, &ovf, ",\"" BABY_ADMISSION_EPOCH_KEY "\":%u",
              (unsigned)e->profile.admissionEpoch);
      appendf(buf, len, &pos, &ovf, ",\"" BABY_DISCHARGE_EPOCH_KEY "\":%u",
              (unsigned)e->profile.dischargeEpoch);
      appendf(buf, len, &pos, &ovf, ",\"" BABY_OUTCOME_KEY "\":%u",
              (unsigned)e->profile.outcome);
      if (e->profile.admissionEpoch != 0 && e->profile.dischargeEpoch != 0 &&
          e->profile.dischargeEpoch >= e->profile.admissionEpoch) {
        appendf(buf, len, &pos, &ovf, ",\"" BABY_STAY_DAYS_KEY "\":%u",
                (unsigned)((e->profile.dischargeEpoch -
                            e->profile.admissionEpoch) /
                           86400u));
      }
      break;

    default:
      return 0;
  }

  appendf(buf, len, &pos, &ovf, haveTs ? "}}" : "}");
  return ovf ? 0 : pos;
}

int babyCloud_buildAttributesJson(const BabyProfile *p, char *buf,
                                  size_t len) {
  if (!p) return 0;
  int pos = 0;
  bool ovf = false;
  appendf(buf, len, &pos, &ovf, "{");
  appendCommonProfileKeys(buf, len, &pos, &ovf, p);
  appendf(buf, len, &pos, &ovf, ",\"" BABY_ADMISSION_EPOCH_KEY "\":%u",
          (unsigned)p->admissionEpoch);
  appendf(buf, len, &pos, &ovf, "}");
  return ovf ? 0 : pos;
}

int babyCloud_buildEmptyAttributesJson(char *buf, size_t len) {
  int pos = 0;
  bool ovf = false;
  // Null rather than omit: omitted keys keep their previous value in
  // ThingsBoard, which would leave a discharged baby on the status cards.
  appendf(buf, len, &pos, &ovf,
          "{\"" BABY_SEQ_KEY "\":0,\"" BABY_NAME_KEY "\":\"\","
          "\"" BABY_GEST_AGE_KEY "\":0,\"" BABY_WEIGHT_KEY "\":0,"
          "\"" BABY_KANGAROO_COUNT_KEY "\":0,"
          "\"" BABY_PHOTO_MINUTES_KEY "\":0,"
          "\"" BABY_THERMO_MINUTES_KEY "\":0,"
          "\"" BABY_HUMIDITY_MINUTES_KEY "\":0,"
          "\"" BABY_ADMISSION_EPOCH_KEY "\":0}");
  return ovf ? 0 : pos;
}
