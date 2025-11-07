#include "variables.h"

#include <PCA9557.h>
#include <lvgl.h>
#include "communication.h"
//#include <DHT20.h>
#include <TAMC_GT911.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include "ui.h"
#include <buzzer.h>
#include <Arduino.h>


// Temperature and humidity variables
double airTempValue, skinTempValue;
int humValue;
int selectedPanel = NO_PANEL_SELECTED;
bool switchTemp = false;
bool switchHum = false;
bool tempSwitched = false;
bool humSwitched = false;
bool arrowsActive = false;
//bool alarmActive = false;
bool prevTempAlarm = false;
bool prevHumAlarm = false;

struct Alarm
{
    int id;
    char type[ALARM_TYPE_LEN];
    char description[ALARM_DESC_LEN];
    bool state;
}; Alarm alarmList[MAX_ALARMS]; // Array to store up to MAX_ALARMS alarms


class LGFX : public lgfx::LGFX_Device
{
public:

  lgfx::Bus_RGB     _bus_instance;
  lgfx::Panel_RGB   _panel_instance;

  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;
      
      cfg.pin_d0  = GPIO_NUM_15; // B0
      cfg.pin_d1  = GPIO_NUM_7;  // B1
      cfg.pin_d2  = GPIO_NUM_6;  // B2
      cfg.pin_d3  = GPIO_NUM_5;  // B3
      cfg.pin_d4  = GPIO_NUM_4;  // B4
      
      cfg.pin_d5  = GPIO_NUM_9;  // G0
      cfg.pin_d6  = GPIO_NUM_46; // G1
      cfg.pin_d7  = GPIO_NUM_3;  // G2
      cfg.pin_d8  = GPIO_NUM_8;  // G3
      cfg.pin_d9  = GPIO_NUM_16; // G4
      cfg.pin_d10 = GPIO_NUM_1;  // G5
      
      cfg.pin_d11 = GPIO_NUM_14; // R0
      cfg.pin_d12 = GPIO_NUM_21; // R1
      cfg.pin_d13 = GPIO_NUM_47; // R2
      cfg.pin_d14 = GPIO_NUM_48; // R3
      cfg.pin_d15 = GPIO_NUM_45; // R4

      cfg.pin_henable = GPIO_NUM_41;
      cfg.pin_vsync   = GPIO_NUM_40;
      cfg.pin_hsync   = GPIO_NUM_39;
      cfg.pin_pclk    = GPIO_NUM_0;
      cfg.freq_write  = CFG_FREQ_WRITE;

      cfg.hsync_polarity    = CFG_HSYNC_POLARITY;
      cfg.hsync_front_porch = CFG_HSYNC_FRONT_PORCH;
      cfg.hsync_pulse_width = CFG_HSYNC_PULSE_WIDTH;
      cfg.hsync_back_porch  = CFG_HSYNC_BACK_PORCH;
      
      cfg.vsync_polarity    = CFG_VSYNC_POLARITY;
      cfg.vsync_front_porch = CFG_VSYNC_FRONT_PORCH;
      cfg.vsync_pulse_width = CFG_VSYNC_PULSE_WIDTH;
      cfg.vsync_back_porch  = CFG_VSYNC_BACK_PORCH;

      cfg.pclk_active_neg   = CFG_PCLK_ACTIVE_NEG;
      cfg.de_idle_high      = CFG_DE_IDLE_HIGH;
      cfg.pclk_idle_high    = CFG_PCLK_IDLE_HIGH;

      _bus_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = CFG_MEMORY_WIDTH;
      cfg.memory_height = CFG_MEMORY_HEIGHT;
      cfg.panel_width  = CFG_PANEL_WIDTH;
      cfg.panel_height = CFG_PANEL_HEIGHT;
      cfg.offset_x = CFG_OFFSET_X;
      cfg.offset_y = CFG_OFFSET_Y;
      _panel_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);

  }
};
LGFX lcd;


