#include "civil_time.h"

// 2021-01-01T00:00:00Z — same "clearly not synced" floor DriveUpload and
// babyStore_nowEpoch() already use.
static const uint32_t MIN_VALID_EPOCH = 1609459200u;

int64_t civil_days_from_epoch(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int64_t era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = (unsigned)(year - era * 400);              // [0, 399]
  const unsigned doy =
      (153u * (month + (month > 2 ? -3u : 9u)) + 2u) / 5u + day - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;  // [0,146096]
  return era * 146097 + (int64_t)doe - 719468;
}

bool civil_to_unix_utc(int year, unsigned month, unsigned day, unsigned hour,
                       unsigned minute, unsigned second, int tzQuarterHours,
                       uint32_t *outEpoch) {
  if (month < 1 || month > 12) return false;
  if (day < 1 || day > 31) return false;
  if (hour > 23 || minute > 59 || second > 60) return false;
  // The modem reports offsets in quarter hours; nothing real exceeds +/-14 h.
  if (tzQuarterHours < -56 || tzQuarterHours > 56) return false;

  int64_t epoch = civil_days_from_epoch(year, month, day) * 86400LL +
                  (int64_t)hour * 3600LL + (int64_t)minute * 60LL +
                  (int64_t)second;
  epoch -= (int64_t)tzQuarterHours * 900LL;  // local -> UTC

  if (epoch < (int64_t)MIN_VALID_EPOCH || epoch > 4102444800LL) {
    return false;  // unsynced modem clock (or absurd future date)
  }
  *outEpoch = (uint32_t)epoch;
  return true;
}
