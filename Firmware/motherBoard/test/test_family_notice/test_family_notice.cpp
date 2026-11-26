#include <unity.h>

#include <string.h>

#include "modules/family/family_notice.h"

static FamilyNotice f;

void setUp(void) { family_notice_init(&f); }
void tearDown(void) {}

static const uint32_t E = 1789000000u; // 2026-09-08

// Mensaje del estado visible a `now`, sin importar lo ya mandado.
static FamilyMsg take(FamilyNotice *n, uint32_t now) {
  FamilyMsg m;
  memset(&m, 0, sizeof m);
  family_notice_take_msg(n, now, true, &m);
  return m;
}

void test_trusted_clock_shows_until(void) {
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 1000);
  TEST_ASSERT_TRUE(take(&f, 1000 + 119000).notice);
  TEST_ASSERT_FALSE(take(&f, 1000 + 121000).notice);
}

void test_trusted_clock_caps_at_30_min(void) {
  family_notice_on_hand_hold(&f, (E + 7200) * 1000.0, false, E, true, 1000);
  TEST_ASSERT_TRUE(take(&f, 1000 + 1799000).notice);
  TEST_ASSERT_FALSE(take(&f, 1000 + 1800000).notice);
}

void test_past_until_ignored(void) {
  family_notice_on_hand_hold(&f, (E - 1) * 1000.0, false, E, true, 1000);
  TEST_ASSERT_FALSE(take(&f, 1000).notice);
}

void test_untrusted_clock_uses_fixed_window(void) {
  family_notice_on_hand_hold(&f, (E + 7200) * 1000.0, false, E, false, 1000);
  TEST_ASSERT_TRUE(take(&f, 1000 + 299000).notice);
  TEST_ASSERT_FALSE(take(&f, 1000 + 300000).notice);
}

void test_request_with_untrusted_clock_ignored(void) {
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, true, E, false, 1000);
  TEST_ASSERT_FALSE(take(&f, 1000).notice);
}

void test_request_with_trusted_clock_shows(void) {
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, true, E, true, 1000);
  TEST_ASSERT_TRUE(take(&f, 1000 + 1000).notice);
}

void test_duplicate_until_ignored(void) {
  const double until = (E + 120) * 1000.0;
  family_notice_on_hand_hold(&f, until, false, E, true, 1000);
  TEST_ASSERT_FALSE(take(&f, 1000 + 121000).notice);
  // ThingsBoard reenvia el ultimo valor al reconectar
  family_notice_on_hand_hold(&f, until, false, E + 121, false, 1000 + 121000);
  TEST_ASSERT_FALSE(take(&f, 1000 + 122000).notice);
}

void test_newer_until_extends(void) {
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 1000);
  family_notice_on_hand_hold(&f, (E + 600) * 1000.0, false, E + 60, true, 1000 + 60000);
  TEST_ASSERT_TRUE(take(&f, 1000 + 539000).notice);
  TEST_ASSERT_FALSE(take(&f, 1000 + 661000).notice);
}

void test_bad_url_clears_qr(void) {
  family_notice_set_url(&f, "https://a.org/f/1");
  TEST_ASSERT_EQUAL_STRING("https://a.org/f/1", take(&f, 0).url);
  family_notice_set_url(&f, "http://x");
  TEST_ASSERT_EQUAL_STRING("", take(&f, 0).url);
  family_notice_set_url(&f, "https://a.org/f/1");
  family_notice_set_url(&f, NULL);
  TEST_ASSERT_EQUAL_STRING("", take(&f, 0).url);
}

void test_discharge_clears_all(void) {
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 1000);
  family_notice_set_url(&f, "https://a.org/f/1");
  TEST_ASSERT_TRUE(take(&f, 2000).notice);
  family_notice_on_discharge(&f);
  FamilyMsg m = take(&f, 2000);
  TEST_ASSERT_FALSE(m.notice);
  TEST_ASSERT_EQUAL_STRING("", m.url);
  // un reenvio del mismo until tras el alta sigue ignorado
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 3000);
  TEST_ASSERT_FALSE(take(&f, 3000).notice);
}

