#include <PCA9557.h>
#include <lvgl.h>
//#include <DHT20.h>
#include <TAMC_GT911.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include "ui.h"
#include <buzzer.h>

// Temperature and humidity variables
double airTempValue, skinTempValue;
int humValue;
int selectedPanel = 0;
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
    char type[30];
    char description[100];
    bool state;
}; Alarm alarmList[10]; // Array to store up to 10 alarms


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
      cfg.freq_write  = 15000000;

      cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 40;
      cfg.hsync_pulse_width = 48;
      cfg.hsync_back_porch  = 40;
      
      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 1;
      cfg.vsync_pulse_width = 31;
      cfg.vsync_back_porch  = 13;

      cfg.pclk_active_neg   = 1;
      cfg.de_idle_high      = 0;
      cfg.pclk_idle_high    = 0;

      _bus_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = 800;
      cfg.memory_height = 480;
      cfg.panel_width  = 800;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);

  }
};
LGFX lcd;

// Touchscreen
#define TOUCH_SDA 19
#define TOUCH_SCL 20
#define TOUCH_INT 38
#define TOUCH_RST -1   // use -1 if you don't have reset pin

#define TOUCH_WIDTH  800
#define TOUCH_HEIGHT 480

TAMC_GT911 ts = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);

// UI
#define TFT_BL 2
int led;
//DHT20 dht20;
SPIClass& spi = SPI;

/* Screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf[800 * 480 / 15];
static lv_disp_drv_t disp_drv;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
 lcd.pushImageDMA(area->x1, area->y1, w, h,(lgfx::rgb565_t*)&color_p->full);
#else
  lcd.pushImageDMA(area->x1, area->y1, w, h,(lgfx::rgb565_t*)&color_p->full);
#endif

  lv_disp_flush_ready(disp);
}

/* Touch input read */
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
    lv_obj_set_style_bg_color(active, lv_color_make(220,240,255), LV_PART_MAIN);
    lv_obj_set_style_opa(active, LV_OPA_COVER, LV_PART_MAIN);

    // Inactive panel → dark gray
    lv_obj_set_style_bg_color(inactive, lv_color_make(100,100,100), LV_PART_MAIN);
    lv_obj_set_style_opa(inactive, LV_OPA_COVER, LV_PART_MAIN);
}

/* Switch callback for temperature and humidity */
void Switch_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_t * panel = NULL;

    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);

    if (obj == ui_Switch1) {  // TEMPERATURE SWITCH
        switchTemp = checked;
        tempSwitched = checked;
        panel = ui_Panel1;

        // If temperature is ON and no panel selected, activate Air panel by default
        if (checked && selectedPanel == 0) {
            selectedPanel = 1; // Air
            set_active_panel(ui_AirPanel, ui_SkinPanel);
        }

        // Enable or disable temperature arrows
        if (checked && selectedPanel != 0) {
            arrowsActive = true;
            lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownTemp, lv_color_make(220,240,255), LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpTemp, lv_color_make(220,240,255), LV_PART_MAIN);
        } else {
            arrowsActive = false;
            lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownTemp, lv_color_make(100,100,100), LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpTemp, lv_color_make(100,100,100), LV_PART_MAIN);
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
            lv_obj_set_style_bg_color(ui_ArrowDownHum, lv_color_make(220,240,255), LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpHum, lv_color_make(220,240,255), LV_PART_MAIN);
        } else {
            lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownHum, lv_color_make(100,100,100), LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpHum, lv_color_make(100,100,100), LV_PART_MAIN);
        }

        lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);
    }

    // If temperature is OFF, disable panels and arrows
    if (!tempSwitched) {
        selectedPanel = 0;   // no panel active
        arrowsActive = false;

        lv_obj_set_style_bg_color(ui_AirPanel, lv_color_make(100,100,100), LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_SkinPanel, lv_color_make(100,100,100), LV_PART_MAIN);
        lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(ui_ImgArrowUpTemp, LV_OBJ_FLAG_CLICKABLE);
    } 
    else {
        // If a panel is active, enable arrows
        if (selectedPanel != 0) {
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
            lv_obj_set_style_bg_color(panel, lv_color_make(220,240,255), LV_PART_MAIN);
            lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(panel, lv_color_make(100,100,100), LV_PART_MAIN);
            lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
        }
    }
}

/* Callback when Air panel is clicked */
void AirPanel_cb(lv_event_t * e) {
    if (!tempSwitched) return;
    selectedPanel = 1;  // Air panel selected
    arrowsActive = true;
    set_active_panel(ui_AirPanel, ui_SkinPanel);
}

