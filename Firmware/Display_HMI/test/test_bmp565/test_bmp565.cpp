#include <unity.h>

#include "modules/debug/bmp565.h"

void setUp(void) {}
void tearDown(void) {}

static uint32_t rd32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

void test_row_stride_is_padded_to_four(void) {
  TEST_ASSERT_EQUAL_UINT32(1600, bmp565_row_stride(800));
  TEST_ASSERT_EQUAL_UINT32(4, bmp565_row_stride(1));
  TEST_ASSERT_EQUAL_UINT32(4, bmp565_row_stride(2));
  TEST_ASSERT_EQUAL_UINT32(8, bmp565_row_stride(3));
}

void test_file_size_of_the_panel(void) {
  TEST_ASSERT_EQUAL_UINT32(66 + 800 * 480 * 2, bmp565_file_size(800, 480));
  TEST_ASSERT_EQUAL_UINT32(0, bmp565_file_size(0, 480));
  TEST_ASSERT_EQUAL_UINT32(0, bmp565_file_size(800, 0));
}

void test_header_fields_for_800x480(void) {
  uint8_t h[BMP565_HEADER_SIZE];
  TEST_ASSERT_EQUAL_UINT32(66, bmp565_write_header(h, 800, 480));
  TEST_ASSERT_EQUAL_UINT8('B', h[0]);
  TEST_ASSERT_EQUAL_UINT8('M', h[1]);
  TEST_ASSERT_EQUAL_UINT32(768066, rd32(h + 2)); // file size
  TEST_ASSERT_EQUAL_UINT32(0, rd32(h + 6));      // reserved
  TEST_ASSERT_EQUAL_UINT32(66, rd32(h + 10));    // pixel offset
  TEST_ASSERT_EQUAL_UINT32(40, rd32(h + 14));    // info header size
  TEST_ASSERT_EQUAL_UINT32(800, rd32(h + 18));
  TEST_ASSERT_EQUAL_INT32(-480, (int32_t)rd32(h + 22)); // top-down
  TEST_ASSERT_EQUAL_UINT16(1, rd16(h + 26));
  TEST_ASSERT_EQUAL_UINT16(16, rd16(h + 28));
  TEST_ASSERT_EQUAL_UINT32(3, rd32(h + 30));      // BI_BITFIELDS
  TEST_ASSERT_EQUAL_UINT32(768000, rd32(h + 34)); // image size
  TEST_ASSERT_EQUAL_UINT32(0, rd32(h + 46));      // colours used
  TEST_ASSERT_EQUAL_UINT32(0xF800, rd32(h + 54));
  TEST_ASSERT_EQUAL_UINT32(0x07E0, rd32(h + 58));
  TEST_ASSERT_EQUAL_UINT32(0x001F, rd32(h + 62));
}

void test_header_rejects_empty_image(void) {
  uint8_t h[BMP565_HEADER_SIZE];
  TEST_ASSERT_EQUAL_UINT32(0, bmp565_write_header(h, 0, 480));
  TEST_ASSERT_EQUAL_UINT32(0, bmp565_write_header(nullptr, 800, 480));
}

void test_rotate180_reverses_pixels(void) {
  uint16_t odd[5] = {1, 2, 3, 4, 5};
  bmp565_rotate180(odd, 5);
  const uint16_t odd_exp[5] = {5, 4, 3, 2, 1};
  TEST_ASSERT_EQUAL_UINT16_ARRAY(odd_exp, odd, 5);

  // 2x2 image: top-left goes to bottom-right.
  uint16_t img[4] = {0xA, 0xB, 0xC, 0xD};
  bmp565_rotate180(img, 4);
  const uint16_t img_exp[4] = {0xD, 0xC, 0xB, 0xA};
  TEST_ASSERT_EQUAL_UINT16_ARRAY(img_exp, img, 4);

  uint16_t one[1] = {7};
  bmp565_rotate180(one, 1);
  TEST_ASSERT_EQUAL_UINT16(7, one[0]);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_row_stride_is_padded_to_four);
  RUN_TEST(test_file_size_of_the_panel);
  RUN_TEST(test_header_fields_for_800x480);
  RUN_TEST(test_header_rejects_empty_image);
  RUN_TEST(test_rotate180_reverses_pixels);
  return UNITY_END();
}