// Touchscreen
#define TOUCH_SDA TOUCH_SDA_PIN
#define TOUCH_SCL TOUCH_SCL_PIN
#define TOUCH_INT TOUCH_INT_PIN
#define TOUCH_RST TOUCH_RST_PIN   // use -1 if you don't have reset pin

#define TOUCH_WIDTH  DISPLAY_WIDTH
#define TOUCH_HEIGHT DISPLAY_HEIGHT

TAMC_GT911 ts = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);

// UI
#define TFT_BL TFT_BL_PIN
int led;
//DHT20 dht20;
SPIClass& spi = SPI;

/* Screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf[DISPLAY_WIDTH * DISPLAY_HEIGHT / COLOR_DIVISOR];
static lv_disp_drv_t disp_drv;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{

  uint32_t w = (area->x2 - area->x1 + AREA_PIXEL_OFFSET);
  uint32_t h = (area->y2 - area->y1 + AREA_PIXEL_OFFSET);


//lcd.fillScreen(TFT_WHITE);
#if (LV_COLOR_16_SWAP != 0)
 lcd.pushImageDMA(area->x1, area->y1, w, h,(lgfx::rgb565_t*)&color_p->full);
#else
  lcd.pushImageDMA(area->x1, area->y1, w, h,(lgfx::rgb565_t*)&color_p->full);//
#endif

  lv_disp_flush_ready(disp);

}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    ts.read();
    if (ts.isTouched) 
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = ts.points[0].x;
        data->point.y = ts.points[0].y;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* Temperature and humidity panel styling */
void set_active_panel(lv_obj_t* active, lv_obj_t* inactive) {
    // Active panel → blue with full opacity
    lv_obj_set_style_bg_color(active, COLOR_PANEL_BLUE, LV_PART_MAIN);
    lv_obj_set_style_opa(active, LV_OPA_COVER, LV_PART_MAIN);

    // Inactive panel → dark gray
    lv_obj_set_style_bg_color(inactive, COLOR_PANEL_GRAY, LV_PART_MAIN);
    lv_obj_set_style_opa(inactive, LV_OPA_COVER, LV_PART_MAIN);
}

