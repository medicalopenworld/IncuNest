/* Tests Unity de la política de orientación USB — cada TEST_CASE mapea un
 * Scenario del requirement "Tolerancia a la inversión de D+/D-" en
 * openspec/changes/sb-usb-pin-autoswap/specs/usb-transport/spec.md.
 * Política pura: se prueba sin host USB y sin tocar el PHY. */
#include "sensorBoard_usb_orient.h"
#include "unity.h"

#define T_MS 2000u

TEST_CASE("orient: sin enumeracion no intercambia antes del plazo", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS, 1000);

    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, 1000));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, 1000 + T_MS / 2));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, 1000 + T_MS - 1));
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(0, sb_usb_orient_swap_count(&st));
}

TEST_CASE("orient: plazo vencido intercambia una vez y rearma", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS, 1000);

    TEST_ASSERT_EQUAL(SB_ORIENT_SWAP, sb_usb_orient_tick(&st, false, 1000 + T_MS));
    TEST_ASSERT_TRUE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(1, sb_usb_orient_swap_count(&st));

    /* Rearmado: el siguiente tick inmediato y los anteriores al nuevo plazo no actúan */
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, 1000 + T_MS + 1));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, 1000 + 2 * T_MS - 1));
    TEST_ASSERT_TRUE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(1, sb_usb_orient_swap_count(&st));
}

TEST_CASE("orient: alterna normal/cruzado en vencimientos sucesivos", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS, 0);

    for (uint32_t n = 1; n <= 6; n++) {
        TEST_ASSERT_EQUAL(SB_ORIENT_SWAP, sb_usb_orient_tick(&st, false, n * T_MS));
        TEST_ASSERT_EQUAL((n % 2) == 1, sb_usb_orient_is_swapped(&st));
        TEST_ASSERT_EQUAL_UINT32(n, sb_usb_orient_swap_count(&st));
    }
}

TEST_CASE("orient: con host activo nunca intercambia y rearma desde la caida", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, T_MS, 0);

    /* Host activo (SETUP recibido o configurado) mucho después del plazo
     * original: nada — un host lento en configurar no provoca intercambios */
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, true, 5 * T_MS));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, true, 6 * T_MS));
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));

    /* Cae el enlace en 6*T: el plazo cuenta desde ahí, no desde t0 */
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, 6 * T_MS + T_MS - 1));
    TEST_ASSERT_EQUAL(SB_ORIENT_SWAP, sb_usb_orient_tick(&st, false, 6 * T_MS + T_MS));
    TEST_ASSERT_TRUE(sb_usb_orient_is_swapped(&st));

    /* Vuelve el enlace en la orientación cruzada: se conserva, no se toca */
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, true, 20 * T_MS));
    TEST_ASSERT_TRUE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(1, sb_usb_orient_swap_count(&st));
}

TEST_CASE("orient: robusta al desbordamiento de now_ms", "[orient]")
{
    sb_usb_orient_t st;
    const uint32_t t0 = UINT32_MAX - 500u; /* vence tras dar la vuelta */
    sb_usb_orient_init(&st, T_MS, t0);

    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, UINT32_MAX));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, 0));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, (uint32_t)(t0 + T_MS - 1)));
    TEST_ASSERT_EQUAL(SB_ORIENT_SWAP, sb_usb_orient_tick(&st, false, (uint32_t)(t0 + T_MS)));
}

TEST_CASE("orient: timeout 0 desactiva la politica", "[orient]")
{
    sb_usb_orient_t st;
    sb_usb_orient_init(&st, 0, 0);

    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, 0));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, 60000));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, true, 120000));
    TEST_ASSERT_EQUAL(SB_ORIENT_NONE, sb_usb_orient_tick(&st, false, UINT32_MAX));
    TEST_ASSERT_FALSE(sb_usb_orient_is_swapped(&st));
    TEST_ASSERT_EQUAL_UINT32(0, sb_usb_orient_swap_count(&st));
}
