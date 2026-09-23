/* Tests Unity de env_sensors — cada TEST_CASE mapea un Scenario de
 * openspec/changes/sb-phase2-env-sensors/specs/env-sensors/spec.md */
#include "unity.h"
#include "sb_env_convert.h"
#include <math.h>
#include <string.h>

/* ── CRC-8 Sensirion ───────────────────────────────────────── */

TEST_CASE("sht4x_crc8 known vector: 0xBE 0xEF == 0x92", "[sht4x]")
{
    const uint8_t data[] = { 0xBE, 0xEF };
    TEST_ASSERT_EQUAL_HEX8(0x92, sht4x_crc8(data, 2));
}

/* ── Conversiones datasheet ────────────────────────────────── */

TEST_CASE("sht4x temp: raw 0x6666 -> 25.0 C", "[sht4x]")
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, sht4x_convert_temp(0x6666));
}

TEST_CASE("sht4x temp: raw 0 -> -45.0 C", "[sht4x]")
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -45.0f, sht4x_convert_temp(0));
}

TEST_CASE("sht4x rh: clamps to [0,100]", "[sht4x]")
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, sht4x_convert_rh(0xFFFF)); /* 119 -> 100 */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sht4x_convert_rh(0));        /* -6 -> 0 */
}

TEST_CASE("sht4x rh: mid-range value unclamped", "[sht4x]")
{
    /* raw 0x8000 -> -6 + 125*0.5 = 56.5 %RH aprox */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 56.5f, sht4x_convert_rh(0x8000));
}

/* ── ALS ───────────────────────────────────────────────────── */

TEST_CASE("als: 1000 mV at 1000 uV/lux -> 1000 lux", "[als]")
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, sb_als_mv_to_lux(1000, 1000));
}

TEST_CASE("als: zero factor does not divide by zero", "[als]")
{
    float lux = sb_als_mv_to_lux(1000, 0);
    TEST_ASSERT_FALSE(isnan(lux) || isinf(lux));
}

/* ── Builder del evento sensor_data ────────────────────────── */

TEST_CASE("build_event: three valid sensors, exact JSON", "[event]")
{
    sb_env_readings_t r = {
        .temp = { 36.5f, 37.0f, 36.8f },
        .hum = { 55.0f, 54.5f, 60.1f },
        .valid = { true, true, true },
        .lux = 320.5f,
        .lux_valid = true,
    };
    char buf[256];
    size_t n = sb_env_build_event(buf, sizeof(buf), &r, 5200);
    TEST_ASSERT_EQUAL_STRING("{\"type\":\"event\",\"cmd\":\"sensor_data\",\"data\":"
                             "{\"temp\":[36.5,37.0,36.8],\"hum\":[55.0,54.5,60.1],"
                             "\"lux\":320.5},\"ts\":5200}",
                             buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

TEST_CASE("build_event: failed sensor is null at its position", "[event]")
{
    sb_env_readings_t r = {
        .temp = { 36.5f, 0.0f, 36.8f },
        .hum = { 55.0f, 0.0f, 60.1f },
        .valid = { true, false, true },
        .lux = 100.0f,
        .lux_valid = true,
    };
    char buf[256];
    size_t n = sb_env_build_event(buf, sizeof(buf), &r, 1);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":[36.5,null,36.8]"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"hum\":[55.0,null,60.1]"));
}

TEST_CASE("build_event: all sensors down still emits", "[event]")
{
    sb_env_readings_t r = { .valid = { false, false, false }, .lux_valid = false };
    char buf[256];
    size_t n = sb_env_build_event(buf, sizeof(buf), &r, 9);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":[null,null,null]"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"lux\":null"));
}

TEST_CASE("build_event: fits in protocol payload limit", "[event]")
{
    /* Valores extremos: el evento debe caber en 256 B */
    sb_env_readings_t r = {
        .temp = { -45.0f, 130.0f, -45.0f },
        .hum = { 100.0f, 100.0f, 100.0f },
        .valid = { true, true, true },
        .lux = 999999.9f,
        .lux_valid = true,
    };
    char buf[257];
    size_t n = sb_env_build_event(buf, sizeof(buf), &r, 4294967295u);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_LESS_OR_EQUAL(256, n);
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
