#include "sb_env_convert.h"

uint8_t sht4x_crc8(const uint8_t *data, size_t len)
{
    (void)data;
    (void)len;
    return 0;
}

float sht4x_convert_temp(uint16_t raw)
{
    (void)raw;
    return 0.0f;
}

float sht4x_convert_rh(uint16_t raw)
{
    (void)raw;
    return 0.0f;
}

float sb_als_mv_to_lux(int mv, uint32_t uv_per_lux)
{
    (void)mv;
    (void)uv_per_lux;
    return 0.0f;
}

size_t sb_env_build_event(char *buf, size_t buf_size, const sb_env_readings_t *r, uint32_t ts_ms)
{
    (void)buf;
    (void)buf_size;
    (void)r;
    (void)ts_ms;
    return 0;
}
