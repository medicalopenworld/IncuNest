/* Tests Unity de mic_sensor — Scenarios de
 * openspec/changes/sb-phase3-mic/specs/mic-sensor/spec.md */
#include "unity.h"
#include "sb_audio_dsp.h"
#include <math.h>
#include <string.h>

/* ── RMS ───────────────────────────────────────────────────── */

TEST_CASE("rms: constant signal equals its amplitude", "[dsp]")
{
    int16_t s[64];
    for (int i = 0; i < 64; i++) {
        s[i] = 1000;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, sb_audio_rms(s, 64));
}

TEST_CASE("rms: alternating sign constant amplitude", "[dsp]")
{
    int16_t s[64];
    for (int i = 0; i < 64; i++) {
        s[i] = (i % 2) ? 500 : -500;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, sb_audio_rms(s, 64));
}

TEST_CASE("rms: empty window returns 0 without div by zero", "[dsp]")
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sb_audio_rms(NULL, 0));
}

/* ── dB ────────────────────────────────────────────────────── */

TEST_CASE("db: fullscale constant is offset dB", "[dsp]")
{
    /* rms = 32767 ~ fullscale -> ~0 dBFS + offset 120 */
    float db = sb_audio_rms_to_db(32767.0f, 120.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 120.0f, db);
}

TEST_CASE("db: silence clamps to floor, finite", "[dsp]")
{
    float db = sb_audio_rms_to_db(0.0f, 120.0f);
    TEST_ASSERT_TRUE(isfinite(db));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, SB_AUDIO_DB_MIN, db);
}

TEST_CASE("db: half amplitude is ~-6 dB from fullscale", "[dsp]")
{
    float full = sb_audio_rms_to_db(32768.0f, 120.0f);
    float half = sb_audio_rms_to_db(16384.0f, 120.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 6.02f, full - half);
}

/* ── Gate ──────────────────────────────────────────────────── */

TEST_CASE("gate: rejects out-of-range and non-finite", "[dsp]")
{
    TEST_ASSERT_TRUE(sb_audio_level_plausible(42.3f));
    TEST_ASSERT_TRUE(sb_audio_level_plausible(0.0f));
    TEST_ASSERT_FALSE(sb_audio_level_plausible(150.0f));
    TEST_ASSERT_FALSE(sb_audio_level_plausible(-1.0f));
    TEST_ASSERT_FALSE(sb_audio_level_plausible(NAN));
    TEST_ASSERT_FALSE(sb_audio_level_plausible(INFINITY));
}

/* ── Builder ───────────────────────────────────────────────── */

TEST_CASE("build_event: exact JSON", "[event]")
{
    char buf[128];
    size_t n = sb_audio_build_event(buf, sizeof(buf), 42.3f, 8100);
    TEST_ASSERT_EQUAL_STRING(
        "{\"type\":\"event\",\"cmd\":\"sound_level\",\"data\":{\"dba\":42.3},\"ts\":8100}", buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

TEST_CASE("build_event: implausible level returns 0", "[event]")
{
    char buf[128];
    TEST_ASSERT_EQUAL(0, sb_audio_build_event(buf, sizeof(buf), 200.0f, 1));
    TEST_ASSERT_EQUAL(0, sb_audio_build_event(buf, sizeof(buf), NAN, 1));
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
