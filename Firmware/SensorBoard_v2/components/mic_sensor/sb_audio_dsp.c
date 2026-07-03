#include "sb_audio_dsp.h"

float sb_audio_rms(const int16_t *samples, size_t count)
{
    (void)samples;
    (void)count;
    return -1.0f;
}

float sb_audio_rms_to_db(float rms, float offset_db)
{
    (void)rms;
    (void)offset_db;
    return -1.0f;
}

bool sb_audio_level_plausible(float db)
{
    (void)db;
    return false;
}

size_t sb_audio_build_event(char *buf, size_t buf_size, float db, uint32_t ts_ms)
{
    (void)buf;
    (void)buf_size;
    (void)db;
    (void)ts_ms;
    return 0;
}
