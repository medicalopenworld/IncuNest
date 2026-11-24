#pragma once
#include <stddef.h>
#include <stdint.h>

// 16-bit BMP (BI_BITFIELDS, RGB565) for the /debug/screenshot endpoint.
//
// Layout: BITMAPFILEHEADER (14) + BITMAPINFOHEADER (40) + three DWORD colour
// masks R 0xF800, G 0x07E0, B 0x001F (12) = 66 bytes, then the pixel rows.
// The height is written negative, so rows go top to bottom and the panel's
// framebuffer can be sent as it is, little-endian RGB565, without reordering.
// BMP needs each row padded to a multiple of 4 bytes; 800 px x 2 = 1600 already is.

constexpr size_t BMP565_HEADER_SIZE = 66;

// Bytes per row, padded to a multiple of 4.
size_t bmp565_row_stride(uint32_t width);

// Header + padded rows. 0 if width or height is 0.
size_t bmp565_file_size(uint32_t width, uint32_t height);

// Writes the 66-byte header into out (at least BMP565_HEADER_SIZE bytes).
// Returns BMP565_HEADER_SIZE, or 0 if width or height is 0.
size_t bmp565_write_header(uint8_t *out, uint32_t width, uint32_t height);

// Reverses n pixels in place: a framebuffer drawn with the panel mirrored on
// both axes (DISPLAY_ROTATE_180) becomes the image the user sees.
void bmp565_rotate180(uint16_t *px, size_t n);
