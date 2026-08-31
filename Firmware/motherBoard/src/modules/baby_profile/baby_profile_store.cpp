#include "baby_profile_store.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp32-hal-log.h"

static const char *TAG = "BABY";

// NVS namespace/keys (see include/config/preferences_keys.h conventions;
// kept local: this module is the only reader/writer).
static const char NS_BABY[] = "mb_baby";
static const char KEY_NEXT_SEQ[] = "nextSeq";
static const char KEY_ACTIVE_SEQ[] = "activeSeq";
// Per-slot blobs: slot0/slot1/slot2.
static const char *SLOT_KEYS[BABY_ACTIVE_SLOTS] = {"slot0", "slot1", "slot2"};

static const char HISTORY_PATH[] = "/baby_history.log";
static const char WEIGHT_ACTIVE_DIR[] = "/weight_active";
static const char WEIGHT_ARCHIVE_DIR[] = "/weight_archive";
// 8-byte header for both the audit log and each weight file.
struct FileHeader {
  uint32_t writeIndex;
  uint32_t count;
};

static BabyProfile s_slots[BABY_ACTIVE_SLOTS];
static uint32_t s_nextSeq = 1;
static uint32_t s_activeSeq = 0;
static bool s_telemetryDirty = false;
// Discharge publishes the *final* record even though the slot is freed
// immediately — snapshot it so telemetry still has the data to send.
static BabyProfile s_telemetryProfile;
static bool s_telemetryHasSnapshot = false;
// Archive byte index, rebuilt from a directory scan at init.
static ArchiveEntry s_archive[128];
static int s_archiveCount = 0;

// ---------------- Cloud event queue ----------------
// Small ring: the transports drain it every publish cycle, so it only has to
// absorb a burst while offline. Oldest is dropped if it ever fills, with a
// warning - losing the oldest beats blocking the control task.
static const int CLOUD_QUEUE_CAP = 8;
static BabyCloudEvent s_cloudQueue[CLOUD_QUEUE_CAP];
static int s_cloudHead = 0;   // next to read
static int s_cloudCount = 0;
static bool s_attributesDirty = true;  // publish once at boot

static void cloudPush(uint8_t type, const BabyProfile *p, uint16_t grams) {
  int tail = (s_cloudHead + s_cloudCount) % CLOUD_QUEUE_CAP;
  if (s_cloudCount == CLOUD_QUEUE_CAP) {
    s_cloudHead = (s_cloudHead + 1) % CLOUD_QUEUE_CAP;  // drop oldest
    ESP_LOGW(TAG, "cloud queue full, dropped oldest event");
  } else {
    s_cloudCount++;
  }
  BabyCloudEvent &e = s_cloudQueue[tail];
  e.type = type;
  e.ts = babyStore_nowEpoch();
  e.weightGrams = grams;
  e.profile = *p;
}

bool babyStore_peekCloudEvent(BabyCloudEvent *out) {
  if (s_cloudCount == 0) return false;
  *out = s_cloudQueue[s_cloudHead];
  return true;
}

void babyStore_popCloudEvent() {
  if (s_cloudCount == 0) return;
  s_cloudHead = (s_cloudHead + 1) % CLOUD_QUEUE_CAP;
  s_cloudCount--;
}

// The baby the incubator is currently treating: the active-control profile
// if there is one, else the most recently created slot. nullptr when empty.
const BabyProfile *babyStore_currentOccupant() {
  if (s_activeSeq != 0) {
    int i = baby_find_slot_by_seq(s_slots, s_activeSeq);
    if (i >= 0) return &s_slots[i];
  }
  const BabyProfile *newest = nullptr;
  for (int i = 0; i < BABY_ACTIVE_SLOTS; i++) {
    if (s_slots[i].slotUsed && (!newest || s_slots[i].seq > newest->seq)) {
      newest = &s_slots[i];
    }
  }
  return newest;
}

bool babyStore_attributesDirty() { return s_attributesDirty; }
void babyStore_clearAttributesDirty() { s_attributesDirty = false; }

