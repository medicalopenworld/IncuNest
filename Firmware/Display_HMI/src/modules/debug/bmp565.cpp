#include "bmp565.h"

#include <string.h>

static void put_u16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)(v >> 24);
}

size_t bmp565_row_stride(uint32_t width) {
  return (((size_t)width * 2u) + 3u) & ~(size_t)3u;
}

size_t bmp565_file_size(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) return 0;
  return BMP565_HEADER_SIZE + bmp565_row_stride(width) * (size_t)height;
}

size_t bmp565_write_header(uint8_t *out, uint32_t width, uint32_t height) {
  if (out == nullptr || width == 0 || height == 0) return 0;
  memset(out, 0, BMP565_HEADER_SIZE);
  const size_t image = bmp565_row_stride(width) * (size_t)height;

  // BITMAPFILEHEADER
  out[0] = 'B';
  out[1] = 'M';
  put_u32(out + 2, (uint32_t)(BMP565_HEADER_SIZE + image)); // file size
  put_u32(out + 10, (uint32_t)BMP565_HEADER_SIZE);          // pixel offset

  // BITMAPINFOHEADER
  put_u32(out + 14, 40);                           // header size
  put_u32(out + 18, width);                        // width
  put_u32(out + 22, (uint32_t)(-(int32_t)height)); // negative: top-down
  put_u16(out + 26, 1);                            // planes
  put_u16(out + 28, 16);                           // bits per pixel
  put_u32(out + 30, 3);                            // BI_BITFIELDS
  put_u32(out + 34, (uint32_t)image);              // image size
  put_u32(out + 38, 2835);                         // 72 dpi horizontal
  put_u32(out + 42, 2835);                         // 72 dpi vertical
  // colours used / important stay 0

  // Colour masks
  put_u32(out + 54, 0xF800);
  put_u32(out + 58, 0x07E0);
  put_u32(out + 62, 0x001F);
  return BMP565_HEADER_SIZE;
}

void bmp565_rotate180(uint16_t *px, size_t n) {
  if (px == nullptr || n < 2) return;
  size_t i = 0, j = n - 1;
  while (i < j) {
    const uint16_t t = px[i];
    px[i] = px[j];
    px[j] = t;
    i++;
    j--;
  }
}
