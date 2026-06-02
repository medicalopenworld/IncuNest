#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "alarm_ids.h"

void     alarm_machine_init(void);
void     alarm_machine_set(AlarmId id, bool active);
bool     alarm_machine_get(AlarmId id);
uint32_t alarm_machine_bitmask(void);
bool     alarm_machine_any_active(void);
bool     alarm_machine_any_critical(void);