// ---------------- Time ----------------

uint32_t babyStore_nowEpoch() {
  time_t now = time(nullptr);
  // Same "synced" threshold DriveUpload uses (2021-01-01).
  if (now < 1609459200) return 0;
  return (uint32_t)now;
}

// ---------------- NVS helpers ----------------

static void persistSlot(int i) {
  Preferences p;
  p.begin(NS_BABY, false);
  if (s_slots[i].slotUsed) {
    p.putBytes(SLOT_KEYS[i], &s_slots[i], sizeof(BabyProfile));
  } else {
    p.remove(SLOT_KEYS[i]);
  }
  p.end();
}

static void persistNextSeq() {
  Preferences p;
  p.begin(NS_BABY, false);
  p.putULong(KEY_NEXT_SEQ, s_nextSeq);
  p.end();
}

// ---------------- File helpers ----------------

static bool readHeader(File &f, FileHeader *h) {
  if (!f.seek(0)) return false;
  return f.read((uint8_t *)h, sizeof(FileHeader)) == sizeof(FileHeader);
}

static void writeHeader(File &f, const FileHeader &h) {
  f.seek(0);
  f.write((const uint8_t *)&h, sizeof(FileHeader));
}

static String weightActivePath(uint32_t seq) {
  return String(WEIGHT_ACTIVE_DIR) + "/" + String(seq) + ".bin";
}
static String weightArchivePath(uint32_t seq) {
  return String(WEIGHT_ARCHIVE_DIR) + "/" + String(seq) + ".bin";
}

// ---------------- Archive byte index ----------------

static void archiveIndexRebuild() {
  s_archiveCount = 0;
  File dir = LittleFS.open(WEIGHT_ARCHIVE_DIR);
  if (!dir || !dir.isDirectory()) return;
  File f = dir.openNextFile();
  while (f && s_archiveCount < (int)(sizeof(s_archive) / sizeof(s_archive[0]))) {
    uint32_t seq = (uint32_t)strtoul(f.name(), nullptr, 10);
    if (seq != 0) {
      s_archive[s_archiveCount].seq = seq;
      s_archive[s_archiveCount].bytes = f.size();
      s_archiveCount++;
    }
    f = dir.openNextFile();
  }
}

static void archiveIndexRemove(uint32_t seq) {
  for (int i = 0; i < s_archiveCount; i++) {
    if (s_archive[i].seq == seq) {
      s_archive[i] = s_archive[s_archiveCount - 1];
      s_archiveCount--;
      return;
    }
  }
}

// ---------------- Audit log ----------------

static bool historyOpen(File &f) {
  if (LittleFS.exists(HISTORY_PATH)) {
    f = LittleFS.open(HISTORY_PATH, "r+");
    return (bool)f;
  }
  f = LittleFS.open(HISTORY_PATH, "w+", true);
  if (!f) return false;
  FileHeader h = {0, 0};
  writeHeader(f, h);
  return true;
}

static void historyTombstone(uint32_t seq) {
  File f;
  if (!historyOpen(f)) return;
  FileHeader h;
  if (!readHeader(f, &h)) {
    f.close();
    return;
  }
  uint8_t rec[BABY_HISTORY_RECORD_SIZE];
  for (uint32_t i = 0; i < h.count; i++) {
    size_t pos = sizeof(FileHeader) + i * BABY_HISTORY_RECORD_SIZE;
    f.seek(pos);
    if (f.read(rec, sizeof(rec)) != sizeof(rec)) break;
    BabyProfile p;
    if (baby_history_decode(rec, &p) && p.seq == seq) {
      rec[0] = 0;  // valid=false
      f.seek(pos);
      f.write(rec, sizeof(rec));
      break;
    }
  }
  f.close();
}

