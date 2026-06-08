#pragma once
#include "control_types.h"

// User Interface display constants
#define valuePosition 245
#define separatorPosition 240
#define unitPosition 315
#define textFontSize 2 // text default size
#define width_select 20
#define TFT_HEIGHT_HEADING 34
#define TFT_SEPARATOR_HEIGHT 4
#define width_back 50
#define side_gap 0
#define letter_height 26
#define letter_width 14
#define logo 40
#define arrow_height 6
#define arrow_tail 5
#define headint_text_height TFT_HEIGHT_HEADING / 5
#define initialSensorsValue "XX"
#define barThickness 3
#define blinkTimeON 1000 // displayed text ON time
#define blinkTimeOFF 100 // displayed text OFF time
#define time_back_draw 255
#define time_back_wait 255

#define BACKLIGHT_NO_INTERACTION_TIME                                          \
  12000 // time to decrease backlight display if no user actions

// colour options
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
#define ORANGE 0xFD20

#define COLOUR_WARNING_TEXT ORANGE
#define COLOUR_MENU BLACK
#define COLOUR_BAR BLACK
#define COLOUR_MENU_TEXT WHITE
#define COLOUR_SELECTED WHITE
#define COLOUR_CHOSEN BLUE
#define COLOUR_HEADING BLUE
#define COLOUR_ARROW BLACK
#define COLOUR_BATTERY BLACK
#define COLOUR_BATTERY_LEFT BLACK
#define COLOUR_FRAME_BAR WHITE
#define COLOUR_LOADING_BAR RED
#define COLOUR_COMPLETED_BAR GREEN
#define introBackColor WHITE
#define introTextColor BLACK
#define transitionEffect BLACK

// pages number in UI
typedef enum {
  MAIN_MENU_PAGE = 1,
  ACTUATORS_PROGRESS_PAGE,
  SETTINGS_PAGE,
  CALIBRATION_SENSORS_PAGE,
  FIRST_POINT_CALIBRATION_PAGE,
  SECOND_POINT_CALIBRATION_PAGE,
  AUTO_CALIBRATION_PAGE,
  FINE_TUNE_CALIBRATION_PAGE,
} UI_PAGES;

typedef enum {
  EVENT_2G = 0,
  EVENT_WIFI,
  EVENT_SERVER_CONNECTION,
  EVENT_OTA_ONGOING,
} UI_EVENTS_ID;

typedef enum {
  EVENT_2G_UI_POS = 5,
  EVENT_SERVER_CONNECTION_UI_POS = EVENT_2G_UI_POS + 2 * letter_width,
  EVENT_WIFI_UI_POS = EVENT_SERVER_CONNECTION_UI_POS + letter_width,
  EVENT_OTA_ONGOING_UI_POS = EVENT_WIFI_UI_POS + letter_width,
} UI_EVENTS_ID_POS;

typedef enum {
  CONTROL_MODE_UI_ROW = 0,
  TEMPERATURE_UI_ROW,
  HUMIDITY_UI_ROW,
  LED_UI_ROW,
  START_UI_ROW,
  SETTINGS_UI_ROW,
} MAIN_MENU_UI;

typedef enum {
  SERIAL_NUMBER_UI_ROW = 0,
  LANGUAGE_UI_ROW,
  WIFI_EN_UI_ROW,
  CCID_UI_ROW,
  CALIBRATION_UI_ROW,
  DEFAULT_VALUES_UI_ROW,
  HW_TEST_UI_ROW,
} SETTINGS_MENU_UI;

typedef enum {
  AUTO_CALIB_UI_ROW = 0,
  FINE_TUNE_UI_ROW,
  TWO_POINT_CALIB_UI_ROW,
  RESET_CALIB_UI_ROW,
} CALIBRATION_MENU_UI;