/* Callback when Skin panel is clicked */
void SkinPanel_cb(lv_event_t * e) {
    if (!tempSwitched) return;
    selectedPanel = 2;  // Skin panel selected
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
    airTempValue = (double)random(200, 370) / 10.0;  // 20.0 to 36.9
    skinTempValue = (double)random(350, 376) / 10.0; // 35.0 to 37.5
    humValue = random(8, 20) * 5;  // Generates values from 40 to 95 in steps of 5

    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValue);
    lv_label_set_text(ui_TempAirDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValue);
    lv_label_set_text(ui_TempSkinDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", humValue);
    lv_label_set_text(ui_HumDesired, buffer);
}

/* Update labels for temperature and humidity */
void update_labels() {
    char buffer[10];
  
    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValue);
    lv_label_set_text(ui_TempAirDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValue);
    lv_label_set_text(ui_TempSkinDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%.d%%", humValue);
    lv_label_set_text(ui_HumDesired, buffer);
}

/* Setup arrow button callbacks for temperature */
void setup_arrow_callbacks() {
    // Up arrow (temperature)
    lv_obj_add_event_cb(ui_ImgArrowUpTemp, [](lv_event_t * e){
        if (!tempSwitched || !arrowsActive) return;
        if (selectedPanel == 1) { 
            airTempValue += 0.1; 
        } else if (selectedPanel == 2) { 
            skinTempValue += 0.1; 
        }
        update_labels();
    }, LV_EVENT_CLICKED, NULL);

    // Down arrow (temperature)
    lv_obj_add_event_cb(ui_ImgArrowDownTemp, [](lv_event_t * e){
        if (!tempSwitched || !arrowsActive) return;
        if (selectedPanel == 2) { 
            skinTempValue -= 0.1; 
        } else if (selectedPanel == 1) { 
            airTempValue -= 0.1; 
        }
        update_labels();
    }, LV_EVENT_CLICKED, NULL);
}

/* Setup arrow button callbacks for humidity */
void setup_arrow_hum_callbacks() {
    lv_obj_add_event_cb(ui_ImgArrowUpHum, [](lv_event_t * e){
        if (!humSwitched) return;
        humValue = min(95, humValue + 5);
        update_labels();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_ImgArrowDownHum, [](lv_event_t * e){
        if (!humSwitched) return;
        humValue = max(40, humValue - 5); 
        update_labels();
    }, LV_EVENT_CLICKED, NULL);
}

/* Animation callback for blinking alarms */
static void blink_cb(void * obj, int32_t v) {
    lv_obj_t * target = (lv_obj_t *)obj;
    lv_obj_set_style_opa(target, v, 0);
}

/* Start blinking animation on alarm panels */
void start_alarm_blink(lv_obj_t * obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 500);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, blink_cb);
    lv_anim_start(&a);
}

/* Update alarm panels according to alarmList */
void update_alarm_panels() {
    int pos = 0;

    for (int i = 0; i < sizeof(alarmList)/sizeof(alarmList[0]); i++) {
        if (alarmList[i].state) {
            switch (pos) {
                case 0:
                    lv_obj_clear_flag(ui_Alarm1Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm1Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm1Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm1Panel);
                    start_alarm_blink(ui_Alarm1Label);
                    break;
                case 1:
                    lv_obj_clear_flag(ui_Alarm2Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm2Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm2Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm2Panel);
                    start_alarm_blink(ui_Alarm2Label);
                    break;
                case 2:
                    lv_obj_clear_flag(ui_Alarm3Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm3Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm3Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm3Panel);
                    start_alarm_blink(ui_Alarm3Label);
                    break;
                case 3:
                    lv_obj_clear_flag(ui_Alarm4Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm4Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm4Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm4Panel);
                    start_alarm_blink(ui_Alarm4Label);
                    break;
            }
            pos++;
            if (pos >= 4) break;
        }
    }

    // Hide remaining panels if fewer than 4 alarms active
    if (pos < 4) {
        if (pos <= 0) { lv_obj_add_flag(ui_Alarm1Panel, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ui_Alarm1Label, LV_OBJ_FLAG_HIDDEN);}
        if (pos <= 1) { lv_obj_add_flag(ui_Alarm2Panel, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ui_Alarm2Label, LV_OBJ_FLAG_HIDDEN);}
        if (pos <= 2) { lv_obj_add_flag(ui_Alarm3Panel, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ui_Alarm3Label, LV_OBJ_FLAG_HIDDEN);}
        if (pos <= 3) { lv_obj_add_flag(ui_Alarm4Panel, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(ui_Alarm4Label, LV_OBJ_FLAG_HIDDEN);}
    }
}



void setup()
{
    // ===========================
    // Serial communication and pins initialization
    // ===========================
    Serial.begin(115200);          // Start serial port for debugging
    pinMode(38, OUTPUT);           // Set pin 38 as output (LED)
    digitalWrite(38, LOW);         // Initialize LED OFF

    Wire.begin(19, 20);            // Initialize I2C on pins 19 (SDA) and 20 (SCL)
    /*Wire1.begin(15, 16);          // Initialize I2C on pins 15 (SDA) and 16 (SCL)

    buzzerOn();
    delay(3000);
    buzzerOff();*/
   
    // ===========================
    // Display initialization
    // ===========================
    lcd.begin();                   // Initialize the display
    lcd.fillScreen(TFT_BLACK);     // Clear the screen with black color
    lcd.setTextSize(2);            // Default text size
    delay(200);                     // Short pause to stabilize

    lv_init();                     // Initialize LVGL library
    ts.begin();                     // Initialize touchscreen
    ts.setRotation(3);             // Set touchscreen orientation
    lcd.setRotation(2);             // Set display rotation

    // ===========================
    // LVGL draw buffer configuration
    // ===========================
    screenWidth = lcd.width();   
    screenHeight = lcd.height();
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 15);

    // ===========================
    // LVGL display driver configuration
    // ===========================
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;        // Horizontal resolution
    disp_drv.ver_res = screenHeight;       // Vertical resolution
    disp_drv.flush_cb = my_disp_flush;     // Function to send pixels to the display
    disp_drv.draw_buf = &draw_buf;         // Draw buffer
    lv_disp_drv_register(&disp_drv);       // Register the display driver in LVGL

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
    ledcSetup(1, 300, 8);          // Setup PWM channel 1, 300 Hz, 8 bits
    ledcAttachPin(TFT_BL, 1);      // Attach backlight pin to PWM channel
    ledcWrite(1, 255);             // Maximum brightness
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);     
    delay(500);
    digitalWrite(TFT_BL, HIGH);
