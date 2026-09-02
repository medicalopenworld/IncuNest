#pragma once
// Pure parse/build for the PROFILE_*/WEIGHT_HISTORY_* protocol lines
// (Firmware/PROTOCOL.md). No I/O: CommTask feeds lines in and sends the
// built responses out. Malformed/truncated lines parse to BABY_MSG_NONE
// (silent discard per .claude/rules/security.md — the caller may log the
// reason, never act on partial data).
#include <stddef.h>

#include "baby_profile_core.h"
#include "nte_table.h"

enum BabyProtoMsgType : uint8_t {
  BABY_MSG_NONE = 0,
  BABY_MSG_LIST_REQ,            // HMI,PROFILE_LIST_REQ
  BABY_MSG_NEW,                 // HMI,PROFILE_NEW,<name>,<gestWeeks>
  BABY_MSG_SELECT,              // HMI,PROFILE_SELECT,<seq>
  BABY_MSG_WEIGHT,              // HMI,PROFILE_WEIGHT,<seq>,<grams|SKIP>
  BABY_MSG_AGE_MANUAL,          // HMI,PROFILE_AGE_MANUAL,<seq>,<ageDays>
  BABY_MSG_DISCHARGE,           // HMI,PROFILE_DISCHARGE,<seq>,<outcome>,<cause>
  BABY_MSG_KANGAROO,            // HMI,PROFILE_KANGAROO,<seq>
  BABY_MSG_HISTORY_REQ,         // HMI,PROFILE_HISTORY_REQ,<page>
  BABY_MSG_WEIGHT_HISTORY_REQ,  // HMI,WEIGHT_HISTORY_REQ,<seq>
};

struct BabyProtoMsg {
  BabyProtoMsgType type;
  uint32_t seq;
  uint16_t grams;      // 0 = SKIP for BABY_MSG_WEIGHT
  uint16_t ageDays;
  uint8_t outcome;     // validated 0-3
  uint8_t cause;       // validated 0-6; only meaningful for outcome==2
  uint32_t page;
  uint8_t gestWeeks;
  char name[BABY_NAME_LEN];
};

// Parses one already-line-split input. Returns the message type
// (BABY_MSG_NONE for anything malformed: wrong field count, non-numeric
// numeric field, out-of-range outcome, empty name).
BabyProtoMsgType baby_proto_parse(const char *line, BabyProtoMsg *out);

// ---- Builders (single bounded lines, '\n'-terminated). Each returns the
// number of chars written (excluding '\0'), or 0 when buf is too small. ----

// CTRL,PROFILE_LIST,<n>,{<seq>,<name>,<gestWeeks>,<weightGrams>}xn
int baby_proto_build_list(char *buf, size_t len,
                          const BabyProfile slots[BABY_ACTIVE_SLOTS]);

// CTRL,PROFILE_RANGE,<seq>,<ageKnown>,<ageDays>,<lo>,<hi>,<mid>,<estimated>
int baby_proto_build_range(char *buf, size_t len, uint32_t seq, bool ageKnown,
                           uint16_t ageDays, const NteRange *range);

// CTRL,PROFILE_HISTORY,<page>,<totalCount>,<n>,{<seq>,<name>,<gestWeeks>,
// <lastWeightGrams>,<admissionEpoch>,<dischargeEpoch>,<outcome>,<cause>}xn
int baby_proto_build_history(char *buf, size_t len, uint32_t page,
                             uint32_t totalCount, const BabyProfile *records,
                             uint32_t n);

// CTRL,WEIGHT_HISTORY,<seq>,<n>,{<dayOffset>,<weightGrams>}xn
int baby_proto_build_weight_history(char *buf, size_t len, uint32_t seq,
                                    const BabyWeightPoint *points,
                                    uint32_t n);
