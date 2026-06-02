#pragma once
#include <stdbool.h>

void hmi_audio_module_init(void);
void hmi_audio_module_beep(int freq_hz, int duration_ms);
