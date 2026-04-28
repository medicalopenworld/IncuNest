/**
 * @file app_config.h
 * @brief Hardware pin assignments and application-level parameters for the CrowPanel Advance 7".
 *
 * @details All hardware-dependent values are centralized here.
 *          Pin assignments come exclusively from display_config.h v2.7 (the authoritative source).
 *          The OLD values in the Arduino main.h (HSYNC/VSYNC/DE/PCLK swapped) are WRONG —
 *          do not use them. See 03_DISPLAY_DRIVER_ANALYSIS.md §1.6 for details.
 *
 * Normativa aplicable:
 * - IEC 60601-2-19 §201.9.6.2.1.101 — minimum display backlight (see DISPLAY_BL_MIN_PCT)
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */
#pragma once

/* IDF includes are NOT included here to keep config/ dependency-free.
 * Each .c file that uses these values includes the appropriate IDF headers. */

/* ==========================================================================
   I2C shared bus (touch GT911 + backlight STC8H1K28)
   Source: display_config.h v2.7
   ========================================================================== */
#define HW_I2C_PORT          0      /**< I2C_NUM_0 */
#define HW_I2C_SDA_PIN       15
#define HW_I2C_SCL_PIN       16
#define HW_I2C_FREQ_HZ       400000

/* ==========================================================================
   Backlight controller — STC8H1K28 MCU auxiliary @ I2C 0x30
   Source: display_config.h v2.7, §DISPLAY_BL_*
   ========================================================================== */
#define HW_BL_I2C_ADDR       0x30
#define HW_BL_ON_VALUE       10     /**< 0x0A — backlight full on */
#define HW_BL_OFF_VALUE      245    /**< 0xF5 — backlight off */

/* ==========================================================================
   Touch controller — Goodix GT911 @ I2C 0x14
   Source: display_config.h v2.7
   ========================================================================== */
#define HW_TOUCH_I2C_ADDR    0x14
#define HW_TOUCH_INT_PIN     (-1)   /**< INT not wired to ESP32-S3 */
#define HW_TOUCH_RST_PIN     (-1)   /**< RST managed by STC8H1K28, not ESP32-S3 */

/* ==========================================================================
   RGB LCD panel — 800×480 RGB565
   Source: display_config.h v2.7 (AUTHORITATIVE — do not use main.h values)
   ========================================================================== */
#define HW_LCD_H_RES              800
#define HW_LCD_V_RES              480
/** Pixel clock restored to 21 MHz (15 MHz was needed for Arduino I2S DMA — no longer applies) */
#define HW_LCD_PCLK_HZ            (21 * 1000 * 1000)
#define HW_LCD_DATA_WIDTH         16     /**< RGB565 */
#define HW_LCD_BIT_PER_PIXEL      16

/** Bounce buffer: 20 lines × 800 px in SRAM to decouple PSRAM DMA from LCD DMA */
#define HW_LCD_BOUNCE_BUF_LINES   20
#define HW_LCD_BOUNCE_BUF_PX      (HW_LCD_BOUNCE_BUF_LINES * HW_LCD_H_RES)

/* --- HSYNC/VSYNC timing (from display_config.h v2.7) --- */
#define HW_LCD_HSYNC_PULSE_W      4
#define HW_LCD_HSYNC_BACK_PORCH   8
#define HW_LCD_HSYNC_FRONT_PORCH  8
#define HW_LCD_VSYNC_PULSE_W      4
#define HW_LCD_VSYNC_BACK_PORCH   8
#define HW_LCD_VSYNC_FRONT_PORCH  8

/* --- Control signals (display_config.h v2.7) --- */
#define HW_LCD_PIN_HSYNC     40
#define HW_LCD_PIN_VSYNC     41
#define HW_LCD_PIN_DE        42
#define HW_LCD_PIN_PCLK      39
#define HW_LCD_PIN_DISP      (-1)  /**< Not used */

/* --- Blue channel (B0 = LSB) --- */
#define HW_LCD_PIN_B0   21
#define HW_LCD_PIN_B1   47
#define HW_LCD_PIN_B2   48
#define HW_LCD_PIN_B3   45
#define HW_LCD_PIN_B4   38

/* --- Green channel (G0 = LSB) --- */
#define HW_LCD_PIN_G0   9
#define HW_LCD_PIN_G1   10
#define HW_LCD_PIN_G2   11
#define HW_LCD_PIN_G3   12
#define HW_LCD_PIN_G4   13
#define HW_LCD_PIN_G5   14

/* --- Red channel (R0 = LSB) --- */
#define HW_LCD_PIN_R0   7
#define HW_LCD_PIN_R1   17
#define HW_LCD_PIN_R2   18
#define HW_LCD_PIN_R3   3
#define HW_LCD_PIN_R4   46

/* ==========================================================================
   UART — Motherboard communication (USB CDC, via CH340C on host side)
   Source: CommTask.h (COMM_BAUD_RATE = 115200, COMM_SERIAL = Serial/USB-CDC)
   ========================================================================== */
#define HW_UART_PORT         0       /**< UART_NUM_0 */
#define HW_UART_BAUD         115200
/** ESP32-S3 USB-CDC pins */
#define HW_UART_TXD          43
#define HW_UART_RXD          44
#define HW_UART_RX_BUF_SIZE  512

/* ==========================================================================
   FreeRTOS task parameters
   Source: 01_ARCHITECTURE_OVERVIEW.md (new IDF architecture recommendations)
   ========================================================================== */
#define TASK_LVGL_STACK_SIZE   (20 * 1024)
#define TASK_LVGL_PRIORITY     5
#define TASK_LVGL_CORE         1

#define TASK_COMM_STACK_SIZE   (8 * 1024)
#define TASK_COMM_PRIORITY     4
#define TASK_COMM_CORE         1

#define TASK_OTA_STACK_SIZE    (8 * 1024)
#define TASK_OTA_PRIORITY      3
#define TASK_OTA_CORE          0

/** IPC event queue depth — comm→ui updates */
#define UI_EVENT_QUEUE_DEPTH   16

/* ==========================================================================
   WiFi — OTA Access Point
   ========================================================================== */
#define HW_WIFI_AP_SSID      "IncuNest_Display"
#define HW_WIFI_AP_PASS      ""              /**< Open AP — no password */
#define HW_WIFI_AP_CHANNEL   6
