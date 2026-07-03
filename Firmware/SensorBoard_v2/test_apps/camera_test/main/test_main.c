/* Tests Unity de camera_sensor — Scenarios de
 * openspec/changes/sb-phase5-camera/specs/camera/spec.md */
#include "unity.h"
#include "sb_cam_builder.h"
#include <string.h>

TEST_CASE("capture_ok: exact JSON", "[cam]")
{
    char buf[160];
    size_t n = sb_cam_build_capture_ok(buf, sizeof(buf), 7, 12345, 99);
    TEST_ASSERT_EQUAL_STRING("{\"type\":\"resp\",\"cmd\":\"capture\",\"id\":7,"
                             "\"status\":\"ok\",\"size\":12345,\"ts\":99}",
                             buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

TEST_CASE("capture_err: exact JSON", "[cam]")
{
    char buf[160];
    size_t n = sb_cam_build_capture_err(buf, sizeof(buf), 7, "capture failed", 55);
    TEST_ASSERT_EQUAL_STRING("{\"type\":\"resp\",\"cmd\":\"capture\",\"id\":7,"
                             "\"status\":\"error\",\"msg\":\"capture failed\",\"ts\":55}",
                             buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

TEST_CASE("capture_err: busy message", "[cam]")
{
    char buf[160];
    size_t n = sb_cam_build_capture_err(buf, sizeof(buf), 2, "busy", 1);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"msg\":\"busy\""));
}

TEST_CASE("builders: tiny buffer returns 0", "[cam]")
{
    char tiny[8];
    TEST_ASSERT_EQUAL(0, sb_cam_build_capture_ok(tiny, sizeof(tiny), 1, 1, 1));
    TEST_ASSERT_EQUAL(0, sb_cam_build_capture_err(tiny, sizeof(tiny), 1, "x", 1));
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
