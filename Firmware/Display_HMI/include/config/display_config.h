/**
 * @file display_config.h
 * @brief Configuración centralizada del display CrowPanel (ESP32-S3)
 *
 * Configuración para CrowPanel Advance 7" (ESP32-S3):
 * - hsync_polarity=0, vsync_polarity=0, de_idle_high=0 (factory code oficial
 * Elecrow).
 * - pclk_active_neg=1, pclk_idle_high=1 (requerido en V1.2 para estabilidad).
 * - Pixel clock: 18 MHz (rango válido 18–21 MHz).
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
// Rotación 180° — define para invertir display y touch simultáneamente.
// 1 = display mirrored X+Y (esp_lcd_panel_mirror true,true) + touch rotation 3
// 0 = sin inversión (mirror false,false) + touch rotation 1
// -----------------------------------------------------------------------------
#define DISPLAY_ROTATE_180 0

// -----------------------------------------------------------------------------
// Resolución del panel
// -----------------------------------------------------------------------------
#define DISPLAY_W 800
#define DISPLAY_H 480

// -----------------------------------------------------------------------------
// Timings de sincronización RGB (ESTABILIDAD DMA)
// -----------------------------------------------------------------------------
/**
 * @brief Frecuencia pixel clock (20 MHz).
 * Aumentado de 18 MHz: algunos lotes de panel requieren PCLK >= 20 MHz para
 * aislar correctamente las filas de gate. A 18 MHz (mínimo del rango 18-21 MHz)
 * ciertos paneles muestran bleed del top de pantalla en la parte inferior.
 * Si hay jitter con Audio I2S activo, probar bajando a 15 MHz.
 */
#define DISPLAY_FREQ_WRITE 20000000UL

#define DISPLAY_HSYNC_POLARITY 0
#define DISPLAY_HSYNC_FRONT_PORCH 8
#define DISPLAY_HSYNC_PULSE_WIDTH 4
#define DISPLAY_HSYNC_BACK_PORCH 8

#define DISPLAY_VSYNC_POLARITY 0
#define DISPLAY_VSYNC_FRONT_PORCH 8
#define DISPLAY_VSYNC_PULSE_WIDTH 4
#define DISPLAY_VSYNC_BACK_PORCH 8

#define DISPLAY_PCLK_ACTIVE_NEG                                                \
  1 // Pixels latched on falling edge (panel requirement)
#define DISPLAY_DE_IDLE_HIGH 0
#define DISPLAY_PCLK_IDLE_HIGH 1

// -----------------------------------------------------------------------------
// Bandera en pantalla de inicio
// INTRO_FLAG_NONE    : sin bandera (no se compila ningún asset de bandera)
// INTRO_FLAG_RASD    : República Árabe Saharaui Democrática
// INTRO_FLAG_TOGO    : República de Togo
// INTRO_FLAG_SENEGAL : República de Senegal
// -----------------------------------------------------------------------------
#define INTRO_FLAG_NONE    0
#define INTRO_FLAG_RASD    1
#define INTRO_FLAG_TOGO    2
#define INTRO_FLAG_SENEGAL 3

#define INTRO_FLAG INTRO_FLAG_SENEGAL

// -----------------------------------------------------------------------------
// Touch hitbox extensions (lv_obj_set_ext_click_area)
// Amplían el área táctil sin cambiar la apariencia visual.
// Ajustar aquí si los usuarios tienen dificultad para pulsar ciertos widgets.
// -----------------------------------------------------------------------------
#define TOUCH_EXT_SMALL  40   // Botones muy pequeños (<40px): ImgButton1, PhotoCancelBtn, LockButtons
#define TOUCH_EXT_MEDIUM 30   // Botones de navegación e iconos (~48px): Settings, Alarms, Arrows, Mute
#define TOUCH_EXT_NARROW 20   // Widgets anchos pero bajos (switches, TempButton, WifiButtons)

// -----------------------------------------------------------------------------
// Power bar widget constants (PID duty cycle indicator)
// -----------------------------------------------------------------------------
#define COLOR_POWER_BAR     lv_color_hex(0xFF8C00)
#define POWER_BAR_WIDTH     8
#define POWER_BAR_HEIGHT    30
#define POWER_BAR_X_OFFSET  10
#define POWER_BAR_PCT_MAX   100