// Evicts the oldest archived baby: deletes its weight file and tombstones
// its audit record (design decision 8, budget-first trigger).
static void evictOldestArchived() {
  int idx = baby_archive_pick_eviction(s_archive, s_archiveCount);
  if (idx < 0) return;
  uint32_t seq = s_archive[idx].seq;
  LittleFS.remove(weightArchivePath(seq));
  archiveIndexRemove(seq);
  historyTombstone(seq);
  ESP_LOGI(TAG, "archive budget eviction: seq=%u", (unsigned)seq);
}

// Appends the audit record; when the log is full, the circular overwrite of
// the oldest record also deletes that record's archived weight file
// (design decision 8, audit-cap trigger).
static void historyAppend(const BabyProfile *p) {
  File f;
  if (!historyOpen(f)) {
    ESP_LOGE(TAG, "history open failed");
    return;
  }
  FileHeader h;
  if (!readHeader(f, &h)) {
    h = {0, 0};
  }
  CircularCursor c = {h.writeIndex, h.count};
  if (c.count >= BABY_HISTORY_CAP) {
    // About to overwrite the oldest record: pair-delete its archive file.
    uint32_t oldestPos = circ_oldest(&c, BABY_HISTORY_CAP);
    uint8_t oldRec[BABY_HISTORY_RECORD_SIZE];
    f.seek(sizeof(FileHeader) + oldestPos * BABY_HISTORY_RECORD_SIZE);
    if (f.read(oldRec, sizeof(oldRec)) == sizeof(oldRec)) {
      BabyProfile old;
      if (baby_history_decode(oldRec, &old)) {
        LittleFS.remove(weightArchivePath(old.seq));
        archiveIndexRemove(old.seq);
      }
    }
  }
  uint32_t pos = circ_advance(&c, BABY_HISTORY_CAP);
  uint8_t rec[BABY_HISTORY_RECORD_SIZE];
  baby_history_encode(p, true, rec);
  f.seek(sizeof(FileHeader) + pos * BABY_HISTORY_RECORD_SIZE);
  f.write(rec, sizeof(rec));
  h.writeIndex = c.writeIndex;
  h.count = c.count;
  writeHeader(f, h);
  f.close();
}

static int wipeDir(const char *dir);  // defined with the wipe-all helpers

// The audit log stores fixed-size records with no version field, so a
// firmware that changed BABY_HISTORY_RECORD_SIZE would read a log written by
// the previous one at the wrong stride and decode garbage into what is a
// clinical record. Refuse to do that: a log whose size does not match its own
// header count times the current record size is from another layout, so drop
// it (with its weight archives, which are only reachable through it) and
// start a clean one.
//
// Writes are sequential until the cap saturates, so the expected size is
// always header + min(count, cap) * recordSize for a log this firmware wrote.
static void historyResetIfIncompatible() {
  if (!LittleFS.exists(HISTORY_PATH)) return;
  File f = LittleFS.open(HISTORY_PATH, "r");
  if (!f) return;
  size_t actual = f.size();
  FileHeader h = {0, 0};  // a header too short to read stays all-zero
  bool headerOk = readHeader(f, &h);
  f.close();

  uint32_t stored = (h.count > BABY_HISTORY_CAP) ? BABY_HISTORY_CAP : h.count;
  size_t expected = sizeof(FileHeader) + (size_t)stored * BABY_HISTORY_RECORD_SIZE;
  if (headerOk && h.count <= BABY_HISTORY_CAP && actual == expected) return;

  size_t legacy =
      sizeof(FileHeader) + (size_t)stored * BABY_HISTORY_RECORD_SIZE_LEGACY_V1;
  ESP_LOGW(TAG,
           "audit log layout mismatch (size=%u, expected=%u, legacy=%u) — "
           "resetting baby history",
           (unsigned)actual, (unsigned)expected, (unsigned)legacy);
  LittleFS.remove(HISTORY_PATH);
  int removed = wipeDir(WEIGHT_ARCHIVE_DIR);
  if (removed > 0) {
    ESP_LOGW(TAG, "dropped %d orphaned weight archive(s)", removed);
  }
}

