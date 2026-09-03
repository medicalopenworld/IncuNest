/* Tests Unity de la política de orientación USB (v2: solo con evidencia de
 * host) — cada TEST_CASE mapea un Scenario del requirement "Tolerancia a la
 * inversión de D+/D-" en
 * openspec/changes/sb-usb-autoswap-host-evidence/specs/usb-transport/spec.md.
 * Política pura: se prueba sin host USB y sin tocar el PHY. */
#include "sensorBoard_usb_orient.h"
#include "unity.h"

#define T_MS 2000u

/* tick sin host y sin reset */
#define TICK_IDLE(st, now) sb_usb_orient_tick((st), false, false, (now))
/* tick sin host con reset visto */
#define TICK_RESET(st, now) sb_usb_orient_tick((st), false, true, (now))
/* tick con host activo */
#define TICK_HOST(st, now) sb_usb_orient_tick((st), true, false, (now))

TEST_CASE("orient: sin evidencia de host nunca intercambia", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS);

    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));
    for (uint32_t t = 0; t <= 600000u; t += 500u) { /* 10 min sin host */
        TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, t));
    }
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(0, sb_usb_orient_swap_count(&st));
}

TEST_CASE("orient: reset sin host -> un intercambio a los T y quieto", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS);
    const uint32_t t0 = 1000;

    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, t0));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, t0 + T_MS / 2));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, t0 + T_MS - 1));
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));

    TEST_ASSERT_EQUAL(SB_ORIENT_SWAP, TICK_IDLE(&st, t0 + T_MS));
    TEST_ASSERT_TRUE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(1, sb_usb_orient_swap_count(&st));

    /* Sin nuevo reset: quieto indefinidamente (el host tarda lo que tarde) */
    for (uint32_t t = t0 + T_MS + 1; t <= t0 + 60 * T_MS; t += 250u) {
        TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, t));
    }
    TEST_ASSERT_TRUE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(1, sb_usb_orient_swap_count(&st));
}

TEST_CASE("orient: resets repetidos no extienden el plazo", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS);
    const uint32_t t0 = 5000;

    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, t0));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, t0 + T_MS / 4));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, t0 + T_MS / 2));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, t0 + T_MS - 1));
    TEST_ASSERT_EQUAL(SB_ORIENT_SWAP, TICK_IDLE(&st, t0 + T_MS));
    TEST_ASSERT_EQUAL_UINT32(1, sb_usb_orient_swap_count(&st));
}

TEST_CASE("orient: host activo desarma y una caida sin reset no intercambia", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS);
    const uint32_t t0 = 0;

    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, t0));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_HOST(&st, t0 + 300)); /* SETUP ~100-300 ms tras reset */
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_HOST(&st, t0 + T_MS));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_HOST(&st, t0 + 10 * T_MS));
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));

    /* El host desaparece sin bus reset (motherboard apagada, sin VBUS sensing) */
    for (uint32_t t = t0 + 10 * T_MS; t <= t0 + 40 * T_MS; t += 250u) {
        TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, t));
    }
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(0, sb_usb_orient_swap_count(&st));
}

TEST_CASE("orient: reset y host activo en el mismo tick no arma", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS);

    /* El host ya habló en este mismo tick: no tiene sentido armar */
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, true, true, 100));
    for (uint32_t t = 100; t <= 100 + 5 * T_MS; t += 250u) {
        TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, t));
    }
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(0, sb_usb_orient_swap_count(&st));
}

TEST_CASE("orient: nueva evidencia tras intercambiar vuelve a intercambiar", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS);

    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, 0));
    TEST_ASSERT_EQUAL(SB_ORIENT_SWAP, TICK_IDLE(&st, T_MS));
    TEST_ASSERT_TRUE(sb_usb_orient_is_swapped(&st));

    /* El host recupera el puerto y hace reset otra vez; sigue sin hablarnos */
    const uint32_t t1 = 3 * T_MS;
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, t1));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, t1 + T_MS - 1));
    TEST_ASSERT_EQUAL(SB_ORIENT_SWAP, TICK_IDLE(&st, t1 + T_MS));
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st)); /* de vuelta a normal */
    TEST_ASSERT_EQUAL_UINT32(2, sb_usb_orient_swap_count(&st));
}

TEST_CASE("orient: robusta al desbordamiento de now_ms", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS);
    const uint32_t t0 = UINT32_MAX - 500u; /* vence tras dar la vuelta */

    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, t0));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, UINT32_MAX));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, 0));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, (uint32_t)(t0 + T_MS - 1)));
    TEST_ASSERT_EQUAL(SB_ORIENT_SWAP, TICK_IDLE(&st, (uint32_t)(t0 + T_MS)));
}

TEST_CASE("orient: timeout 0 desactiva la politica", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, 0);

    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, 0));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, 60000));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_HOST(&st, 120000));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_RESET(&st, 180000));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, TICK_IDLE(&st, UINT32_MAX));
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(0, sb_usb_orient_swap_count(&st));
}
