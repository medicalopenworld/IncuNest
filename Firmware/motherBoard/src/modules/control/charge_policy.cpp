#include "charge_policy.h"

void charge_policy_init(ChargePolicy *p) { p->mode = CHG_POLICY_NORMAL; }

ChargePolicyMode charge_policy_update(ChargePolicy *p, bool vbat_valid,
                                      uint16_t vbat_mv) {
  if (!vbat_valid) return p->mode;

  if (vbat_mv > CHG_POLICY_VBAT_MAX_MV) {
    p->mode = CHG_POLICY_INHIBIT;
    return p->mode;
  }
  switch (p->mode) {
  case CHG_POLICY_INHIBIT:
    if (vbat_mv < CHG_POLICY_VBAT_MAX_RELEASE_MV) {
      p->mode = (vbat_mv < CHG_POLICY_VBAT_LOW_MV) ? CHG_POLICY_REDUCED
                                                   : CHG_POLICY_NORMAL;
    }
    break;
  case CHG_POLICY_REDUCED:
    if (vbat_mv > CHG_POLICY_VBAT_LOW_RELEASE_MV) p->mode = CHG_POLICY_NORMAL;
    break;
  case CHG_POLICY_NORMAL:
  default:
    if (vbat_mv < CHG_POLICY_VBAT_LOW_MV) p->mode = CHG_POLICY_REDUCED;
    break;
  }
  return p->mode;
}

const char *charge_policy_name(ChargePolicyMode m) {
  switch (m) {
  case CHG_POLICY_INHIBIT: return "inhibida";
  case CHG_POLICY_REDUCED: return "reducida";
  case CHG_POLICY_NORMAL:
  default: return "normal";
  }
}
