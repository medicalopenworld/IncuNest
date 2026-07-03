#include "sb_env_convert.h"
#include <math.h>
#include <stdio.h>

uint8_t sht4x_crc8(const uint8_t *data, size_t len)
{
    /* CRC-8 Sensirion: poly 0x31, init 0xFF, sin reflexión */
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x31u) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

float sht4x_convert_temp(uint16_t raw)
{
    return -45.0f + 175.0f * ((float)raw / 65535.0f);
}

float sht4x_convert_rh(uint16_t raw)
{
    float rh = -6.0f + 125.0f * ((float)raw / 65535.0f);
    if (rh < 0.0f) {
        rh = 0.0f;
    } else if (rh > 100.0f) {
        rh = 100.0f;
    }
    return rh;
}

float sb_als_mv_to_lux(int mv, uint32_t uv_per_lux)
{
    if (uv_per_lux == 0) {
        return 0.0f; /* factor inválido: no dividir por cero */
    }
    return ((float)mv * 1000.0f) / (float)uv_per_lux;
}

/* Añade "%.1f" o "null" en buf[pos]; devuelve la nueva posición o -1 */
static int append_val(char *buf, size_t buf_size, int pos, float v, bool valid)
{
    if (pos < 0 || (size_t)pos >= buf_size) {
        return -1;
    }
    /* NaN/inf con %.1f emitiría "nan"/"inf" — JSON inválido con CRC válido.
     * Un valor no finito se trata como sensor no válido (fail-safe). */
    bool emit = valid && isfinite(v);
    int n = emit ? snprintf(buf + pos, buf_size - (size_t)pos, "%.1f", (double)v)
                 : snprintf(buf + pos, buf_size - (size_t)pos, "null");
    if (n < 0 || n >= (int)(buf_size - (size_t)pos)) {
        return -1;
    }
    return pos + n;
}

static int append_str(char *buf, size_t buf_size, int pos, const char *s)
{
    if (pos < 0 || (size_t)pos >= buf_size) {
        return -1;
    }
    int n = snprintf(buf + pos, buf_size - (size_t)pos, "%s", s);
    if (n < 0 || n >= (int)(buf_size - (size_t)pos)) {
        return -1;
    }
    return pos + n;
}

size_t sb_env_build_event(char *buf, size_t buf_size, const sb_env_readings_t *r, uint32_t ts_ms)
{
    if (buf == NULL || buf_size == 0 || r == NULL) {
        return 0;
    }

    int pos = append_str(buf, buf_size, 0, "{\"type\":\"event\",\"cmd\":\"sensor_data\",\"data\":{\"temp\":[");
    for (int i = 0; i < SB_ENV_SHT_COUNT && pos >= 0; i++) {
        if (i > 0) {
            pos = append_str(buf, buf_size, pos, ",");
        }
        pos = append_val(buf, buf_size, pos, r->temp[i], r->valid[i]);
    }
    pos = append_str(buf, buf_size, pos, "],\"hum\":[");
    for (int i = 0; i < SB_ENV_SHT_COUNT && pos >= 0; i++) {
        if (i > 0) {
            pos = append_str(buf, buf_size, pos, ",");
        }
        pos = append_val(buf, buf_size, pos, r->hum[i], r->valid[i]);
    }
    pos = append_str(buf, buf_size, pos, "],\"lux\":");
    pos = append_val(buf, buf_size, pos, r->lux, r->lux_valid);
    if (pos < 0) {
        return 0;
    }

    char tail[24];
    snprintf(tail, sizeof(tail), "},\"ts\":%lu}", (unsigned long)ts_ms);
    pos = append_str(buf, buf_size, pos, tail);

    return (pos < 0) ? 0 : (size_t)pos;
}