// ---------------- Weight files ----------------

static uint32_t weightFileSize(uint32_t count) {
  return sizeof(FileHeader) + count * BABY_WEIGHT_POINT_SIZE;
}

static void weightAppend(uint32_t seq, uint32_t timestamp, uint16_t grams) {
  String path = weightActivePath(seq);
  File f;
  if (LittleFS.exists(path)) {
    f = LittleFS.open(path, "r+");
  } else {
    LittleFS.mkdir(WEIGHT_ACTIVE_DIR);
    f = LittleFS.open(path, "w+", true);
    if (f) {
      FileHeader h = {0, 0};
      writeHeader(f, h);
    }
  }
  if (!f) {
    ESP_LOGE(TAG, "weight file open failed seq=%u", (unsigned)seq);
    return;
  }
  FileHeader h;
  if (!readHeader(f, &h)) h = {0, 0};

  // Dedup: compare against the most recently stored point.
  if (h.count > 0) {
    uint32_t lastPos = (h.writeIndex + BABY_WEIGHT_POINT_CAP - 1) %
                       BABY_WEIGHT_POINT_CAP;
    uint8_t buf[BABY_WEIGHT_POINT_SIZE];
    f.seek(sizeof(FileHeader) + lastPos * BABY_WEIGHT_POINT_SIZE);
    if (f.read(buf, sizeof(buf)) == sizeof(buf)) {
      uint32_t ts;
      uint16_t lastGrams;
      baby_weight_point_decode(buf, &ts, &lastGrams);
      if (!baby_weight_should_append(true, lastGrams, grams)) {
        f.close();
        return;
      }
    }
  }

  CircularCursor c = {h.writeIndex, h.count};
  uint32_t pos = circ_advance(&c, BABY_WEIGHT_POINT_CAP);
  uint8_t buf[BABY_WEIGHT_POINT_SIZE];
  baby_weight_point_encode(timestamp, grams, buf);
  f.seek(sizeof(FileHeader) + pos * BABY_WEIGHT_POINT_SIZE);
  f.write(buf, sizeof(buf));
  h.writeIndex = c.writeIndex;
  h.count = c.count;
  writeHeader(f, h);
  f.close();
}

// ---------------- archiveProfile (design decision 9) ----------------
// Single routine, two entry points (FIFO evict / explicit discharge).

static void archiveProfile(int slotIdx) {
  BabyProfile *p = &s_slots[slotIdx];
  uint32_t seq = p->seq;

  // Size of the weight file about to enter the archive (0 if none).
  uint32_t incomingBytes = 0;
  String activePath = weightActivePath(seq);
  bool hasWeightFile = LittleFS.exists(activePath);
  if (hasWeightFile) {
    File f = LittleFS.open(activePath, "r");
    if (f) {
      incomingBytes = f.size();
      f.close();
    }
  }

  // Unified retention: free space (budget trigger) before archiving; the
  // audit-cap trigger is handled inside historyAppend's overwrite path.
  while (baby_unified_eviction_check(
             0 /*audit handled on append*/, BABY_HISTORY_CAP,
             baby_archive_total_bytes(s_archive, s_archiveCount),
             incomingBytes, BABY_WEIGHT_ARCHIVE_BUDGET_BYTES) ==
         BABY_EVICT_WEIGHT_BUDGET) {
    int before = s_archiveCount;
    evictOldestArchived();
    if (s_archiveCount == before) break;  // nothing left to free
  }

  historyAppend(p);

  if (hasWeightFile) {
    LittleFS.mkdir(WEIGHT_ARCHIVE_DIR);
    String archPath = weightArchivePath(seq);
    LittleFS.remove(archPath);  // stale leftover from an unclean shutdown
    if (LittleFS.rename(activePath, archPath) &&
        s_archiveCount < (int)(sizeof(s_archive) / sizeof(s_archive[0]))) {
      s_archive[s_archiveCount].seq = seq;
      s_archive[s_archiveCount].bytes = incomingBytes;
      s_archiveCount++;
    }
  }

  p->slotUsed = false;
  persistSlot(slotIdx);
  if (s_activeSeq == seq) {
    babyStore_setActiveSeq(0);
  }
  ESP_LOGI(TAG, "archived profile seq=%u", (unsigned)seq);
}