#endif

    // ===========================
    // UI initialization
    // ===========================
    ui_init();                      // Initialize UI objects

    // ===========================
    // Initialize panel colors
    // ===========================
    lv_obj_set_style_bg_color(ui_Panel2, lv_color_make(220,240,255), LV_PART_MAIN); // blue
    lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel5, lv_color_make(220,240,255), LV_PART_MAIN); // blue
    lv_obj_set_style_opa(ui_Panel5, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel6, lv_color_make(220,240,255), LV_PART_MAIN); // blue
    lv_obj_set_style_opa(ui_Panel6, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel4, lv_color_make(220,240,255), LV_PART_MAIN); // blue
    lv_obj_set_style_opa(ui_Panel4, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel1, lv_color_make(100,100,100), LV_PART_MAIN); // grey
    lv_obj_set_style_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel3, lv_color_make(100,100,100), LV_PART_MAIN); // grey
    lv_obj_set_style_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_ArrowDownTemp, lv_color_make(100,100,100), LV_PART_MAIN); // temperature arrow panel grey
    lv_obj_set_style_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpTemp, lv_color_make(100,100,100), LV_PART_MAIN); // temperature arrow panel grey
    lv_obj_set_style_opa(ui_ArrowUpTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowDownHum, lv_color_make(100,100,100), LV_PART_MAIN); // humidity arrow panel grey
    lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ArrowUpHum, lv_color_make(100,100,100), LV_PART_MAIN); // humidity arrow panel grey
    lv_obj_set_style_opa(ui_ArrowUpHum, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_AirPanel, lv_color_make(100,100,100), LV_PART_MAIN); // Air panel grey
    lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_SkinPanel, lv_color_make(100,100,100), LV_PART_MAIN); // Skin panel grey
    lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

    lv_timer_handler();      // Process any initial LVGL tasks

    // ===========================
    // Temperature and humidity
    // ===========================
    init_values();           // Assign initial random values and update labels

    // Add event callbacks to switches
    lv_obj_add_event_cb(ui_Switch1, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Switch2, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Connect panel selection callbacks
    setup_panel_callbacks();
    // Connect arrow callbacks
    setup_arrow_callbacks();
    setup_arrow_hum_callbacks();

    // ===========================
    // Alarm configuration
    // ===========================
    alarmList[0].id = 1;
    strcpy(alarmList[0].type, "High Temperature");
    strcpy(alarmList[0].description, "Desc");
    alarmList[0].state = false;

    alarmList[1].id = 2;
    strcpy(alarmList[1].type, "Low Humidity");
    strcpy(alarmList[1].description, "Desc");
    alarmList[1].state = false;
}

void loop() {
    /*char DHT_buffer[6];
    int a = (int)dht20.getTemperature();
    int b = (int)dht20.getHumidity();
    snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", a);
    lv_label_set_text(ui_Label1, DHT_buffer);
    snprintf(DHT_buffer, sizeof(DHT_buffer), "%d", b);
    lv_label_set_text(ui_Label2, DHT_buffer);*/

    lv_timer_handler();
    delay(10);

    bool tempAlarm = skinTempValue > 37.0;
    bool humAlarm  = humValue < 60.0;

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
