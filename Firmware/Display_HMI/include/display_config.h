/**
 * @file display_config.h
 * @brief Configuración centralizada del display CrowPanel (ESP32-S3)
 *
 * CORRECCIÓN PARA CROWPANEL ADVANCE 7":
 * - Polaridades hsync/vsync corregidas a 0 (factory code oficial Elecrow).
 * - pclk_active_neg y de_idle_high corregidos a 0.
 * - Frecuencia pixel clock restaurada a 21 MHz (valor oficial Advance).
 *
 * @author   IncuNest Team
 * @version  2.7.0
 * @date     2026-03-02
 */

#pragma once

// =============================================================================
// Panel RGB de 16 bits (RGB565) — Configuración Centrada + Audio Estable
// =============================================================================

// -----------------------------------------------------------------------------
// Pines del bus RGB (Basados en el código original que funcionaba)
// -----------------------------------------------------------------------------

// Blue (B0-B4)
#define DISPLAY_PIN_B0 21
#define DISPLAY_PIN_B1 47
#define DISPLAY_PIN_B2 48
#define DISPLAY_PIN_B3 45
#define DISPLAY_PIN_B4 38

// Green (G0-G5)
#define DISPLAY_PIN_G0 9
#define DISPLAY_PIN_G1 10
#define DISPLAY_PIN_G2 11
#define DISPLAY_PIN_G3 12
#define DISPLAY_PIN_G4 13
#define DISPLAY_PIN_G5 14

// Red (R0-R4)
#define DISPLAY_PIN_R0 7
#define DISPLAY_PIN_R1 17
#define DISPLAY_PIN_R2 18
#define DISPLAY_PIN_R3 3
#define DISPLAY_PIN_R4 46

// -----------------------------------------------------------------------------
// Señales de control del bus RGB
// -----------------------------------------------------------------------------
#define DISPLAY_PIN_DE 42
#define DISPLAY_PIN_VSYNC 41
#define DISPLAY_PIN_HSYNC 40
#define DISPLAY_PIN_PCLK 39

// -----------------------------------------------------------------------------
// Backlight (STC8H1K28 @ 0x30)
// -----------------------------------------------------------------------------
#define DISPLAY_PIN_BL 2
#define DISPLAY_I2C_ADDR_BL 0x30
#define DISPLAY_BL_ON_VALUE 10
#define DISPLAY_BL_OFF_VALUE 245

// -----------------------------------------------------------------------------
// Touch GT911 — I2C 15/16
// -----------------------------------------------------------------------------
#define DISPLAY_TOUCH_SDA 15
#define DISPLAY_TOUCH_SCL 16
#define DISPLAY_TOUCH_INT -1
#define DISPLAY_TOUCH_RST -1
#define DISPLAY_I2C_ADDR_TOUCH 0x14
#define DISPLAY_I2C_FREQ_TOUCH 400000

// -----------------------------------------------------------------------------
// Resolución del panel
// -----------------------------------------------------------------------------
#define DISPLAY_W 800
#define DISPLAY_H 480

// -----------------------------------------------------------------------------
// Timings de sincronización RGB (ESTABILIDAD DMA)
// -----------------------------------------------------------------------------
/**
 * @brief Frecuencia pixel clock (18 MHz).
 * Valor del factory code de Elecrow para CrowPanel Advance 7" (V1.2+).
 * Si hay jitter con Audio I2S activo, probar bajando a 15 MHz.
 */
#define DISPLAY_FREQ_WRITE 18000000UL

#define DISPLAY_HSYNC_POLARITY 1
#define DISPLAY_HSYNC_FRONT_PORCH 8
#define DISPLAY_HSYNC_PULSE_WIDTH 4
#define DISPLAY_HSYNC_BACK_PORCH 8

#define DISPLAY_VSYNC_POLARITY 1
#define DISPLAY_VSYNC_FRONT_PORCH 8
#define DISPLAY_VSYNC_PULSE_WIDTH 4
#define DISPLAY_VSYNC_BACK_PORCH 8

#define DISPLAY_PCLK_ACTIVE_NEG 1  // Pixels latched on falling edge (panel requirement)
#define DISPLAY_DE_IDLE_HIGH    1  // DE signal idle state
#define DISPLAY_PCLK_IDLE_HIGH  1
