#include "modules/trend/trend_policy.h"

TrendSetpointValidity trend_setpoint_validity(int actuation, bool airMode) {
  TrendSetpointValidity v = {false, false, false};
  // ActuationMode (shared/control_types.h): 0 off, 1 temp, 2 hum, 3 ambas. Se
  // usan los valores y no el enum para que el modulo compile sin shared/.
  if (actuation < 0 || actuation > 3) return v;
  const bool tempOn = (actuation == 1 || actuation == 3);
  const bool humOn = (actuation == 2 || actuation == 3);
  v.air = tempOn && airMode;
  v.skin = tempOn && !airMode;
  v.hum = humOn;
  return v;
}

int trend_slot_to_px(int64_t slot, int64_t startSlot, int64_t totalSlots,
                     int widthPx) {
  if (totalSlots <= 0 || widthPx <= 0) return -1;
  const int64_t off = slot - startSlot;
  if (off < 0 || off >= totalSlots) return -1;
  return (int)((off * (int64_t)widthPx) / totalSlots);
}

TrendMarkKind trend_mark_kind(uint8_t priority, bool raised) {
  if (!raised) return TREND_MARK_CLEARED;
  switch (priority) {
    case 0:
      return TREND_MARK_LOW;
    case 1:
      return TREND_MARK_MEDIUM;
    default:
      return TREND_MARK_HIGH;  // 2 y cualquier valor desconocido
  }
}
