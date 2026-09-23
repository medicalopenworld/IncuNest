/* Tests Unity de door_sensor — Scenarios de
 * openspec/changes/sb-phase4-door/specs/door-sensor/spec.md */
#include "unity.h"
#include "sb_door_logic.h"
#include <string.h>

/* ── FSM ───────────────────────────────────────────────────── */

TEST_CASE("fsm: initial stable level reports once", "[door]")
{
    sb_door_fsm_t fsm;
    sb_door_fsm_init(&fsm);
    /* activo-bajo: nivel 0 = cerrada */
    TEST_ASSERT_EQUAL(SB_DOOR_EVT_CLOSED, sb_door_fsm_update(&fsm, 0, true));
    /* mismo estado: no re-emite */
    TEST_ASSERT_EQUAL(SB_DOOR_EVT_NONE, sb_door_fsm_update(&fsm, 0, true));
}

TEST_CASE("fsm: stable change emits exactly once", "[door]")
{
    sb_door_fsm_t fsm;
    sb_door_fsm_init(&fsm);
    sb_door_fsm_update(&fsm, 0, true); /* inicial: cerrada */

    TEST_ASSERT_EQUAL(SB_DOOR_EVT_OPEN, sb_door_fsm_update(&fsm, 1, true));
    TEST_ASSERT_EQUAL(SB_DOOR_EVT_NONE, sb_door_fsm_update(&fsm, 1, true));
    TEST_ASSERT_EQUAL(SB_DOOR_EVT_CLOSED, sb_door_fsm_update(&fsm, 0, true));
}

TEST_CASE("fsm: bounce back to same level emits nothing", "[door]")
{
    sb_door_fsm_t fsm;
    sb_door_fsm_init(&fsm);
    sb_door_fsm_update(&fsm, 1, true); /* inicial: abierta */
    /* la ventana absorbió el rebote: el nivel estable sigue siendo 1 */
    TEST_ASSERT_EQUAL(SB_DOOR_EVT_NONE, sb_door_fsm_update(&fsm, 1, true));
}

TEST_CASE("fsm: active-high inverts interpretation", "[door]")
{
    sb_door_fsm_t fsm;
    sb_door_fsm_init(&fsm);
    /* activo-alto: nivel 0 = abierta */
    TEST_ASSERT_EQUAL(SB_DOOR_EVT_OPEN, sb_door_fsm_update(&fsm, 0, false));
    TEST_ASSERT_EQUAL(SB_DOOR_EVT_CLOSED, sb_door_fsm_update(&fsm, 1, false));
}

/* ── Builder ───────────────────────────────────────────────── */

TEST_CASE("build_event: door_open exact JSON", "[door]")
{
    char buf[96];
    size_t n = sb_door_build_event(buf, sizeof(buf), SB_DOOR_EVT_OPEN, 5100);
    TEST_ASSERT_EQUAL_STRING("{\"type\":\"event\",\"cmd\":\"door_open\",\"ts\":5100}", buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

TEST_CASE("build_event: door_closed exact JSON", "[door]")
{
    char buf[96];
    size_t n = sb_door_build_event(buf, sizeof(buf), SB_DOOR_EVT_CLOSED, 5300);
    TEST_ASSERT_EQUAL_STRING("{\"type\":\"event\",\"cmd\":\"door_closed\",\"ts\":5300}", buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

TEST_CASE("build_event: NONE or tiny buffer returns 0", "[door]")
{
    char buf[8];
    TEST_ASSERT_EQUAL(0, sb_door_build_event(buf, sizeof(buf), SB_DOOR_EVT_OPEN, 1));
    char big[96];
    TEST_ASSERT_EQUAL(0, sb_door_build_event(big, sizeof(big), SB_DOOR_EVT_NONE, 1));
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
