/* Parte pura de env_sensors (sin I2C/ADC/RTOS): conversiones y builder del
 * evento. Expuesta en include/ para los tests Unity. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SB_ENV_SHT_COUNT 3

/* CRC-8 Sensirion: poly 0x31, init 0xFF (datasheet SHT4x, tabla 10) */
uint8_t sht4x_crc8(const uint8_t *data, size_t len);

/* Fórmulas datasheet SHT4x: T = -45 + 175·raw/65535 (°C);
 * RH = -6 + 125·raw/65535, acotada a [0,100] (%) */
float sht4x_convert_temp(uint16_t raw);
float sht4x_convert_rh(uint16_t raw);

/* mV → lux con factor µV/lux (Kconfig SB_ALS_UV_PER_LUX; NO calibrado) */
float sb_als_mv_to_lux(int mv, uint32_t uv_per_lux);

typedef struct {
    float temp[SB_ENV_SHT_COUNT];
    float hum[SB_ENV_SHT_COUNT];
    bool valid[SB_ENV_SHT_COUNT]; /* posición i = sensor i (ADR-0002) */
    float lux;
    bool lux_valid;
} sb_env_readings_t;

/* {"type":"event","cmd":"sensor_data","data":{"temp":[...],"hum":[...],
 * "lux":L},"ts":ts} — null en posiciones no válidas. Devuelve longitud o 0. */
size_t sb_env_build_event(char *buf, size_t buf_size, const sb_env_readings_t *r, uint32_t ts_ms);
