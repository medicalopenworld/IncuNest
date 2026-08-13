#pragma once
// Pure, hardware-free baby-profile logic (no Arduino/NVS/LittleFS access):
// slot selection, FIFO eviction with activeSeq protection, age derivation,
// audit-record encoding, circular-cursor math, weight-point dedup, archive
// byte-budget accounting, unified-eviction decisions, and downsampling.
// All I/O lives in baby_profile_store.{h,cpp} (not native-testable).
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BABY_NAME_LEN 24
#define BABY_ACTIVE_SLOTS 3
#define BABY_HISTORY_CAP 1000u
#define BABY_WEIGHT_POINT_CAP 1000u
#define BABY_WEIGHT_ARCHIVE_BUDGET_BYTES (600u * 1024u)
// valid(1) + seq(4) + name(24) + gestWeeks(1) + weightGrams(2) +
// admissionEpoch(4) + dischargeEpoch(4) + outcome(1) + kangarooCount(2) +
// lastKangarooEpoch(4) + phototherapyMinutes(4) + thermoMinutes(4)
#define BABY_HISTORY_RECORD_SIZE 55u
// timestamp(4) + weightGrams(2)
#define BABY_WEIGHT_POINT_SIZE 6u
#define BABY_WEIGHT_HISTORY_MAX_OUT 50u

enum BabyOutcome : uint8_t {
  BABY_OUTCOME_UNKNOWN = 0,
  BABY_OUTCOME_SURVIVED = 1,
  BABY_OUTCOME_DECEASED = 2,
  BABY_OUTCOME_TRANSFERRED = 3,
};

struct BabyProfile {
  bool slotUsed;
  uint32_t seq;
  char name[BABY_NAME_LEN];
  uint8_t gestWeeks;
  uint16_t weightGrams;      // last known; 0 = never provided (SKIP)
  uint32_t admissionEpoch;   // 0 = unknown (no synced time at creation)
  uint32_t dischargeEpoch;   // 0 = not explicitly discharged
  uint8_t outcome;           // BabyOutcome
  // Kangaroo care: the baby leaving the incubator to be with the mother is a
  // normal, repeatable event — explicitly NOT a discharge, so the profile
  // stays in its active slot. Tracked for skin-to-skin traceability.
  uint16_t kangarooCount;    // times taken out to the mother
  uint32_t lastKangarooEpoch;// 0 = never (or no synced clock at the time)
  // Accumulated phototherapy exposure for THIS baby (device-lifetime
  // totals already exist separately in in3.phototherapy_active_time).
  uint32_t phototherapyMinutes;
  // Accumulated time under active thermal control (air or skin) for THIS
  // baby, independent of the device-lifetime totals in in3.
  uint32_t thermoMinutes;
};

// ---------------- Active slots ----------------
// Index of the first free slot, or -1 if all 3 are used.
int baby_find_free_slot(const BabyProfile slots[BABY_ACTIVE_SLOTS]);
// Index of the slot holding `seq`, or -1.
int baby_find_slot_by_seq(const BabyProfile slots[BABY_ACTIVE_SLOTS],
                          uint32_t seq);
// FIFO eviction pick: index of the used slot with the lowest seq, EXCLUDING
// the slot whose seq == activeSeq (activeSeq==0 protects nothing).
// Returns -1 if no eligible slot exists (defensive: caller must refuse
// creation, never evict the active profile).
int baby_pick_eviction_slot(const BabyProfile slots[BABY_ACTIVE_SLOTS],
                            uint32_t activeSeq);

// ---------------- Age derivation ----------------
// True + ageDays when derivable (admissionEpoch known, time synced, and
// now >= admissionEpoch). False otherwise -> caller reports ageKnown=0.
bool baby_derive_age_days(uint32_t admissionEpoch, uint32_t nowEpoch,
                          bool timeSynced, uint16_t *ageDays);

// ---------------- Audit history record ----------------
void baby_history_encode(const BabyProfile *p, bool valid,
                         uint8_t out[BABY_HISTORY_RECORD_SIZE]);
// Returns the record's valid flag; decodes fields into *out.
bool baby_history_decode(const uint8_t rec[BABY_HISTORY_RECORD_SIZE],
                         BabyProfile *out);

// ---------------- Circular cursor (audit log & weight files) ----------------
struct CircularCursor {
  uint32_t writeIndex;  // next slot to write
  uint32_t count;       // records stored, saturates at cap
};
// Slot index to write the next record at; advances the cursor.
uint32_t circ_advance(CircularCursor *c, uint32_t cap);
// Slot index of the oldest stored record (only meaningful if count > 0).
uint32_t circ_oldest(const CircularCursor *c, uint32_t cap);

// ---------------- Weight-point dedup ----------------
// A new point is appended only when it differs from the last stored one.
bool baby_weight_should_append(bool hasLastPoint, uint16_t lastWeightGrams,
                               uint16_t newWeightGrams);
void baby_weight_point_encode(uint32_t timestamp, uint16_t weightGrams,
                              uint8_t out[BABY_WEIGHT_POINT_SIZE]);
void baby_weight_point_decode(const uint8_t in[BABY_WEIGHT_POINT_SIZE],
                              uint32_t *timestamp, uint16_t *weightGrams);

// ---------------- Archive byte budget ----------------
struct ArchiveEntry {
  uint32_t seq;
  uint32_t bytes;
};
uint32_t baby_archive_total_bytes(const ArchiveEntry *entries, int n);
// Index of the oldest (lowest-seq) entry, or -1 when n == 0.
int baby_archive_pick_eviction(const ArchiveEntry *entries, int n);
// True when adding incomingBytes to the current total would exceed budget.
bool baby_archive_over_budget(uint32_t totalBytes, uint32_t incomingBytes,
                              uint32_t budgetBytes);

// ---------------- Unified eviction decision ----------------
enum BabyEvictionTrigger : uint8_t {
  BABY_EVICT_NONE = 0,
  BABY_EVICT_AUDIT_CAP = 1,     // audit log full: circular overwrite of the
                                // oldest record + delete its archive file
  BABY_EVICT_WEIGHT_BUDGET = 2, // archive budget hit first: delete oldest
                                // archive file + tombstone its audit record
};
// Decides which trigger (if any) fires for one incoming archival: an audit
// append of one record plus an archive file of incomingArchiveBytes.
// Budget trigger is evaluated first (may fire repeatedly until it fits);
// audit-cap trigger fires when the audit log is already full.
BabyEvictionTrigger baby_unified_eviction_check(uint32_t auditCount,
                                                uint32_t auditCap,
                                                uint32_t archiveTotalBytes,
                                                uint32_t incomingArchiveBytes,
                                                uint32_t budgetBytes);

// ---------------- Weight point for the read/protocol path ----------------
struct BabyWeightPoint {
  uint16_t dayOffset;
  uint16_t weightGrams;
};

// ---------------- Weight-history downsampling ----------------
// Fills outIndices with the evenly-spaced point indices to send (design
// decision 11): passthrough when count <= maxOut, else
// idx(i) = round(i * (count-1) / (maxOut-1)). Returns how many were written.
// outIndices must hold at least maxOut entries. Deterministic.
uint32_t baby_downsample_indices(uint32_t count, uint32_t maxOut,
                                 uint32_t *outIndices);
