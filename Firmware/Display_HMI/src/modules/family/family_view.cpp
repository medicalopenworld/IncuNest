#include "modules/family/family_view.h"

FamilyViewMode family_view_mode(const FamilyViewInputs *in) {
  if (!in) return FAMILY_VIEW_HIDDEN;
  if (!in->notice || !in->onLockScreen || in->anyAlarm || in->linkLost ||
      in->pulseOxShown || in->unlockSliderShown)
    return FAMILY_VIEW_HIDDEN;
  return in->hasUrl ? FAMILY_VIEW_TEXT_QR : FAMILY_VIEW_TEXT;
}

bool family_help_entry_visible(bool hasUrl, bool linkLost) {
  return hasUrl && !linkLost;
}
