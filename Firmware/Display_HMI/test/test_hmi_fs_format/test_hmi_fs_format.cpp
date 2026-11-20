#include <unity.h>

#include "modules/storage/hmi_fs_format.h"

void setUp(void) {}
void tearDown(void) {}

// La particion "spiffs" real: 0x5E0000 = 6 012 928 B.
constexpr uint32_t PART = 0x5E0000;

void test_constants_are_64k_ranges_100ms_apart(void) {
  TEST_ASSERT_EQUAL_UINT32(65536u, HMIFS_ERASE_RANGE_BYTES);
  TEST_ASSERT_EQUAL_UINT32(100u, HMIFS_ERASE_GAP_MS);
}

void test_the_real_partition_is_94_ranges_of_64k(void) {
  TEST_ASSERT_EQUAL_UINT32(94u, hmifs_erase_plan_ranges(PART));
  HmiFsErasePlan p;
  hmifs_erase_plan_init(&p, PART);
  uint32_t off = 0, len = 0, n = 0, covered = 0;
  while (hmifs_erase_plan_next(&p, &off, &len)) {
    TEST_ASSERT_EQUAL_UINT32(covered, off);  // contiguos, empezando en 0
    TEST_ASSERT_EQUAL_UINT32(65536u, len);
    covered += len;
    n++;
  }
  TEST_ASSERT_EQUAL_UINT32(94u, n);
  TEST_ASSERT_EQUAL_UINT32(PART, covered);
  TEST_ASSERT_TRUE(hmifs_erase_plan_done(&p));
}

// El primer rango es el offset 0: es donde viven los superblocks, asi que un
// corte a mitad deja la particion "virgen" y el siguiente arranque reinicia el
// formateo en vez de montar una imagen a medias.
void test_the_first_range_starts_at_offset_zero(void) {
  HmiFsErasePlan p;
  hmifs_erase_plan_init(&p, PART);
  uint32_t off = 99, len = 0;
  TEST_ASSERT_TRUE(hmifs_erase_plan_next(&p, &off, &len));
  TEST_ASSERT_EQUAL_UINT32(0u, off);
}

void test_the_last_range_may_be_shorter(void) {
  HmiFsErasePlan p;
  hmifs_erase_plan_init(&p, 100u * 1024u);  // 64 KB + 36 KB
  uint32_t off, len;
  TEST_ASSERT_TRUE(hmifs_erase_plan_next(&p, &off, &len));
  TEST_ASSERT_EQUAL_UINT32(65536u, len);
  TEST_ASSERT_TRUE(hmifs_erase_plan_next(&p, &off, &len));
  TEST_ASSERT_EQUAL_UINT32(65536u, off);
  TEST_ASSERT_EQUAL_UINT32(36u * 1024u, len);
  TEST_ASSERT_FALSE(hmifs_erase_plan_next(&p, &off, &len));
  TEST_ASSERT_EQUAL_UINT32(2u, hmifs_erase_plan_ranges(100u * 1024u));
}

void test_an_empty_plan_is_already_done(void) {
  HmiFsErasePlan p;
  hmifs_erase_plan_init(&p, 0);
  uint32_t off, len;
  TEST_ASSERT_TRUE(hmifs_erase_plan_done(&p));
  TEST_ASSERT_FALSE(hmifs_erase_plan_next(&p, &off, &len));
  TEST_ASSERT_EQUAL_UINT32(0u, hmifs_erase_plan_ranges(0));
}

void test_a_finished_plan_keeps_saying_done(void) {
  HmiFsErasePlan p;
  hmifs_erase_plan_init(&p, 65536u);
  uint32_t off, len;
  TEST_ASSERT_TRUE(hmifs_erase_plan_next(&p, &off, &len));
  for (int i = 0; i < 3; i++) TEST_ASSERT_FALSE(hmifs_erase_plan_next(&p, &off, &len));
}

void test_erase_steps_are_spaced_100ms_even_across_the_millis_wrap(void) {
  TEST_ASSERT_FALSE(hmifs_erase_step_due(1099, 1000));
  TEST_ASSERT_TRUE(hmifs_erase_step_due(1100, 1000));
  TEST_ASSERT_FALSE(hmifs_erase_step_due(50, 0xFFFFFFF0u));   // 66 ms
  TEST_ASSERT_TRUE(hmifs_erase_step_due(100, 0xFFFFFFF0u));   // 116 ms
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_constants_are_64k_ranges_100ms_apart);
  RUN_TEST(test_the_real_partition_is_94_ranges_of_64k);
  RUN_TEST(test_the_first_range_starts_at_offset_zero);
  RUN_TEST(test_the_last_range_may_be_shorter);
  RUN_TEST(test_an_empty_plan_is_already_done);
  RUN_TEST(test_a_finished_plan_keeps_saying_done);
  RUN_TEST(test_erase_steps_are_spaced_100ms_even_across_the_millis_wrap);
  return UNITY_END();
}
