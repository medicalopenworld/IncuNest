/**
 * @file display_config.h
 * @brief Configuración centralizada del display CrowPanel ANTIGUO (ESP32-S3)
 *
 * LEGACY DISPLAY COMPATIBILITY (v3.0.0):
 * - Pines RGB, control, touch y timings adaptados a la pantalla antigua.
 * - Backlight controlado por PWM (no I2C STC8H1K28).
 * - Touch reset vía expansor PCA9557 @ 0x18.
 *
 * @author   IncuNest Team
 * @version  3.0.0
 * @date     2026-03-31
 */

#pragma once

// =============================================================================
// Panel RGB de 16 bits (RGB565) — Pantalla Antigua (CrowPanel legacy)
// =============================================================================

// -----------------------------------------------------------------------------
// Pines del bus RGB (d0-d15 de la pantalla antigua)
// Mapping: d0-d4 = B0-B4, d5-d10 = G0-G5, d11-d15 = R0-R4
// -----------------------------------------------------------------------------

// Blue (B0-B4) → d0..d4
#define DISPLAY_PIN_B0 15
#define DISPLAY_PIN_B1 7
#define DISPLAY_PIN_B2 6
#define DISPLAY_PIN_B3 5
#define DISPLAY_PIN_B4 4

// Green (G0-G5) → d5..d10
#define DISPLAY_PIN_G0 9
#define DISPLAY_PIN_G1 46
#define DISPLAY_PIN_G2 3
#define DISPLAY_PIN_G3 8
#define DISPLAY_PIN_G4 16
#define DISPLAY_PIN_G5 1

// Red (R0-R4) → d11..d15
#define DISPLAY_PIN_R0 14
#define DISPLAY_PIN_R1 21
#define DISPLAY_PIN_R2 47
#define DISPLAY_PIN_R3 48
#define DISPLAY_PIN_R4 45

// -----------------------------------------------------------------------------
// Señales de control del bus RGB (pantalla antigua)
// -----------------------------------------------------------------------------
#define DISPLAY_PIN_DE 41
#define DISPLAY_PIN_VSYNC 40
#define DISPLAY_PIN_HSYNC 39
#define DISPLAY_PIN_PCLK 0

// -----------------------------------------------------------------------------
// Backlight — PWM directo (pantalla antigua, sin STC8H1K28)
// -----------------------------------------------------------------------------
#define DISPLAY_PIN_BL 2
#define DISPLAY_BL_USE_PWM 1          // 1 = PWM, 0 = I2C (STC8H1K28)
#define DISPLAY_I2C_ADDR_BL 0x30      // No usado en pantalla antigua
#define DISPLAY_BL_ON_VALUE 10        // No usado en pantalla antigua
#define DISPLAY_BL_OFF_VALUE 245      // No usado en pantalla antigua

// -----------------------------------------------------------------------------
// Touch GT911 — I2C 19/20 (pantalla antigua)
// Reset vía PCA9557 IO expander @ 0x18
// -----------------------------------------------------------------------------
#define DISPLAY_TOUCH_SDA 19
#define DISPLAY_TOUCH_SCL 20
#define DISPLAY_TOUCH_INT -1
#define DISPLAY_TOUCH_RST -1
#define DISPLAY_I2C_ADDR_TOUCH 0x14
#define DISPLAY_I2C_FREQ_TOUCH 400000
#define DISPLAY_TOUCH_PCA9557_ADDR 0x18

// -----------------------------------------------------------------------------
// Resolución del panel
// -----------------------------------------------------------------------------
#define DISPLAY_W 800
#define DISPLAY_H 480

// -----------------------------------------------------------------------------
// Timings de sincronización RGB (pantalla antigua)
// -----------------------------------------------------------------------------
#define DISPLAY_FREQ_WRITE 15000000UL

#define DISPLAY_HSYNC_POLARITY 0
#define DISPLAY_HSYNC_FRONT_PORCH 40
#define DISPLAY_HSYNC_PULSE_WIDTH 48
#define DISPLAY_HSYNC_BACK_PORCH 40

#define DISPLAY_VSYNC_POLARITY 0
#define DISPLAY_VSYNC_FRONT_PORCH 1
#define DISPLAY_VSYNC_PULSE_WIDTH 31
#define DISPLAY_VSYNC_BACK_PORCH 13

#define DISPLAY_PCLK_ACTIVE_NEG 1
#define DISPLAY_DE_IDLE_HIGH 0
#define DISPLAY_PCLK_IDLE_HIGH 0
