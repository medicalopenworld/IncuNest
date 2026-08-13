#include "baby_profile_protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

// Strict unsigned parse of a full field: false on empty/garbage/overflow.
bool parseU32(const char *s, size_t len, uint32_t *out) {
  if (len == 0 || len > 10) return false;
  uint64_t v = 0;
  for (size_t i = 0; i < len; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10u + (uint64_t)(s[i] - '0');
  }
  if (v > 0xFFFFFFFFull) return false;
  *out = (uint32_t)v;
  return true;
}

// Splits line into comma-separated fields (pointers+lengths, no copy).
// Returns the field count (up to maxFields; more than that = malformed).
int splitFields(const char *line, const char **starts, size_t *lens,
                int maxFields) {
  int n = 0;
  const char *p = line;
  while (true) {
    if (n >= maxFields) return -1;
    const char *comma = strchr(p, ',');
    starts[n] = p;
    lens[n] = comma ? (size_t)(comma - p) : strlen(p);
    n++;
    if (!comma) break;
    p = comma + 1;
  }
  return n;
}

bool fieldEquals(const char *start, size_t len, const char *lit) {
  return strlen(lit) == len && strncmp(start, lit, len) == 0;
}

}  // namespace

BabyProtoMsgType baby_proto_parse(const char *line, BabyProtoMsg *out) {
  memset(out, 0, sizeof(*out));
  out->type = BABY_MSG_NONE;
  if (!line || strncmp(line, "HMI,", 4) != 0) return BABY_MSG_NONE;

  const char *f[8];
  size_t fl[8];
  int n = splitFields(line, f, fl, 8);
  if (n < 2) return BABY_MSG_NONE;

  const char *cmd = f[1];
  size_t cmdLen = fl[1];

  if (fieldEquals(cmd, cmdLen, "PROFILE_LIST_REQ")) {
    if (n != 2) return BABY_MSG_NONE;
    out->type = BABY_MSG_LIST_REQ;
    return out->type;
  }

  if (fieldEquals(cmd, cmdLen, "PROFILE_NEW")) {
    if (n != 4) return BABY_MSG_NONE;
    if (fl[2] == 0 || fl[2] >= BABY_NAME_LEN) return BABY_MSG_NONE;
    uint32_t gest;
    if (!parseU32(f[3], fl[3], &gest) || gest > 255) return BABY_MSG_NONE;
    memcpy(out->name, f[2], fl[2]);
    out->name[fl[2]] = '\0';
    out->gestWeeks = (uint8_t)gest;
    out->type = BABY_MSG_NEW;
    return out->type;
  }

  if (fieldEquals(cmd, cmdLen, "PROFILE_SELECT")) {
    if (n != 3 || !parseU32(f[2], fl[2], &out->seq)) return BABY_MSG_NONE;
    out->type = BABY_MSG_SELECT;
    return out->type;
  }

  if (fieldEquals(cmd, cmdLen, "PROFILE_WEIGHT")) {
    if (n != 4 || !parseU32(f[2], fl[2], &out->seq)) return BABY_MSG_NONE;
    if (fieldEquals(f[3], fl[3], "SKIP")) {
      out->grams = 0;
    } else {
      uint32_t g;
      if (!parseU32(f[3], fl[3], &g) || g == 0 || g > 65535) {
        return BABY_MSG_NONE;
      }
      out->grams = (uint16_t)g;
    }
    out->type = BABY_MSG_WEIGHT;
    return out->type;
  }

  if (fieldEquals(cmd, cmdLen, "PROFILE_AGE_MANUAL")) {
    uint32_t days;
    if (n != 4 || !parseU32(f[2], fl[2], &out->seq) ||
        !parseU32(f[3], fl[3], &days) || days > 65535) {
      return BABY_MSG_NONE;
    }
    out->ageDays = (uint16_t)days;
    out->type = BABY_MSG_AGE_MANUAL;
    return out->type;
  }

  if (fieldEquals(cmd, cmdLen, "PROFILE_DISCHARGE")) {
    uint32_t oc;
    if (n != 4 || !parseU32(f[2], fl[2], &out->seq) ||
        !parseU32(f[3], fl[3], &oc) || oc > BABY_OUTCOME_TRANSFERRED) {
      return BABY_MSG_NONE;
    }
    out->outcome = (uint8_t)oc;
    out->type = BABY_MSG_DISCHARGE;
    return out->type;
  }

  if (fieldEquals(cmd, cmdLen, "PROFILE_KANGAROO")) {
    if (n != 3 || !parseU32(f[2], fl[2], &out->seq)) return BABY_MSG_NONE;
    out->type = BABY_MSG_KANGAROO;
    return out->type;
  }

  if (fieldEquals(cmd, cmdLen, "PROFILE_HISTORY_REQ")) {
    if (n != 3 || !parseU32(f[2], fl[2], &out->page)) return BABY_MSG_NONE;
    out->type = BABY_MSG_HISTORY_REQ;
    return out->type;
  }

  if (fieldEquals(cmd, cmdLen, "WEIGHT_HISTORY_REQ")) {
    if (n != 3 || !parseU32(f[2], fl[2], &out->seq)) return BABY_MSG_NONE;
    out->type = BABY_MSG_WEIGHT_HISTORY_REQ;
    return out->type;
  }

  return BABY_MSG_NONE;
}

