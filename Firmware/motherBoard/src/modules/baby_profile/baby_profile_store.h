#pragma once
// NVS + LittleFS persistence for baby profiles. All decisions (slot pick,
// eviction, dedup, budget, downsampling) live in baby_profile_core.h; this
// layer only executes them against Preferences and LittleFS.
//
// Thread-safety: call from a single task (CommTask) — no internal locking.
#include "baby_cloud.h"
#include "baby_profile_core.h"

// Mounts nothing itself: LittleFS.begin() is owned by DriveUpload/boot.
// Loads the 3 slots + nextSeq + activeSeq from NVS, rebuilds the archive
// byte index from a directory scan of /weight_archive (defensive vs. a
// crash between a file delete and an index update).
void babyStore_init();

// ---------------- Active slots ----------------
// Snapshot of the 3 active slots (unused slots have slotUsed=false).
void babyStore_getSlots(BabyProfile out[BABY_ACTIVE_SLOTS]);
// Pointer to the slot holding seq, or nullptr.
const BabyProfile *babyStore_findBySeq(uint32_t seq);

// Creates a new profile (name is truncated/comma-stripped defensively).
// If all slots are used, FIFO-evicts (excluding activeSeq) via
// archiveProfile(). Returns the new profile's seq, or 0 when creation is
// refused (defensive: no eligible eviction slot).
uint32_t babyStore_createProfile(const char *name, uint8_t gestWeeks);

// Records a weight answer for seq: updates the slot's last-known weight and
// appends a (now, grams) point to weight_active/<seq>.bin unless it equals
// the last stored point. grams==0 (SKIP) changes nothing. Returns false for
// unknown seq.
bool babyStore_recordWeight(uint32_t seq, uint16_t grams);

// Kangaroo care: the baby is taken out to be with the mother. Explicitly
// NOT a discharge — the profile stays in its active slot and keeps its
// activeSeq protection; only the counter and timestamp move. Returns false
// for an unknown seq.
bool babyStore_recordKangaroo(uint32_t seq);

// Adds accumulated phototherapy exposure to a profile. Caller batches the
// minutes (see CommTask): one NVS slot write per minute would wear flash
// for nothing. Returns false for an unknown seq.
bool babyStore_addPhototherapyMinutes(uint32_t seq, uint32_t minutes);

// Same batching contract, for time under active thermal control.
bool babyStore_addThermoMinutes(uint32_t seq, uint32_t minutes);
bool babyStore_addHumidityMinutes(uint32_t seq, uint32_t minutes);

// Explicit discharge (design decision 9, second entry point): stamps
// dischargeEpoch (now if synced, else 0) + outcome + cause, archives
// immediately, frees the slot. cause is only meaningful for
// outcome==BABY_OUTCOME_DECEASED (validated 0-6 by the caller); store it
// as given for every outcome, don't reinterpret it. Returns false for
// unknown seq.
bool babyStore_discharge(uint32_t seq, uint8_t outcome, uint8_t cause);

// ---------------- Active-control protection ----------------
uint32_t babyStore_getActiveSeq();
void babyStore_setActiveSeq(uint32_t seq);   // 0 clears
// ---------------- Age ----------------
// ageKnown=false when admissionEpoch==0 or no synced time right now.
bool babyStore_deriveAgeDays(uint32_t seq, uint16_t *ageDays);

// ---------------- Audit history (read path) ----------------
// Valid (non-tombstoned) records, newest-archived first, page-sliced.
// Returns how many records were written to out (0..pageSize); *totalCount
// gets the total valid-record count.
uint32_t babyStore_readHistoryPage(uint32_t page, uint32_t pageSize,
                                   BabyProfile *out, uint32_t *totalCount);

// ---------------- Weight history (read path) ----------------
// Reads up to BABY_WEIGHT_HISTORY_MAX_OUT downsampled points for seq, from
// weight_active/ if seq is an active slot, else weight_archive/. Timestamps
// are converted to day-offsets from the profile's admissionEpoch when known
// (0-based otherwise). Returns the number of points written.
uint32_t babyStore_readWeightHistory(uint32_t seq, BabyWeightPoint *out,
                                     uint32_t maxOut);

// Wipes ALL baby data: the 3 NVS slots, the seq counters, the audit log
// and every stored weight curve. Deliberately narrow — calibration, WiFi
// credentials and device counters are untouched, so this is not a factory
// reset and does not need a flash erase. Returns the number of LittleFS
// files removed (for the log).
int babyStore_wipeAll();

// ---------------- Cloud event queue ----------------
// Lifecycle events waiting to be published. Peek -> send -> pop, so a failed
// publish keeps the event queued instead of dropping it: the previous
// dirty-flag scheme cleared itself while BUILDING the payload, so any send
// failure lost the record permanently.
bool babyStore_peekCloudEvent(BabyCloudEvent *out);
void babyStore_popCloudEvent();
// True while the current-occupant attributes differ from what was published.
bool babyStore_attributesDirty();
void babyStore_clearAttributesDirty();
// Snapshot of the current occupant, or nullptr when the incubator is empty.
const BabyProfile *babyStore_currentOccupant();

// Current unix time (0 when never synced) — single source for this module.
uint32_t babyStore_nowEpoch();

// ---------------- Cloud telemetry (design decision 14) ----------------
// The published profile: the active-control baby when set, else the most
// recently created used slot, else nullptr. name/outcome/dischargeEpoch are
// intentionally included in cloud telemetry (Ministry of Health access via
// ThingsBoard) — do not strip them on privacy-minimization grounds. `cause`
// (added alongside this comment) rides the same BABY_EVT_DISCHARGE JSON
// payload as outcome (see baby_cloud.cpp, BABY_CAUSE_KEY) but nothing drains
// this cloud-event queue over the wire yet — that GPRS/WiFi transport wiring
// was still in flight on another branch at the time this was written.
const BabyProfile *babyStore_telemetryProfile();
// One-shot dirty flag, set by create/weight/discharge (same consume-once
// semantics the legacy newBabyDataForTelemetry flag had).
bool babyStore_telemetryDirty();
void babyStore_clearTelemetryDirty();
