#include <esp_task_wdt.h>

#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

void watchdogInit(uint32_t wdt_timeout);
void watchdogReload();
void brownOutConfig(uint32_t val);