/* Switch callback for temperature and humidity */
void Switch_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);  // switch that triggered the event
    lv_obj_t * panel = NULL;

    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);

    if (obj == ui_Switch1) {  // TEMPERATURE SWITCH
        switchTemp = checked;
        tempSwitched = checked;
        panel = ui_Panel1;

        // If temperature is ON and no panel selected, activate Air panel by default
        if (checked && selectedPanel == NO_PANEL_SELECTED) {
            selectedPanel = AIR_PANEL_SELECTED; // Air
            set_active_panel(ui_AirPanel, ui_SkinPanel);
        }

        // Enable or disable temperature arrows
        if (checked && selectedPanel != NO_PANEL_SELECTED) {
            arrowsActive = true;
            lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_BLUE, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_BLUE, LV_PART_MAIN);
        } else {
            arrowsActive = false;
            lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
        }

        lv_obj_set_style_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);
    } 
    else if (obj == ui_Switch2) {  // HUMIDITY SWITCH
        switchHum = checked;
        humSwitched = checked;
        panel = ui_Panel3;

        if (checked) {
            lv_obj_add_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_BLUE, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_BLUE, LV_PART_MAIN);
        } else {
            lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
        }

        lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);
    }

    // If temperature is OFF, disable panels and arrows
    if (!tempSwitched) {
        selectedPanel = NO_PANEL_SELECTED;   // no panel active
        arrowsActive = false;

        lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    } 
    else {
        // If a panel is active, enable arrows
        if (selectedPanel != NO_PANEL_SELECTED) {
            arrowsActive = true;
            lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
        } else {
            arrowsActive = false;
            lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    // Change background color of associated panel
    if (panel != NULL) {
        if (checked) {
            lv_obj_set_style_bg_color(panel, COLOR_PANEL_BLUE, LV_PART_MAIN);
            lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(panel, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
        }
    }
}

/* Callback when Air panel is clicked */
void AirPanel_cb(lv_event_t * e) {
    if (!tempSwitched) return;
    selectedPanel = AIR_PANEL_SELECTED;  // Air panel selected
    arrowsActive = true;
    set_active_panel(ui_AirPanel, ui_SkinPanel);
}

/* Callback when Skin panel is clicked */
void SkinPanel_cb(lv_event_t * e) {
    if (!tempSwitched) return;
    selectedPanel = SKIN_PANEL_SELECTED;  // Skin panel selected
    arrowsActive = true;
    set_active_panel(ui_SkinPanel, ui_AirPanel);
}

/* Setup panel click callbacks */
void setup_panel_callbacks() {
    lv_obj_add_event_cb(ui_AirPanel, AirPanel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_SkinPanel, SkinPanel_cb, LV_EVENT_CLICKED, NULL);
}

/* Initialize random values for temperature and humidity */
void init_values() {
    airTempValue = (double)random(RAND_AIR_MIN, RAND_AIR_MAX) / (double)TEMP_DIVISOR;  // 20.0 to 36.9 equivalent
    skinTempValue = (double)random(RAND_SKIN_MIN, RAND_SKIN_MAX) / (double)TEMP_DIVISOR; // 35.0 to 37.5 equivalent
    humValue = random(RAND_HUM_MIN, RAND_HUM_MAX) * HUM_STEP;  // Generates values from HUM_MIN to HUM_MAX in steps of HUM_STEP

    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValue);
    lv_label_set_text(ui_TempAirDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValue);
    lv_label_set_text(ui_TempSkinDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", humValue);
    lv_label_set_text(ui_HumDesired, buffer);
}

/* Update labels for temperature and humidity */
void update_labels() {
    char buffer[BUFFER_SIZE];
  
    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValue);
    lv_label_set_text(ui_TempAirDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValue);
    lv_label_set_text(ui_TempSkinDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", humValue);
    lv_label_set_text(ui_HumDesired, buffer);
}

/* Setup arrow button callbacks for temperature */
void setup_arrow_callbacks() {
    // Up arrow (temperature)
    lv_obj_add_event_cb(ui_ImgArrowUpTemp, [](lv_event_t * e){
        if (!tempSwitched || !arrowsActive) return;
        if (selectedPanel == AIR_PANEL_SELECTED) { 
            airTempValue += TEMP_INCREMENT; 
        } else if (selectedPanel == SKIN_PANEL_SELECTED) { 
            skinTempValue += TEMP_INCREMENT; 
        }
        update_labels();
    }, LV_EVENT_CLICKED, NULL);

    // Down arrow (temperature)
    lv_obj_add_event_cb(ui_ImgArrowDownTemp, [](lv_event_t * e){
        if (!tempSwitched || !arrowsActive) return;
        if (selectedPanel == SKIN_PANEL_SELECTED) { 
            skinTempValue -= TEMP_INCREMENT; 
        } else if (selectedPanel == AIR_PANEL_SELECTED) { 
            airTempValue -= TEMP_INCREMENT; 
        }
        update_labels();
    }, LV_EVENT_CLICKED, NULL);
}

/* Setup arrow button callbacks for humidity */
void setup_arrow_hum_callbacks() {
    lv_obj_add_event_cb(ui_ImgArrowUpHum, [](lv_event_t * e){
        if (!humSwitched) return;
        humValue = min(HUM_MAX, humValue + HUM_STEP);
        update_labels();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_ImgArrowDownHum, [](lv_event_t * e){
        if (!humSwitched) return;
        humValue = max(HUM_MIN, humValue - HUM_STEP); 
        update_labels();
    }, LV_EVENT_CLICKED, NULL);
}

/* Animation callback for blinking alarms */
static void blink_cb(void * obj, int32_t v) {
    lv_obj_t * target = (lv_obj_t *)obj;
    lv_obj_set_style_opa(target, v, STYLE_SELECTOR_DEFAULT);
}

/* Start blinking animation on alarm panels */
void start_alarm_blink(lv_obj_t * obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, ANIM_TIME_MS);
    lv_anim_set_playback_time(&a, ANIM_PLAYBACK_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, blink_cb);
    lv_anim_start(&a);
}

/* Update alarm panels according to alarmList */
void update_alarm_panels() {
    int pos = NUM_ALARMA_0; // Position index for active alarms

    for (int i = 0; i < MAX_ALARMS; i++) {
        if (alarmList[i].state) {
            switch (pos) {
                case NUM_ALARMA_0:
                    lv_obj_clear_flag(ui_Alarm1Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm1Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm1Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm1Panel);
                    start_alarm_blink(ui_Alarm1Label);
                    break;
                case NUM_ALARMA_1:
                    lv_obj_clear_flag(ui_Alarm2Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm2Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm2Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm2Panel);
                    start_alarm_blink(ui_Alarm2Label);
                    break;
                case NUM_ALARMA_2:
                    lv_obj_clear_flag(ui_Alarm3Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm3Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm3Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm3Panel);
                    start_alarm_blink(ui_Alarm3Label);
                    break;
                case NUM_ALARMA_3:
                    lv_obj_clear_flag(ui_Alarm4Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm4Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm4Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm4Panel);
                    start_alarm_blink(ui_Alarm4Label);
                    break;
            }
            pos++;
            if (pos >= MAX_ALARM_DISPLAY) break;
        }
    }

    // Hide remaining panels if fewer than MAX_ALARM_DISPLAY alarms active
    if (pos < MAX_ALARM_DISPLAY) {
        if (pos <= NUM_ALARMA_0) { lv_obj_add_flag(ui_Alarm1Panel, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ui_Alarm1Label, LV_OBJ_FLAG_HIDDEN);}
        if (pos <= NUM_ALARMA_1) { lv_obj_add_flag(ui_Alarm2Panel, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ui_Alarm2Label, LV_OBJ_FLAG_HIDDEN);}
        if (pos <= NUM_ALARMA_2) { lv_obj_add_flag(ui_Alarm3Panel, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ui_Alarm3Label, LV_OBJ_FLAG_HIDDEN);}
        if (pos <= NUM_ALARMA_3) { lv_obj_add_flag(ui_Alarm4Panel, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ui_Alarm4Label, LV_OBJ_FLAG_HIDDEN);}
    }
}



void setup()
{
    // ===========================
    // Serial communication and pins initialization
    // ===========================
    Serial.begin(SERIAL_BAUD);          // Start serial port for debugging
    pinMode(LED_PIN, OUTPUT);           // Set LED pin as output
    digitalWrite(LED_PIN, LOW);         // Initialize LED OFF

    Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);            // Initialize I2C on touch pins
    /*Wire1.begin(PIN_D0, PIN_D1);          // Initialize I2C on alternate pins (originally commented)

    buzzerOn();
    delay(DELAY_BUZZ_MS);
    buzzerOff();*/
   
    // ===========================
    // Display initialization
    // ===========================
    lcd.begin();                   // Initialize the display
    lcd.fillScreen(TFT_BLACK);     // Clear the screen with black color
    lcd.setTextSize(2);            // Default text size
    delay(DELAY_SHORT_MS);                     // Short pause to stabilize

    lv_init();                     // Initialize LVGL library
    ts.begin();                     // Initialize touchscreen
    ts.setRotation(TOUCH_ROTATION);             // Set touchscreen orientation
    lcd.setRotation(LCD_ROTATION);             // Set display rotation

    // ===========================
    // LVGL draw buffer configuration
    // ===========================
    screenWidth = lcd.width();   
    screenHeight = lcd.height();
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / COLOR_DIVISOR);

    // ===========================
    // LVGL display driver configuration
    // ===========================
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;        // Horizontal resolution
    disp_drv.ver_res = screenHeight;       // Vertical resolution
    disp_drv.flush_cb = my_disp_flush;     // Function to send pixels to the display
    disp_drv.draw_buf = &draw_buf;         // Draw buffer
    lv_disp_drv_register(&disp_drv);       // Register the driver

    // ===========================
    // Input device (touch) initialization
    // ===========================
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;    // Pointer type (touch)
    indev_drv.read_cb = my_touchpad_read;      // Callback to read touch input
    lv_indev_drv_register(&indev_drv);        // Register input device in LVGL

    // ===========================
    // Display backlight configuration
    // ===========================
#ifdef TFT_BL
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);          // Setup PWM channel
    ledcAttachPin(TFT_BL, PWM_CHANNEL);      // Attach backlight pin to PWM channel
    ledcWrite(PWM_CHANNEL, BRIGHTNESS_MAX);             // Brightness max
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);     
    delay(DELAY_BACKLIGHT_MS);
    digitalWrite(TFT_BL, HIGH);