// ---------------- Builders ----------------

namespace {
// Bounded append; sets *overflow instead of writing past len.
void appendf(char *buf, size_t len, int *pos, bool *overflow, const char *fmt,
             ...) {
  if (*overflow || *pos < 0 || (size_t)*pos >= len) {
    *overflow = true;
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  int w = vsnprintf(buf + *pos, len - (size_t)*pos, fmt, ap);
  va_end(ap);
  if (w < 0 || (size_t)(*pos + w) >= len) {
    *overflow = true;
    return;
  }
  *pos += w;
}
}  // namespace

int baby_proto_build_list(char *buf, size_t len,
                          const BabyProfile slots[BABY_ACTIVE_SLOTS]) {
  int used = 0;
  for (int i = 0; i < BABY_ACTIVE_SLOTS; i++) {
    if (slots[i].slotUsed) used++;
  }
  int pos = 0;
  bool ovf = false;
  appendf(buf, len, &pos, &ovf, "CTRL,PROFILE_LIST,%d", used);
  for (int i = 0; i < BABY_ACTIVE_SLOTS; i++) {
    if (!slots[i].slotUsed) continue;
    appendf(buf, len, &pos, &ovf, ",%u,%s,%u,%u,%u,%u,%u",
            (unsigned)slots[i].seq, slots[i].name,
            (unsigned)slots[i].gestWeeks, (unsigned)slots[i].weightGrams,
            (unsigned)slots[i].kangarooCount,
            (unsigned)slots[i].phototherapyMinutes,
            (unsigned)slots[i].thermoMinutes);
  }
  appendf(buf, len, &pos, &ovf, "\n");
  return ovf ? 0 : pos;
}

int baby_proto_build_range(char *buf, size_t len, uint32_t seq, bool ageKnown,
                           uint16_t ageDays, const NteRange *range) {
  int pos = 0;
  bool ovf = false;
  appendf(buf, len, &pos, &ovf,
          "CTRL,PROFILE_RANGE,%u,%d,%u,%.1f,%.1f,%.1f,%d\n", (unsigned)seq,
          ageKnown ? 1 : 0, (unsigned)ageDays, (double)range->lo,
          (double)range->hi, (double)range->mid, range->estimated ? 1 : 0);
  return ovf ? 0 : pos;
}

int baby_proto_build_history(char *buf, size_t len, uint32_t page,
                             uint32_t totalCount, const BabyProfile *records,
                             uint32_t n) {
  int pos = 0;
  bool ovf = false;
  appendf(buf, len, &pos, &ovf, "CTRL,PROFILE_HISTORY,%u,%u,%u",
          (unsigned)page, (unsigned)totalCount, (unsigned)n);
  for (uint32_t i = 0; i < n; i++) {
    appendf(buf, len, &pos, &ovf, ",%u,%s,%u,%u,%u,%u,%u,%u,%u,%u",
            (unsigned)records[i].seq, records[i].name,
            (unsigned)records[i].gestWeeks, (unsigned)records[i].weightGrams,
            (unsigned)records[i].admissionEpoch,
            (unsigned)records[i].dischargeEpoch,
            (unsigned)records[i].outcome,
            (unsigned)records[i].kangarooCount,
            (unsigned)records[i].phototherapyMinutes,
            (unsigned)records[i].thermoMinutes);
  }
  appendf(buf, len, &pos, &ovf, "\n");
  return ovf ? 0 : pos;
}

int baby_proto_build_weight_history(char *buf, size_t len, uint32_t seq,
                                    const BabyWeightPoint *points,
                                    uint32_t n) {
  int pos = 0;
  bool ovf = false;
  appendf(buf, len, &pos, &ovf, "CTRL,WEIGHT_HISTORY,%u,%u", (unsigned)seq,
          (unsigned)n);
  for (uint32_t i = 0; i < n; i++) {
    appendf(buf, len, &pos, &ovf, ",%u,%u", (unsigned)points[i].dayOffset,
            (unsigned)points[i].weightGrams);
  }
  appendf(buf, len, &pos, &ovf, "\n");
  return ovf ? 0 : pos;
}