void test_take_msg_only_on_change(void) {
  FamilyMsg m;
  TEST_ASSERT_TRUE(family_notice_take_msg(&f, 0, false, &m));
  TEST_ASSERT_FALSE(family_notice_take_msg(&f, 0, false, &m));
  TEST_ASSERT_TRUE(family_notice_take_msg(&f, 0, true, &m));
  family_notice_set_url(&f, "https://a.org/f/1");
  TEST_ASSERT_TRUE(family_notice_take_msg(&f, 0, false, &m));
  TEST_ASSERT_EQUAL_STRING("https://a.org/f/1", m.url);
  TEST_ASSERT_FALSE(family_notice_take_msg(&f, 0, false, &m));
}

void test_take_msg_on_expiry(void) {
  FamilyMsg m;
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 1000);
  TEST_ASSERT_TRUE(family_notice_take_msg(&f, 1000, false, &m));
  TEST_ASSERT_TRUE(m.notice);
  TEST_ASSERT_FALSE(family_notice_take_msg(&f, 1000 + 60000, false, &m));
  TEST_ASSERT_TRUE(family_notice_take_msg(&f, 1000 + 121000, false, &m));
  TEST_ASSERT_FALSE(m.notice);
  TEST_ASSERT_FALSE(family_notice_take_msg(&f, 1000 + 122000, false, &m));
}

void test_millis_wraparound(void) {
  const uint32_t t0 = 0xFFFFF000u;
  family_notice_on_hand_hold(&f, (E + 7200) * 1000.0, false, E, false, t0);
  TEST_ASSERT_TRUE(take(&f, t0 + 299000u).notice);
  TEST_ASSERT_FALSE(take(&f, t0 + 300000u).notice);
}

void test_expired_notice_does_not_return_after_2_pow_31_ms(void) {
  FamilyMsg m;
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 1000);
  TEST_ASSERT_TRUE(family_notice_take_msg(&f, 1000 + 121000, false, &m));
  TEST_ASSERT_FALSE(m.notice);
  // ~24,8 dias despues la resta con signo volveria a dar positivo
  const uint32_t later = 1000 + 121000 + 0x80000000u + 1u;
  TEST_ASSERT_FALSE(take(&f, later).notice);
  TEST_ASSERT_FALSE(family_notice_take_msg(&f, later, false, &m));
}

void test_expired_flag_cleared_before_extend(void) {
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 1000);
  // vence sin que nadie llame a take_msg; un valor invalido/duplicado despues
  // de 2^31 ms no debe resucitarlo
  const uint32_t later = 1000 + 121000 + 0x80000000u + 1u;
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 1000 + 121000);
  TEST_ASSERT_FALSE(take(&f, later).notice);
}

void test_invalid_until_no_state_change(void) {
  family_notice_on_hand_hold(&f, 0.0, false, E, true, 1000);
  family_notice_on_hand_hold(&f, -5.0, false, E, true, 1000);
  TEST_ASSERT_FALSE(take(&f, 1000).notice);
  // un valor valido posterior si cuenta
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 1000);
  TEST_ASSERT_TRUE(take(&f, 2000).notice);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_trusted_clock_shows_until);
  RUN_TEST(test_trusted_clock_caps_at_30_min);
  RUN_TEST(test_past_until_ignored);
  RUN_TEST(test_untrusted_clock_uses_fixed_window);
  RUN_TEST(test_request_with_untrusted_clock_ignored);
  RUN_TEST(test_request_with_trusted_clock_shows);
  RUN_TEST(test_duplicate_until_ignored);
  RUN_TEST(test_newer_until_extends);
  RUN_TEST(test_bad_url_clears_qr);
  RUN_TEST(test_discharge_clears_all);
  RUN_TEST(test_take_msg_only_on_change);
  RUN_TEST(test_take_msg_on_expiry);
  RUN_TEST(test_millis_wraparound);
  RUN_TEST(test_expired_notice_does_not_return_after_2_pow_31_ms);
  RUN_TEST(test_expired_flag_cleared_before_extend);
  RUN_TEST(test_invalid_until_no_state_change);
  return UNITY_END();
}
