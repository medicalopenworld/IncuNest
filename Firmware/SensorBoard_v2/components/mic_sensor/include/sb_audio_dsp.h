/* Parte pura de mic_sensor (sin I2S/RTOS). Expuesta para tests Unity. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SB_AUDIO_DB_MIN 0.0f
#define SB_AUDIO_DB_MAX 140.0f

/* RMS de una ventana int16; 0.0f si count == 0 */
float sb_audio_rms(const int16_t *samples, size_t count);

/* rms → dB: 20·log10(rms/32768) + offset_db, con suelo en SB_AUDIO_DB_MIN
 * (el silencio absoluto no produce -inf) */
float sb_audio_rms_to_db(float rms, float offset_db);

/* Gate de plausibilidad: finito y dentro de [SB_AUDIO_DB_MIN, SB_AUDIO_DB_MAX] */
bool sb_audio_level_plausible(float db);

/* {"type":"event","cmd":"sound_level","data":{"dba":<n.1>},"ts":ts}
 * Devuelve 0 si el nivel no pasa el gate o no cabe. */
size_t sb_audio_build_event(char *buf, size_t buf_size, float db, uint32_t ts_ms);
