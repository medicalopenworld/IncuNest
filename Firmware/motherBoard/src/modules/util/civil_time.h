#pragma once
// Pure civil-date -> Unix-epoch conversion. Kept out of the GPRS task so it
// can be unit-tested on the host: mktime() would depend on the runtime TZ,
// which is exactly the kind of ambiguity we don't want when stamping a
// baby's admission time.
#include <stdbool.h>
#include <stdint.h>

// Days since 1970-01-01 for a proleptic-Gregorian civil date.
// (Howard Hinnant's days_from_civil.) Month is 1-12, day 1-31.
int64_t civil_days_from_epoch(int year, unsigned month, unsigned day);

// UTC epoch for a civil date/time, minus tzQuarterHours (the offset the
// modem reports alongside its clock, in quarter-hour units, so +8 == UTC+2).
// Returns false when the fields are out of range or the result is before
// 2021-01-01 (the same "clearly not synced" floor the rest of the firmware
// uses) — an unsynced SIM800 reports 2004-01-01 and must never be trusted.
bool civil_to_unix_utc(int year, unsigned month, unsigned day, unsigned hour,
                       unsigned minute, unsigned second, int tzQuarterHours,
                       uint32_t *outEpoch);
