#pragma once
#include "incunest_afe4490.h"
#include <freertos/task.h>

#define SPO2_INIT_TIME 1000

extern INCUNEST_AFE4490 afe;
extern TaskHandle_t     g_spo2_task;
extern volatile AFE4490Data g_spo2_data;

void initSPO2();
void SPO2_Task(void *pvParameters);
