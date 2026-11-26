#include <unity.h>

#include "modules/family/family_view.h"

void setUp(void) {}
void tearDown(void) {}

static FamilyViewInputs ok() { return {true, true, true, false, false, false, false}; }

void test_all_clear_with_url_shows_qr(void) {
  auto in = ok();
  TEST_ASSERT_EQUAL(FAMILY_VIEW_TEXT_QR, family_view_mode(&in));
}

void test_without_url_text_only(void) {
  auto in = ok();
  in.hasUrl = false;
  TEST_ASSERT_EQUAL(FAMILY_VIEW_TEXT, family_view_mode(&in));
}

void test_no_notice_hidden(void) {
  auto in = ok();
  in.notice = false;
  TEST_ASSERT_EQUAL(FAMILY_VIEW_HIDDEN, family_view_mode(&in));
}

void test_not_on_lock_hidden(void) {
  auto in = ok();
  in.onLockScreen = false;
  TEST_ASSERT_EQUAL(FAMILY_VIEW_HIDDEN, family_view_mode(&in));
}

void test_alarm_hides_then_returns(void) {
  auto in = ok();
  in.anyAlarm = true;
  TEST_ASSERT_EQUAL(FAMILY_VIEW_HIDDEN, family_view_mode(&in));
  in.anyAlarm = false;
  TEST_ASSERT_EQUAL(FAMILY_VIEW_TEXT_QR, family_view_mode(&in));
}

void test_link_lost_hides(void) {
  auto in = ok();
  in.linkLost = true;
  TEST_ASSERT_EQUAL(FAMILY_VIEW_HIDDEN, family_view_mode(&in));
}

void test_pulseox_hides(void) {
  auto in = ok();
  in.pulseOxShown = true;
  TEST_ASSERT_EQUAL(FAMILY_VIEW_HIDDEN, family_view_mode(&in));
}

void test_unlock_slider_hides(void) {
  auto in = ok();
  in.unlockSliderShown = true;
  TEST_ASSERT_EQUAL(FAMILY_VIEW_HIDDEN, family_view_mode(&in));
}

void test_help_entry(void) {
  TEST_ASSERT_TRUE(family_help_entry_visible(true, false));
  TEST_ASSERT_FALSE(family_help_entry_visible(false, false));
  TEST_ASSERT_FALSE(family_help_entry_visible(true, true));
  TEST_ASSERT_FALSE(family_help_entry_visible(false, true));
}

// URL sin aviso (la madre ingresa y nadie le coge la mano todavia): la
// tarjeta de bloqueo no sale, pero la ayuda si ofrece la tarjeta Familia.
void test_url_without_notice_help_only(void) {
  auto in = ok();
  in.notice = false;
  TEST_ASSERT_EQUAL(FAMILY_VIEW_HIDDEN, family_view_mode(&in));
  TEST_ASSERT_TRUE(family_help_entry_visible(true, false));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_all_clear_with_url_shows_qr);
  RUN_TEST(test_without_url_text_only);
  RUN_TEST(test_no_notice_hidden);
  RUN_TEST(test_not_on_lock_hidden);
  RUN_TEST(test_alarm_hides_then_returns);
  RUN_TEST(test_link_lost_hides);
  RUN_TEST(test_pulseox_hides);
  RUN_TEST(test_unlock_slider_hides);
  RUN_TEST(test_help_entry);
  RUN_TEST(test_url_without_notice_help_only);
  return UNITY_END();
}