// ---------------- Public API ----------------

void babyStore_init() {
  // Idempotent mount: CommTask init can run before the DriveUpload task
  // (the usual mount owner) has called LittleFS.begin().
  if (!LittleFS.begin(true)) {
    ESP_LOGE(TAG, "LittleFS mount failed — history/weight disabled");
  }
  memset(s_slots, 0, sizeof(s_slots));
  Preferences p;
  p.begin(NS_BABY, true);
  s_nextSeq = p.getULong(KEY_NEXT_SEQ, 1);
  s_activeSeq = p.getULong(KEY_ACTIVE_SEQ, 0);
  for (int i = 0; i < BABY_ACTIVE_SLOTS; i++) {
    // Slots are raw struct blobs, so a firmware that changed BabyProfile's
    // layout finds a size mismatch here and starts the slot empty rather than
    // reinterpreting the old bytes. Logged, because losing an active profile
    // is visible to the ward and must not look like a glitch.
    size_t got = p.getBytes(SLOT_KEYS[i], &s_slots[i], sizeof(BabyProfile));
    if (got != sizeof(BabyProfile)) {
      if (got != 0) {
        ESP_LOGW(TAG,
                 "slot%d blob is %u bytes, expected %u (layout changed) — "
                 "slot reset",
                 i, (unsigned)got, (unsigned)sizeof(BabyProfile));
      }
      memset(&s_slots[i], 0, sizeof(BabyProfile));
    }
  }
  p.end();
  historyResetIfIncompatible();
  archiveIndexRebuild();
  ESP_LOGI(TAG, "init: nextSeq=%u activeSeq=%u archived=%d",
           (unsigned)s_nextSeq, (unsigned)s_activeSeq, s_archiveCount);
}

void babyStore_getSlots(BabyProfile out[BABY_ACTIVE_SLOTS]) {
  memcpy(out, s_slots, sizeof(s_slots));
}

const BabyProfile *babyStore_findBySeq(uint32_t seq) {
  int i = baby_find_slot_by_seq(s_slots, seq);
  return (i >= 0) ? &s_slots[i] : nullptr;
}

const BabyProfile *babyStore_telemetryProfile() {
  if (s_activeSeq != 0) {
    const BabyProfile *p = babyStore_findBySeq(s_activeSeq);
    if (p) return p;
  }
  const BabyProfile *newest = nullptr;
  for (int i = 0; i < BABY_ACTIVE_SLOTS; i++) {
    if (s_slots[i].slotUsed &&
        (!newest || s_slots[i].seq > newest->seq)) {
      newest = &s_slots[i];
    }
  }
  if (newest) return newest;
  return s_telemetryHasSnapshot ? &s_telemetryProfile : nullptr;
}

bool babyStore_telemetryDirty() { return s_telemetryDirty; }
void babyStore_clearTelemetryDirty() { s_telemetryDirty = false; }

