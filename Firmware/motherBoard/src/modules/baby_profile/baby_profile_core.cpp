#include "baby_profile_core.h"

#include <string.h>

// ---------------- Active slots ----------------

int baby_find_free_slot(const BabyProfile slots[BABY_ACTIVE_SLOTS]) {
  for (int i = 0; i < BABY_ACTIVE_SLOTS; i++) {
    if (!slots[i].slotUsed) return i;
  }
  return -1;
}

int baby_find_slot_by_seq(const BabyProfile slots[BABY_ACTIVE_SLOTS],
                          uint32_t seq) {
  for (int i = 0; i < BABY_ACTIVE_SLOTS; i++) {
    if (slots[i].slotUsed && slots[i].seq == seq) return i;
  }
  return -1;
}

int baby_pick_eviction_slot(const BabyProfile slots[BABY_ACTIVE_SLOTS],
                            uint32_t activeSeq) {
  int best = -1;
  for (int i = 0; i < BABY_ACTIVE_SLOTS; i++) {
    if (!slots[i].slotUsed) continue;
    if (activeSeq != 0 && slots[i].seq == activeSeq) continue;
    if (best < 0 || slots[i].seq < slots[best].seq) best = i;
  }
  return best;
}

// ---------------- Age derivation ----------------

bool baby_derive_age_days(uint32_t admissionEpoch, uint32_t nowEpoch,
                          bool timeSynced, uint16_t *ageDays) {
  if (!timeSynced || admissionEpoch == 0 || nowEpoch < admissionEpoch) {
    return false;
  }
  uint32_t days = (nowEpoch - admissionEpoch) / 86400u;
  if (days > 0xFFFFu) days = 0xFFFFu;
  *ageDays = (uint16_t)days;
  return true;
}

// ---------------- Audit history record ----------------
// Layout (little-endian): valid(1) seq(4) name(24) gestWeeks(1)
// weightGrams(2) admissionEpoch(4) dischargeEpoch(4) outcome(1)
// kangarooCount(2) lastKangarooEpoch(4) phototherapyMinutes(4)
// thermoMinutes(4) humidityMinutes(4) = 59 bytes.

static void put_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}
static uint32_t get_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static void put_u16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}
static uint16_t get_u16(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void baby_history_encode(const BabyProfile *p, bool valid,
                         uint8_t out[BABY_HISTORY_RECORD_SIZE]) {
  out[0] = valid ? 1 : 0;
  put_u32(out + 1, p->seq);
  memcpy(out + 5, p->name, BABY_NAME_LEN);
  out[5 + BABY_NAME_LEN - 1] = '\0';  // record name is always terminated
  out[29] = p->gestWeeks;
  put_u16(out + 30, p->weightGrams);
  put_u32(out + 32, p->admissionEpoch);
  put_u32(out + 36, p->dischargeEpoch);
  out[40] = p->outcome;
  put_u16(out + 41, p->kangarooCount);
  put_u32(out + 43, p->lastKangarooEpoch);
  put_u32(out + 47, p->phototherapyMinutes);
  put_u32(out + 51, p->thermoMinutes);
  put_u32(out + 55, p->humidityMinutes);
  out[59] = p->cause;
}

bool baby_history_decode(const uint8_t rec[BABY_HISTORY_RECORD_SIZE],
                         BabyProfile *out) {
  out->slotUsed = true;
  out->seq = get_u32(rec + 1);
  memcpy(out->name, rec + 5, BABY_NAME_LEN);
  out->name[BABY_NAME_LEN - 1] = '\0';
  out->gestWeeks = rec[29];
  out->weightGrams = get_u16(rec + 30);
  out->admissionEpoch = get_u32(rec + 32);
  out->dischargeEpoch = get_u32(rec + 36);
  out->outcome = rec[40];
  out->kangarooCount = get_u16(rec + 41);
  out->lastKangarooEpoch = get_u32(rec + 43);
  out->phototherapyMinutes = get_u32(rec + 47);
  out->thermoMinutes = get_u32(rec + 51);
  out->humidityMinutes = get_u32(rec + 55);
  out->cause = rec[59];
  return rec[0] != 0;
}

// ---------------- Circular cursor ----------------

uint32_t circ_advance(CircularCursor *c, uint32_t cap) {
  uint32_t pos = c->writeIndex;
  c->writeIndex = (c->writeIndex + 1u) % cap;
  if (c->count < cap) c->count++;
  return pos;
}

uint32_t circ_oldest(const CircularCursor *c, uint32_t cap) {
  if (c->count < cap) return 0;  // never wrapped: oldest is slot 0
  return c->writeIndex;          // wrapped: next overwrite target is oldest
}

// ---------------- Weight-point dedup ----------------

bool baby_weight_should_append(bool hasLastPoint, uint16_t lastWeightGrams,
                               uint16_t newWeightGrams) {
  return !hasLastPoint || lastWeightGrams != newWeightGrams;
}

void baby_weight_point_encode(uint32_t timestamp, uint16_t weightGrams,
                              uint8_t out[BABY_WEIGHT_POINT_SIZE]) {
  put_u32(out, timestamp);
  put_u16(out + 4, weightGrams);
}

void baby_weight_point_decode(const uint8_t in[BABY_WEIGHT_POINT_SIZE],
                              uint32_t *timestamp, uint16_t *weightGrams) {
  *timestamp = get_u32(in);
  *weightGrams = get_u16(in + 4);
}

// ---------------- Archive byte budget ----------------

uint32_t baby_archive_total_bytes(const ArchiveEntry *entries, int n) {
  uint32_t total = 0;
  for (int i = 0; i < n; i++) total += entries[i].bytes;
  return total;
}

int baby_archive_pick_eviction(const ArchiveEntry *entries, int n) {
  int best = -1;
  for (int i = 0; i < n; i++) {
    if (best < 0 || entries[i].seq < entries[best].seq) best = i;
  }
  return best;
}

bool baby_archive_over_budget(uint32_t totalBytes, uint32_t incomingBytes,
                              uint32_t budgetBytes) {
  return totalBytes + incomingBytes > budgetBytes;
}

// ---------------- Unified eviction decision ----------------

BabyEvictionTrigger baby_unified_eviction_check(uint32_t auditCount,
                                                uint32_t auditCap,
                                                uint32_t archiveTotalBytes,
                                                uint32_t incomingArchiveBytes,
                                                uint32_t budgetBytes) {
  // Byte pressure is resolved first: freeing archive space also tombstones
  // the paired audit record, whereas the audit-cap path frees no bytes.
  if (baby_archive_over_budget(archiveTotalBytes, incomingArchiveBytes,
                               budgetBytes)) {
    return BABY_EVICT_WEIGHT_BUDGET;
  }
  if (auditCount >= auditCap) {
    return BABY_EVICT_AUDIT_CAP;
  }
  return BABY_EVICT_NONE;
}

// ---------------- Weight-history downsampling ----------------

uint32_t baby_downsample_indices(uint32_t count, uint32_t maxOut,
                                 uint32_t *outIndices) {
  if (count == 0 || maxOut == 0) return 0;
  if (count <= maxOut) {
    for (uint32_t i = 0; i < count; i++) outIndices[i] = i;
    return count;
  }
  // idx(i) = round(i * (count-1) / (maxOut-1)), integer arithmetic.
  for (uint32_t i = 0; i < maxOut; i++) {
    uint64_t num = (uint64_t)i * (count - 1u);
    outIndices[i] = (uint32_t)((num + (maxOut - 1u) / 2u) / (maxOut - 1u));
  }
  return maxOut;
}