#endif

    // ===========================
    // UI initialization
    // ===========================
    ui_init();                      // Initialize UI objects

    // ===========================
    // Initialize panel colors
    // ===========================
    lv_obj_set_style_bg_color(ui_Panel2, COLOR_PANEL_BLUE, LV_PART_MAIN); // blue
    lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel5, COLOR_PANEL_BLUE, LV_PART_MAIN); // blue
    lv_obj_set_style_opa(ui_Panel5, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel6, COLOR_PANEL_BLUE, LV_PART_MAIN); // blue
    lv_obj_set_style_opa(ui_Panel6, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel4, COLOR_PANEL_BLUE, LV_PART_MAIN); // blue
    lv_obj_set_style_opa(ui_Panel4, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel1, COLOR_PANEL_GRAY, LV_PART_MAIN); // grey
    lv_obj_set_style_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel3, COLOR_PANEL_GRAY, LV_PART_MAIN); // grey
    lv_obj_set_style_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY, LV_PART_MAIN); // temp arrow panel grey
    lv_obj_set_style_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, COLOR_PANEL_GRAY, LV_PART_MAIN); // temp arrow panel grey
    lv_obj_set_style_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY, LV_PART_MAIN); // hum arrow panel grey
    lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, COLOR_PANEL_GRAY, LV_PART_MAIN); // hum arrow panel grey
    lv_obj_set_style_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN); // Air panel grey
    lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN); // Skin panel grey
    lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

    lv_timer_handler();      // Process any initial LVGL tasks

    // ===========================
    // Temperature and humidity
    // ===========================
    init_values();           // Assign initial random values and update labels

    lv_obj_add_event_cb(ui_Switch1, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Switch2, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Connect panel selection callbacks
    setup_panel_callbacks();
    // Connect callbacks for arrows
    setup_arrow_callbacks();
    setup_arrow_hum_callbacks();

    // ===========================
    // Alarm configuration
    // ===========================
    alarmList[0].id = 1;
    strncpy(alarmList[0].type, "High Temperature", ALARM_TYPE_LEN);
    strncpy(alarmList[0].description, "Desc", ALARM_DESC_LEN);
    alarmList[0].state = false;

    alarmList[1].id = 2;
    strncpy(alarmList[1].type, "Low Humidity", ALARM_TYPE_LEN);
    strncpy(alarmList[1].description, "Desc", ALARM_DESC_LEN);
    alarmList[1].state = false;
}

void loop() {

    /*char DHT_buffer[DHT_BUFFER_SIZE];
    int a = (int)dht20.getTemperature();
    int b = (int)dht20.getHumidity();
    snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", a);
    lv_label_set_text(ui_Label1, DHT_buffer);
    snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", b);
    lv_label_set_text(ui_Label2, DHT_buffer);*/


    lv_timer_handler();
    delay(LOOP_DELAY_MS);

    bool tempAlarm = skinTempValue > TEMP_ALARM_THRESHOLD;
    bool humAlarm  = humValue < HUM_ALARM_THRESHOLD;

    if(tempAlarm != prevTempAlarm) {
        alarmList[0].state = tempAlarm;
        update_alarm_panels();
        prevTempAlarm = tempAlarm;
    }
    if(humAlarm != prevHumAlarm) {
        alarmList[1].state = humAlarm;
        update_alarm_panels();
        prevHumAlarm = humAlarm;
    }

    /*if (alarmActive) {
        buzzerOn();
    } else {
        buzzerOff();
    }*/
}