uint32_t babyStore_createProfile(const char *name, uint8_t gestWeeks) {
  int slot = baby_find_free_slot(s_slots);
  if (slot < 0) {
    slot = baby_pick_eviction_slot(s_slots, s_activeSeq);
    if (slot < 0) {
      ESP_LOGE(TAG, "profile creation refused: no eligible slot");
      return 0;
    }
    archiveProfile(slot);
  }

  BabyProfile *p = &s_slots[slot];
  memset(p, 0, sizeof(BabyProfile));
  // nextSeq is persisted BEFORE the slot is marked used (design decision 3):
  // a reboot in between loses only the in-progress creation, never reuses a
  // seq for an already-committed profile.
  uint32_t seq = s_nextSeq++;
  persistNextSeq();

  p->seq = seq;
  size_t j = 0;
  for (size_t i = 0; name && name[i] && j < BABY_NAME_LEN - 1; i++) {
    if (name[i] == ',') continue;  // defensive: protocol is comma-delimited
    p->name[j++] = name[i];
  }
  p->name[j] = '\0';
  p->gestWeeks = gestWeeks;
  p->weightGrams = 0;
  p->admissionEpoch = babyStore_nowEpoch();
  p->dischargeEpoch = 0;
  p->outcome = BABY_OUTCOME_UNKNOWN;
  p->slotUsed = true;
  persistSlot(slot);
  s_attributesDirty = true;
  s_telemetryDirty = true;
  ESP_LOGI(TAG, "created profile seq=%u name='%s' gest=%u", (unsigned)seq,
           p->name, (unsigned)gestWeeks);
  return seq;
}

bool babyStore_recordWeight(uint32_t seq, uint16_t grams) {
  int i = baby_find_slot_by_seq(s_slots, seq);
  if (i < 0) return false;
  if (grams == 0) return true;  // SKIP: nothing persisted
  s_slots[i].weightGrams = grams;
  persistSlot(i);
  weightAppend(seq, babyStore_nowEpoch(), grams);
  cloudPush(BABY_EVT_WEIGHT, &s_slots[i], grams);
  s_attributesDirty = true;
  s_telemetryDirty = true;
  return true;
}

bool babyStore_recordKangaroo(uint32_t seq) {
  int i = baby_find_slot_by_seq(s_slots, seq);
  if (i < 0) return false;
  if (s_slots[i].kangarooCount < 0xFFFFu) s_slots[i].kangarooCount++;
  // 0 when the clock is not synced — the count still increments, so the
  // event is never silently lost just because there is no date for it.
  s_slots[i].lastKangarooEpoch = babyStore_nowEpoch();
  persistSlot(i);
  s_telemetryDirty = true;
  cloudPush(BABY_EVT_KANGAROO, &s_slots[i], 0);
  s_attributesDirty = true;
  ESP_LOGI(TAG, "kangaroo seq=%u count=%u", (unsigned)seq,
           (unsigned)s_slots[i].kangarooCount);
  return true;
}

// Shared by both therapy counters: saturating add + persist, so neither can
// wrap around to zero after a very long stay.
static bool addTherapyMinutes(uint32_t seq, uint32_t minutes,
                              uint32_t BabyProfile::*field,
                              const char *what) {
  if (minutes == 0) return true;
  int i = baby_find_slot_by_seq(s_slots, seq);
  if (i < 0) return false;
  uint32_t cur = s_slots[i].*field;
  uint32_t total = cur + minutes;
  if (total < cur) total = 0xFFFFFFFFu;
  s_slots[i].*field = total;
  persistSlot(i);
  s_attributesDirty = true;
  s_telemetryDirty = true;
  ESP_LOGI(TAG, "%s seq=%u +%u min (total %u)", what, (unsigned)seq,
           (unsigned)minutes, (unsigned)total);
  return true;
}

bool babyStore_addPhototherapyMinutes(uint32_t seq, uint32_t minutes) {
  return addTherapyMinutes(seq, minutes, &BabyProfile::phototherapyMinutes,
                           "phototherapy");
}

bool babyStore_addThermoMinutes(uint32_t seq, uint32_t minutes) {
  return addTherapyMinutes(seq, minutes, &BabyProfile::thermoMinutes,
                           "thermoregulation");
}

bool babyStore_addHumidityMinutes(uint32_t seq, uint32_t minutes) {
  return addTherapyMinutes(seq, minutes, &BabyProfile::humidityMinutes,
                           "humidity");
}

