#include "modules/maintenance/weigh_reminder.h"

// Esqueleto del ciclo rojo: compila y enlaza, pero no decide nada todavia.

uint32_t weigh_reference_epoch(const weigh_inputs_t *) { return 0; }

uint32_t weigh_elapsed_hours(const weigh_inputs_t *) { return 0; }

bool weigh_reminder_due(const weigh_inputs_t *) { return false; }

bool weigh_reminder_should_warn(const weigh_inputs_t *) { return false; }

uint32_t weigh_snooze_until(uint32_t) { return 0; }
