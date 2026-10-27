#include "modules/storage/hmi_fs_format.h"

void hmifs_erase_plan_init(HmiFsErasePlan *p, uint32_t totalBytes) {
  if (!p) return;
  p->total = totalBytes;
  p->next = 0;
}

bool hmifs_erase_plan_done(const HmiFsErasePlan *p) {
  return !p || p->next >= p->total;
}

bool hmifs_erase_plan_next(HmiFsErasePlan *p, uint32_t *offset, uint32_t *len) {
  if (!p || !offset || !len || hmifs_erase_plan_done(p)) return false;
  const uint32_t left = p->total - p->next;
  *offset = p->next;
  *len = left < HMIFS_ERASE_RANGE_BYTES ? left : HMIFS_ERASE_RANGE_BYTES;
  p->next += *len;
  return true;
}

uint32_t hmifs_erase_plan_ranges(uint32_t totalBytes) {
  return (totalBytes + HMIFS_ERASE_RANGE_BYTES - 1u) / HMIFS_ERASE_RANGE_BYTES;
}

bool hmifs_erase_step_due(uint32_t nowMs, uint32_t lastStepMs) {
  return (uint32_t)(nowMs - lastStepMs) >= HMIFS_ERASE_GAP_MS;
}
