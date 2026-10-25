#include "service_state.h"

namespace {

struct Slot {
  bool known;
  uint32_t hours;
  uint32_t thresholdH;
};

Slot s_slot[SVC_COUNT];
bool s_snoozed = false;
uint32_t s_snoozeUntil = 0;
// Actuadores vencidos en el momento de pulsar MAS TARDE (bit i = id i).
uint32_t s_snoozedMask = 0;

bool slotDue(const Slot &s) {
  return s.known && s.thresholdH != 0 && s.hours >= s.thresholdH;
}

uint32_t dueMask() {
  uint32_t m = 0;
  for (uint8_t i = 0; i < SVC_COUNT; i++) {
    if (slotDue(s_slot[i])) m |= 1u << i;
  }
  return m;
}

}  // namespace

void ServiceState_Reset(void) {
  for (uint8_t i = 0; i < SVC_COUNT; i++) s_slot[i] = Slot{false, 0, 0};
  s_snoozed = false;
  s_snoozeUntil = 0;
  s_snoozedMask = 0;
}

void ServiceState_Apply(const ServiceReport *r) {
  if (!r) return;
  for (uint8_t i = 0; i < r->n && i < SVC_COUNT; i++) {
    const ServiceEntry &e = r->entry[i];
    if (e.id >= SVC_COUNT) continue;
    s_slot[e.id] = Slot{true, e.hours, e.thresholdH};
  }
}

bool ServiceState_HasData(void) {
  for (uint8_t i = 0; i < SVC_COUNT; i++) {
    if (s_slot[i].known) return true;
  }
  return false;
}

bool ServiceState_Get(uint8_t id, uint32_t *hours, uint32_t *thresholdH) {
  if (id >= SVC_COUNT || !s_slot[id].known) return false;
  if (hours) *hours = s_slot[id].hours;
  if (thresholdH) *thresholdH = s_slot[id].thresholdH;
  return true;
}

bool ServiceState_IsDue(uint8_t id) {
  return id < SVC_COUNT && slotDue(s_slot[id]);
}

bool ServiceState_AnyDue(void) { return dueMask() != 0; }

uint8_t ServiceState_DueCount(void) {
  const uint32_t m = dueMask();
  uint8_t n = 0;
  for (uint8_t i = 0; i < SVC_COUNT; i++) {
    if (m & (1u << i)) n++;
  }
  return n;
}

bool ServiceState_ShouldWarn(uint32_t nowSec) {
  const uint32_t due = dueMask();
  if (due == 0) return false;
  if (!s_snoozed) return true;
  if (due & ~s_snoozedMask) return true;  // algo nuevo que no se ha visto
  // Resta con wrap: correcto aunque nowSec de la vuelta.
  return (uint32_t)(nowSec - s_snoozeUntil) < 0x80000000u;
}

void ServiceState_Snooze(uint32_t nowSec) {
  s_snoozed = true;
  s_snoozeUntil = nowSec + SERVICE_SNOOZE_SECS;
  s_snoozedMask = dueMask();
}