bool babyStore_discharge(uint32_t seq, uint8_t outcome) {
  int i = baby_find_slot_by_seq(s_slots, seq);
  if (i < 0) return false;
  s_slots[i].dischargeEpoch = babyStore_nowEpoch();
  s_slots[i].outcome = outcome;
  // Snapshot for telemetry before the slot is freed by archiveProfile().
  s_telemetryProfile = s_slots[i];
  s_telemetryHasSnapshot = true;
  // Queue the end-of-stay record before archiveProfile() frees the slot:
  // this single event is the one row per baby the history table reads.
  cloudPush(BABY_EVT_DISCHARGE, &s_slots[i], s_slots[i].weightGrams);
  archiveProfile(i);
  s_attributesDirty = true;
  s_telemetryDirty = true;
  return true;
}

// Removes every regular file in dir (LittleFS.rmdir only works on empty
// directories, and we want the directory itself to survive for reuse).
static int wipeDir(const char *dir) {
  int removed = 0;
  File d = LittleFS.open(dir);
  if (!d || !d.isDirectory()) return 0;
  // Collect first, delete after: deleting while the directory handle walks it
  // is exactly the kind of open-fd juggling esp_littlefs is documented to
  // mishandle (docs/known_issues.md).
  String names[64];
  int n = 0;
  File f = d.openNextFile();
  while (f && n < 64) {
    names[n++] = String(dir) + "/" + f.name();
    f = d.openNextFile();
  }
  d.close();
  for (int i = 0; i < n; i++) {
    if (LittleFS.remove(names[i])) removed++;
  }
  return removed;
}

int babyStore_wipeAll() {
  int removed = 0;
  if (LittleFS.remove(HISTORY_PATH)) removed++;
  removed += wipeDir(WEIGHT_ACTIVE_DIR);
  removed += wipeDir(WEIGHT_ARCHIVE_DIR);

  Preferences p;
  p.begin(NS_BABY, false);
  p.clear();
  p.end();

  memset(s_slots, 0, sizeof(s_slots));
  s_nextSeq = 1;
  s_activeSeq = 0;
  s_archiveCount = 0;
  s_telemetryHasSnapshot = false;
  s_telemetryDirty = false;
  s_cloudHead = 0;
  s_cloudCount = 0;
  s_attributesDirty = true;
  ESP_LOGW(TAG, "ALL baby data wiped (%d files removed)", removed);
  return removed;
}

uint32_t babyStore_getActiveSeq() { return s_activeSeq; }

void babyStore_setActiveSeq(uint32_t seq) {
  if (s_activeSeq != seq) s_attributesDirty = true;
  s_activeSeq = seq;
  Preferences p;
  p.begin(NS_BABY, false);
  p.putULong(KEY_ACTIVE_SEQ, seq);
  p.end();
}

bool babyStore_deriveAgeDays(uint32_t seq, uint16_t *ageDays) {
  const BabyProfile *p = babyStore_findBySeq(seq);
  if (!p) return false;
  uint32_t now = babyStore_nowEpoch();
  return baby_derive_age_days(p->admissionEpoch, now, now != 0, ageDays);
}

uint32_t babyStore_readHistoryPage(uint32_t page, uint32_t pageSize,
                                   BabyProfile *out, uint32_t *totalCount) {
  *totalCount = 0;
  File f;
  if (!LittleFS.exists(HISTORY_PATH)) return 0;
  f = LittleFS.open(HISTORY_PATH, "r");
  if (!f) return 0;
  FileHeader h;
  if (!readHeader(f, &h)) {
    f.close();
    return 0;
  }

  // Walk newest -> oldest (design decision 12: most-recently-archived
  // first), skipping tombstones. Two passes: count, then page-slice.
  CircularCursor c = {h.writeIndex, h.count};
  uint8_t rec[BABY_HISTORY_RECORD_SIZE];

  uint32_t valid = 0;
  for (uint32_t k = 0; k < h.count; k++) {
    // k-th newest record position.
    uint32_t pos = (h.writeIndex + BABY_HISTORY_CAP - 1 - k) %
                   BABY_HISTORY_CAP;
    if (h.count < BABY_HISTORY_CAP && pos >= h.count) continue;
    f.seek(sizeof(FileHeader) + pos * BABY_HISTORY_RECORD_SIZE);
    if (f.read(rec, sizeof(rec)) != sizeof(rec)) continue;
    if (rec[0] != 0) valid++;
  }
  *totalCount = valid;

  uint32_t written = 0;
  uint32_t validSeen = 0;
  uint32_t startRank = page * pageSize;
  for (uint32_t k = 0; k < h.count && written < pageSize; k++) {
    uint32_t pos = (h.writeIndex + BABY_HISTORY_CAP - 1 - k) %
                   BABY_HISTORY_CAP;
    if (h.count < BABY_HISTORY_CAP && pos >= h.count) continue;
    f.seek(sizeof(FileHeader) + pos * BABY_HISTORY_RECORD_SIZE);
    if (f.read(rec, sizeof(rec)) != sizeof(rec)) continue;
    BabyProfile p;
    if (!baby_history_decode(rec, &p)) continue;
    if (validSeen++ < startRank) continue;
    out[written++] = p;
  }
  f.close();
  return written;
}

