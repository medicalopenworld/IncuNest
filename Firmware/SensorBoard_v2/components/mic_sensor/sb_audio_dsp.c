#include "sb_audio_dsp.h"
#include <math.h>
#include <stdio.h>

float sb_audio_rms(const int16_t *samples, size_t count)
{
    if (samples == NULL || count == 0) {
        return 0.0f;
    }
    /* Acumulador double: 16k muestras de 32767^2 desbordarían float antes
     * de la raíz sin pérdida controlada */
    double acc = 0.0;
    for (size_t i = 0; i < count; i++) {
        double v = (double)samples[i];
        acc += v * v;
    }
    return (float)sqrt(acc / (double)count);
}

float sb_audio_rms_to_db(float rms, float offset_db)
{
    if (!(rms > 0.0f)) {
        return SB_AUDIO_DB_MIN; /* silencio: suelo finito, nunca -inf */
    }
    float db = 20.0f * log10f(rms / 32768.0f) + offset_db;
    return (db < SB_AUDIO_DB_MIN) ? SB_AUDIO_DB_MIN : db;
}

bool sb_audio_level_plausible(float db)
{
    return isfinite(db) && db >= SB_AUDIO_DB_MIN && db <= SB_AUDIO_DB_MAX;
}

size_t sb_audio_build_event(char *buf, size_t buf_size, float db, uint32_t ts_ms)
{
    if (buf == NULL || buf_size == 0 || !sb_audio_level_plausible(db)) {
        return 0;
    }
    int n = snprintf(buf, buf_size,
                     "{\"type\":\"event\",\"cmd\":\"sound_level\",\"data\":{\"dba\":%.1f},\"ts\":%lu}",
                     (double)db, (unsigned long)ts_ms);
    return (n < 0 || n >= (int)buf_size) ? 0 : (size_t)n;
}
