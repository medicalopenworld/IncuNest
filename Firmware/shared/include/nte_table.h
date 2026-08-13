#pragma once

#include <cstdint>

// Neutral Thermal Environment (NTE) air-temperature range lookup.
// Single source of truth shared by motherBoard and Display_HMI — neither
// board may keep its own copy of this table (see
// specs/nte-calculation-engine/spec.md, "Single shared implementation").
struct NteRange {
  float lo;
  float hi;
  float mid;
  bool estimated;
};

// Pure function: no I/O, no global state, no hardware access. Safe for any
// input — out-of-table combinations return the same sentinel as SKIP
// (estimated=true, lo=hi=mid=-1) instead of reading out of bounds.
//
// weightGrams: 0 is the SKIP sentinel (weight not provided).
// ageDays: whole days since admission (0 = day of admission).
NteRange calculateNteRange(uint16_t weightGrams, uint8_t gestWeeks,
                            uint16_t ageDays);