uint32_t babyStore_readWeightHistory(uint32_t seq, BabyWeightPoint *out,
                                     uint32_t maxOut) {
  String path = weightActivePath(seq);
  if (!LittleFS.exists(path)) {
    path = weightArchivePath(seq);
    if (!LittleFS.exists(path)) return 0;
  }
  File f = LittleFS.open(path, "r");
  if (!f) return 0;
  FileHeader h;
  if (!readHeader(f, &h)) {
    f.close();
    return 0;
  }
  if (h.count == 0) {
    f.close();
    return 0;
  }

  uint32_t indices[BABY_WEIGHT_HISTORY_MAX_OUT];
  if (maxOut > BABY_WEIGHT_HISTORY_MAX_OUT) maxOut = BABY_WEIGHT_HISTORY_MAX_OUT;
  uint32_t n = baby_downsample_indices(h.count, maxOut, indices);

  // Day offsets are relative to the first stored point's timestamp when the
  // profile's admissionEpoch is unknown/unavailable.
  uint32_t baseEpoch = 0;
  const BabyProfile *active = babyStore_findBySeq(seq);
  if (active && active->admissionEpoch != 0) {
    baseEpoch = active->admissionEpoch;
  }

  CircularCursor cur = {h.writeIndex, h.count};
  uint32_t oldest = circ_oldest(&cur, BABY_WEIGHT_POINT_CAP);

  uint32_t written = 0;
  for (uint32_t i = 0; i < n; i++) {
    // indices are oldest-first ranks; map rank -> physical slot.
    uint32_t rank = indices[i];
    uint32_t pos = (oldest + rank) % BABY_WEIGHT_POINT_CAP;
    uint8_t buf[BABY_WEIGHT_POINT_SIZE];
    f.seek(sizeof(FileHeader) + pos * BABY_WEIGHT_POINT_SIZE);
    if (f.read(buf, sizeof(buf)) != sizeof(buf)) continue;
    uint32_t ts;
    uint16_t grams;
    baby_weight_point_decode(buf, &ts, &grams);
    // El primer punto CON HORA fija la base, no el primero a secas: un punto
    // registrado sin reloj llega con ts=0, y tomarlo como base dejaba baseEpoch
    // en 0, con lo que cualquier punto posterior ya sincronizado se convertia
    // en un offset de ~20.000 dias y reventaba la escala del eje.
    if (baseEpoch == 0 && ts != 0) baseEpoch = ts;
    // Sin base o sin hora en el punto no hay dia que calcular: 0 es "dia
    // desconocido", y el display ya trata un eje entero a 0 como "sin dias".
    uint32_t dayOff =
        (baseEpoch != 0 && ts >= baseEpoch) ? (ts - baseEpoch) / 86400u : 0;
    if (dayOff > 0xFFFFu) dayOff = 0xFFFFu;
    out[written].dayOffset = (uint16_t)dayOff;
    out[written].weightGrams = grams;
    written++;
  }
  f.close();
  return written;
}
