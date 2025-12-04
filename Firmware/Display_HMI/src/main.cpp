#include "main.h"

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
double airTempValueDetected = 0.00, skinTempValueDetected = 0.00;
int humValue;
int humValueDetected = 0;
int selectedPanel = NO_PANEL_SELECTED;
int lastSelectedPanel = NO_PANEL_SELECTED;
bool skinPanelEnabled = false;
bool switchTemp = false;
bool switchHum = false;
bool tempSwitched = false;
bool humSwitched = false;
bool arrowsActive = false;
bool alarmActive = false;
bool prevTempAlarm = false;
bool prevHumAlarm = false;
int alarmSlotToIndex[MAX_ALARM_DISPLAY] = { -1, -1, -1, -1 };


bool wifiVisible = false;
bool LanguagesVisible = false;

lv_chart_series_t * tempSeries = NULL;
lv_chart_series_t * humSeries  = NULL;






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
    lv_obj_set_style_bg_color(active, COLOR_PANEL_WHITE, LV_PART_MAIN);
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

        if (checked) {  // Temperature switch turned ON
            // ==== FORCE HUM OFF ====
            lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
            switchHum   = false;
            humSwitched = false;

            // Deactivate humidity arrows
            lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpHum,   LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpHum,   COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_ArrowUpHum,   LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_Panel3,   COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_Panel3,   LV_OPA_COVER, LV_PART_MAIN);

            // Gray out both panels initially
            if (lastSelectedPanel == AIR_PANEL_SELECTED) {
                selectedPanel = AIR_PANEL_SELECTED;
                set_active_panel(ui_AirPanel, ui_SkinPanel);
                hmi_msg.controlMode = CONTROL_AIR;
            } 
            else if (lastSelectedPanel == SKIN_PANEL_SELECTED) {
                selectedPanel = SKIN_PANEL_SELECTED;
                set_active_panel(ui_SkinPanel, ui_AirPanel);
                hmi_msg.controlMode = CONTROL_SKIN;
            } 
            else {  // No previous panel, default to Air
                selectedPanel = AIR_PANEL_SELECTED;
                set_active_panel(ui_AirPanel, ui_SkinPanel);
                hmi_msg.controlMode = CONTROL_AIR;
            }

            // Enable temperature arrows
            arrowsActive = true;
            lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgArrowUpTemp,   LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_WHITE, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpTemp,   COLOR_PANEL_WHITE, LV_PART_MAIN);
        } 
        else {  // Temperature switch turned OFF
            arrowsActive = false;

            // Disable temperature arrows
            lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpTemp,   LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpTemp,   COLOR_PANEL_GRAY, LV_PART_MAIN);

            // Gray out both panels
            lv_obj_set_style_bg_color(ui_AirPanel,  COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_AirPanel,  LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);
        }
    } 
    else if (obj == ui_Switch2) {  // HUMIDITY SWITCH
        switchHum = checked;
        humSwitched = checked;
        panel = ui_Panel3;

        if (checked) {
            // ==== FORCE TEMP OFF ====
            lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
            switchTemp   = false;
            tempSwitched = false;
            arrowsActive = false;

            // Deactivate temperature arrows
            lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpTemp,   LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownTemp, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpTemp,   COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_ArrowDownTemp, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_ArrowUpTemp,   LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_Panel1,  COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_Panel1,   LV_OPA_COVER, LV_PART_MAIN);

            // Gray out air/skin panels
            lv_obj_set_style_bg_color(ui_AirPanel,  COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_AirPanel,  LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);


            // Enable humidity arrows
            lv_obj_add_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgArrowUpHum,   LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_WHITE, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpHum,   COLOR_PANEL_WHITE, LV_PART_MAIN);
        } else {
            // Humidity OFF
            lv_obj_clear_flag(ui_ImgArrowDownHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpHum,   LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_ArrowDownHum, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_ArrowUpHum,   COLOR_PANEL_GRAY, LV_PART_MAIN);

            lv_obj_set_style_opa(ui_ArrowDownHum, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_ArrowUpHum,   LV_OPA_COVER, LV_PART_MAIN);
        }
    }
    else if (obj == ui_Switch3) {  // PHOTOTHERAPY SWITCH
        bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
        hmi_msg.phototherapyMode = checked ? PHOTOTHERAPY_ON : PHOTOTHERAPY_OFF;
        hmi_msg.shouldSendData = true;
    }
    else if (obj == ui_Switch4) {    // SKIN BLOCK SWITCH
        bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
        skinPanelEnabled = checked;

        if (checked) {
            // show container of skin
            lv_obj_clear_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);

            lv_obj_set_style_bg_color(ui_SkinPanelCont, COLOR_PANEL_WHITE, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_SkinPanelCont, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            // Hide container of skin
            lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
            
            if (selectedPanel == SKIN_PANEL_SELECTED) {
                selectedPanel     = AIR_PANEL_SELECTED;
                lastSelectedPanel = selectedPanel;

                // Visualmente: Air activo, Skin inactivo
                set_active_panel(ui_AirPanel, ui_SkinPanel);

                // Lógica de control: pasamos a controlar aire
                if (tempSwitched) {                 // solo tiene sentido si la temp está ON
                    hmi_msg.controlMode = CONTROL_AIR;
                }
            }
        }
        // hmi_msg.skinBlockEnabled = checked; (si lo quieres mandar)
    }

    // If temperature is OFF, disable panels and arrows (por si acaso)
    if (!tempSwitched) {
        arrowsActive = false;

        lv_obj_set_style_bg_color(ui_AirPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_SkinPanel, COLOR_PANEL_GRAY, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_AirPanel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_SkinPanel, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(ui_ImgArrowUpTemp,   LV_OBJ_FLAG_CLICKABLE);
    } 
    else {
        // If a panel is active, enable arrows
        if (selectedPanel != NO_PANEL_SELECTED) {
            arrowsActive = true;
            lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgArrowUpTemp,   LV_OBJ_FLAG_CLICKABLE);
        } else {
            arrowsActive = false;
            lv_obj_clear_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgArrowUpTemp,   LV_OBJ_FLAG_CLICKABLE);
        }
    }

    // Change background color of associated panel
    if (panel != NULL) {
        if (checked) {
            lv_obj_set_style_bg_color(panel, COLOR_PANEL_WHITE, LV_PART_MAIN);
            lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(panel, COLOR_PANEL_GRAY, LV_PART_MAIN);
            lv_obj_set_style_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
        }
    }

    // --- Actuation mode selection logic (ya con exclusión asegurada) ---
    if (switchTemp && switchHum) {
        // En teoría no debería ocurrir, pero por seguridad:
        hmi_msg.actuation = ACTUATION_TEMPERATURE; // o el que prefieras
    } else if (switchTemp) {
        hmi_msg.actuation = ACTUATION_TEMPERATURE;
    } else if (switchHum) {
        hmi_msg.actuation = ACTUATION_HUMIDITY;
    } else {
        hmi_msg.actuation = ACTUATION_NONE;
    }

    hmi_msg.desiredAirTemperature  = airTempValue;
    hmi_msg.desiredSkinTemperature = skinTempValue;
    hmi_msg.desiredHumidity        = humValue;
    hmi_msg.shouldSendData = true;
}

/* Callback when Wifi button is clicked */
void WifiButton_cb(lv_event_t * e) {

    bool wifiHidden = lv_obj_has_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);

    if (wifiHidden) {
        // Show Wifi
        lv_obj_clear_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
        wifiVisible = true;

        // Hide Languages
        lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
        LanguagesVisible = false;
    }
    else {
        // Hide Wifi (none visible)
        lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
        wifiVisible = false;
    }
}

/* Callback when Language button is clicked */
void LanguageButton_cb(lv_event_t * e) {

    bool langHidden = lv_obj_has_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);

    if (langHidden) {
        // Show Languages
        lv_obj_clear_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
        LanguagesVisible = true;

        // Hide Wifi
        lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
        wifiVisible = false;
    }
    else {
        // Hide Languages (none visible)
        lv_obj_add_flag(ui_LanguagesDropDown, LV_OBJ_FLAG_HIDDEN);
        LanguagesVisible = false;
    }
}


    /* Callback al pulsar una TextArea (SSID / Password) */
void TextArea_focus_cb(lv_event_t * e) {
    lv_obj_t * ta = lv_event_get_target(e);  // TextArea that triggered the event

    // Show keyboard
    lv_obj_clear_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);

    // Associate keyboard with this TextArea
    lv_keyboard_set_textarea(ui_Keyboard1, ta);
}

/* Callback of the keyboard: hide when OK or Cancel is pressed */
void Keyboard_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        // Disassociate textarea
        lv_keyboard_set_textarea(ui_Keyboard1, NULL);
        // Hide keyboard
        lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Callback when Air panel is clicked */
void AirPanel_cb(lv_event_t * e) {
    if (!tempSwitched) return;
    selectedPanel = AIR_PANEL_SELECTED;  // Air panel selected
    lastSelectedPanel = selectedPanel;

    arrowsActive = true;
    set_active_panel(ui_AirPanel, ui_SkinPanel);

    // --- Show only the Air container ---
    lv_obj_clear_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
    
    hmi_msg.controlMode = CONTROL_AIR;
    hmi_msg.desiredAirTemperature = airTempValue;
    hmi_msg.desiredSkinTemperature = skinTempValue;
    hmi_msg.desiredHumidity = humValue;
    hmi_msg.shouldSendData = true;
}

/* Callback when Skin panel is clicked */
void SkinPanel_cb(lv_event_t * e) {
    if (!tempSwitched) return;
    selectedPanel = SKIN_PANEL_SELECTED;  // Skin panel selected
    lastSelectedPanel = selectedPanel;

    arrowsActive = true;
    set_active_panel(ui_SkinPanel, ui_AirPanel);

    // --- Show only the Skin container ---
    lv_obj_clear_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_AirTempBarCont, LV_OBJ_FLAG_HIDDEN);


    hmi_msg.controlMode = CONTROL_SKIN;
    hmi_msg.desiredAirTemperature = airTempValue;
    hmi_msg.desiredSkinTemperature = skinTempValue;
    hmi_msg.desiredHumidity = humValue;
    hmi_msg.shouldSendData = true;
}

/* Setup panel click callbacks */
void setup_panel_callbacks() {
    lv_obj_add_event_cb(ui_AirPanel, AirPanel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_SkinPanel, SkinPanel_cb, LV_EVENT_CLICKED, NULL);
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

    if (airTempValueDetected != 0 || skinTempValueDetected != 0 || humValueDetected != 0) {
        snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValueDetected);
        lv_label_set_text(ui_TempAirDetected, buffer);
        lv_label_set_text(ui_TempAirDetectedRight, buffer);

        snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValueDetected);
        lv_label_set_text(ui_TempSkinDetected, buffer);
        lv_label_set_text(ui_TempSkinDetectedRight, buffer);

        snprintf(buffer, sizeof(buffer), "%d%%", humValueDetected);
        lv_label_set_text(ui_HumDetected, buffer);
        lv_label_set_text(ui_HumDetectedRight, buffer);


        double tAir  = airTempValueDetected;
        double tSkin = skinTempValueDetected;

        // Air
        int airBar;
        if (tAir <= 20.0)      airBar = 0;
        else if (tAir >= 40.0) airBar = 20;
        else                   airBar = (int)round(tAir - 20.0);

        // Skin
        int skinBar;
        if (tSkin <= 20.0)      skinBar = 0;
        else if (tSkin >= 40.0) skinBar = 20;
        else                    skinBar = (int)round(tSkin - 20.0);

        // Humidity
        int humBar = constrain(humValueDetected, 0, 100);

        lv_bar_set_value(ui_AirTempBar,  airBar,  LV_ANIM_OFF);
        lv_bar_set_value(ui_SkinTempBar, skinBar, LV_ANIM_OFF);
        lv_bar_set_value(ui_HumBar,      humBar,  LV_ANIM_OFF);

    
    }
}

/* Initialize random values for temperature and humidity */
void init_values() {
    //DESIRED VALUES
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

    //DETECTED VALUES
    airTempValueDetected  = (double)random(RAND_AIR_MIN, RAND_AIR_MAX) / (double)TEMP_DIVISOR;  // 20.0 to 36.9 equivalent
    skinTempValueDetected = (double)random(RAND_SKIN_MIN, RAND_SKIN_MAX) / (double)TEMP_DIVISOR; // 35.0 to 37.5 equivalent
    humValueDetected = random(RAND_HUM_MIN, RAND_HUM_MAX) * HUM_STEP;  // Generates values from HUM_MIN to HUM_MAX in steps of HUM_STEP

    snprintf(buffer, sizeof(buffer), "%.1f°C", airTempValueDetected);
    lv_label_set_text(ui_TempAirDetected, buffer);
    lv_label_set_text(ui_TempAirDetectedRight, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f°C", skinTempValueDetected);
    lv_label_set_text(ui_TempSkinDetected, buffer);
    lv_label_set_text(ui_TempSkinDetectedRight, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", humValueDetected);
    lv_label_set_text(ui_HumDetected, buffer);
    lv_label_set_text(ui_HumDetectedRight, buffer);

    update_labels();

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

        hmi_msg.desiredAirTemperature = airTempValue;
        hmi_msg.desiredSkinTemperature = skinTempValue;
        hmi_msg.desiredHumidity = humValue;
        hmi_msg.shouldSendData = true;
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

        hmi_msg.desiredAirTemperature = airTempValue;
        hmi_msg.desiredSkinTemperature = skinTempValue;
        hmi_msg.desiredHumidity = humValue;
        hmi_msg.shouldSendData = true;
    }, LV_EVENT_CLICKED, NULL);
}

/* Setup arrow button callbacks for humidity */
void setup_arrow_hum_callbacks() {
    lv_obj_add_event_cb(ui_ImgArrowUpHum, [](lv_event_t * e){
        if (!humSwitched) return;
        humValue = min(HUM_MAX, humValue + HUM_STEP);
        update_labels();

        hmi_msg.desiredAirTemperature = airTempValue;
        hmi_msg.desiredSkinTemperature = skinTempValue;
        hmi_msg.desiredHumidity = humValue;
        hmi_msg.shouldSendData = true;
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_ImgArrowDownHum, [](lv_event_t * e){
        if (!humSwitched) return;
        humValue = max(HUM_MIN, humValue - HUM_STEP); 
        update_labels();

        hmi_msg.desiredAirTemperature = airTempValue;
        hmi_msg.desiredSkinTemperature = skinTempValue;
        hmi_msg.desiredHumidity = humValue;
        hmi_msg.shouldSendData = true;
    }, LV_EVENT_CLICKED, NULL);
}

/* Animation callback for blinking alarms */
static void blink_cb(void * obj, int32_t v) {
    lv_obj_t * target = (lv_obj_t *)obj;
    lv_obj_set_style_opa(target, v, STYLE_SELECTOR_DEFAULT);
}

/* Start blinking animation on alarm panels */
void start_alarm_blink(lv_obj_t * obj) {

    lv_anim_del(obj, blink_cb);


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

    // Resetear mapa contenedor → índice de alarma
    for (int s = 0; s < MAX_ALARM_DISPLAY; s++) {
        alarmSlotToIndex[s] = -1;
    }

    int activeCount = 0;

    for (int i = 0; i < MAX_ALARMS; i++) {
        if (alarmList[i].state) {
            activeCount++;
            switch (pos) {
                case NUM_ALARMA_0:
                    alarmSlotToIndex[0] = i;
                    lv_obj_clear_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm1Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm1Cont);
                    break;
                case NUM_ALARMA_1:
                    alarmSlotToIndex[1] = i;
                    lv_obj_clear_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm2Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm2Cont);
                    break;
                case NUM_ALARMA_2:
                    alarmSlotToIndex[2] = i;
                    lv_obj_clear_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm3Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm3Cont);
                    break;
                case NUM_ALARMA_3:
                    alarmSlotToIndex[3] = i;
                    lv_obj_clear_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm4Label, alarmList[i].type);
                    start_alarm_blink(ui_Alarm4Cont);
                    break;
            }
            pos++;
            if (pos >= MAX_ALARM_DISPLAY) break;
        }

        
    }

    // ----- Global alarm state -----
        alarmActive = (activeCount > 0);

        if (alarmActive) {
            lv_obj_clear_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN); // show
        } else {
            lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);   // hide
        }

         // ----- Indicator on main screen: Panel10 + NumAlarm -----
        if (alarmActive) {
            // Text with the number of alarms
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", activeCount);
            lv_label_set_text(ui_NumAlarm, buf);

            // Show them and start animations
            lv_obj_clear_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);

            // Make them blink in unison
            start_alarm_blink(ui_Panel10);
            start_alarm_blink(ui_NumAlarm);
            start_alarm_blink(ui_AlarmButton);
        } else {
            // No alarms: hide and remove animations
            lv_obj_add_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);

            lv_anim_del(ui_Panel10, blink_cb);
            lv_anim_del(ui_NumAlarm, blink_cb);
            lv_anim_del(ui_AlarmButton, blink_cb);
            lv_obj_set_style_opa(ui_Panel10, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_opa(ui_NumAlarm, LV_OPA_COVER, LV_PART_MAIN);
        }

    // Hide remaining panels if fewer than MAX_ALARM_DISPLAY alarms active
    if (pos < MAX_ALARM_DISPLAY) {
        if (pos <= NUM_ALARMA_0) { lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN); }
        if (pos <= NUM_ALARMA_1) { lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN); }
        if (pos <= NUM_ALARMA_2) { lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN); }
        if (pos <= NUM_ALARMA_3) { lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN); }
    }
}

void show_alarm_detail_from_slot(int slot)
{
    if (slot < 0 || slot >= MAX_ALARM_DISPLAY) return;

    int idx = alarmSlotToIndex[slot];
    if (idx < 0) return;   // no hay alarma asignada a ese contenedor

    // Cambiar a la pestaña "View details" (segunda pestaña => índice 1)
    lv_tabview_set_act(ui_AlarmsTabview, 1, LV_ANIM_ON);

    // Poner la descripción de la alarma seleccionada
    lv_label_set_text(ui_AlarmDetailLabel, alarmList[idx].description);
}

void Alarm1Cont_cb(lv_event_t * e) { show_alarm_detail_from_slot(0); }
void Alarm2Cont_cb(lv_event_t * e) { show_alarm_detail_from_slot(1); }
void Alarm3Cont_cb(lv_event_t * e) { show_alarm_detail_from_slot(2); }
void Alarm4Cont_cb(lv_event_t * e) { show_alarm_detail_from_slot(3); }

void AlarmButton_cb(lv_event_t * e)
{
    // Change to the "Alarms" tab (first tab => index 0)
    lv_tabview_set_act(ui_AlarmsTabview, 0, LV_ANIM_ON);
}

void AlarmsTabview_cb(lv_event_t * e)
{
    lv_obj_t * tv = lv_event_get_target(e);
    uint16_t act = lv_tabview_get_tab_act(tv);

    // If we are on tab 0 ("Alarms"), clear the text area
    if (act == 0) {
        lv_label_set_text(ui_AlarmDetailLabel, "");
    }
}

void chart_add_temp_value(float temp)
{
    if (tempSeries == NULL) return;

    // Convert to lv_coord_t (int16)
    lv_chart_set_next_value(ui_TempChart, tempSeries, (lv_coord_t)temp);
}

void chart_add_hum_value(float hum)
{
    if (humSeries == NULL) return;

    // Ensure limits 0–100 %
    if (hum < 0)   hum = 0;
    if (hum > 100) hum = 100;

    lv_chart_set_next_value(ui_HumChart, humSeries, (lv_coord_t)hum);
}


void applyHMIData() {
    // -----------------------------
    // Update numeric values
    // -----------------------------
    
    airTempValueDetected  = ctrl_tel_msg.detectedAirTemperature;
    skinTempValueDetected = ctrl_tel_msg.detectedSkinTemperature;
    humValueDetected      = (int)ctrl_tel_msg.detectedHumidity;
    update_labels();

    // Add to charts
    //chart_add_hum_value((float)humValueDetected);
    //chart_add_skin_temp_value((float)skinTempValueDetected);
    chart_add_temp_value((float)airTempValueDetected);
    chart_add_hum_value((float)humValueDetected);
    // -----------------------------
    // Simulate switches according to 'actuation'
    // -----------------------------
    /*switch (hmi_msg.actuation) {
        case ACTUATION_NONE:
            lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
            lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
            break;
        case ACTUATION_TEMPERATURE:
            lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);
            lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
            break;
        case ACTUATION_HUMIDITY:
            lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);
            lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
            break;
        case ACTUATION_TEMP_AND_HUMIDITY:
            lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);
            lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
            break;
    }

    // -----------------------------
    // Trigger events to apply all logic
    // -----------------------------
    lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED, NULL);
    lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);

    // -----------------------------
    // Panels according to controlMode (can also use Switch_cb)
    // -----------------------------
    if (hmi_msg.controlMode == CONTROL_AIR) {
        selectedPanel = AIR_PANEL_SELECTED;
        lastSelectedPanel = selectedPanel;
        set_active_panel(ui_AirPanel, ui_SkinPanel);
    } else if (hmi_msg.controlMode == CONTROL_SKIN) {
        selectedPanel = SKIN_PANEL_SELECTED;
        lastSelectedPanel = selectedPanel;
        set_active_panel(ui_SkinPanel, ui_AirPanel);
    } else {
        selectedPanel = NO_PANEL_SELECTED;
        set_active_panel(ui_AirPanel, ui_SkinPanel);
    }

    // -----------------------------
    // Phototherapy switch
    // -----------------------------
    if (hmi_msg.phototherapyMode == PHOTOTHERAPY_ON) {
        lv_obj_add_state(ui_Switch3, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_Switch3, LV_STATE_CHECKED);
    }
    lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);*/

}

void processReceivedAlarm(const ControlBoard_Message_Alarm &alarm) {
    alarmActive = true;
    
    lv_obj_clear_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN); // Show mute button

    // Search for existing alarm by ID
    int index = -1;
    for (int i = 0; i < MAX_ALARMS; i++) {
        if (alarmList[i].id == alarm.id) {
            index = i;
            break;
        }
    }

    // If not found, search for a free slot
    if (index == -1) {
        for (int i = 0; i < MAX_ALARMS; i++) {
            if (alarmList[i].state == false) { // false = free
                index = i;
                break;
            }
        }
    }

    if (index != -1) {
        // Save/update alarm info
        alarmList[index].id = alarm.id;
        strncpy(alarmList[index].type, alarm.type, ALARM_TYPE_LEN);
        alarmList[index].type[ALARM_TYPE_LEN-1] = '\0';
        strncpy(alarmList[index].description, alarm.description, ALARM_DESC_LEN);
        alarmList[index].description[ALARM_DESC_LEN-1] = '\0';
        alarmList[index].state = alarm.state;

        // Update display
        update_alarm_panels();
    } else {
        log_w("No space to store new alarm ID=%d", alarm.id);
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

    Communication_Init();


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
    
    // Visual elements initial configuration
    // Bars ranges
    lv_bar_set_range(ui_AirTempBar,  0, 20);
    lv_bar_set_range(ui_SkinTempBar, 0, 20);

    // Humidity remains in 0..100
    lv_bar_set_range(ui_HumBar, 0, 100);


    // Mute alarm button callback:
    lv_obj_add_event_cb(ui_MuteAlarm, [](lv_event_t * e){
    // Activate alarm mute
        alarmActive = false;

        // Make button non-visible
        lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);

    
    }, LV_EVENT_CLICKED, NULL);

    // --- SKIN PANEL: hide at startup ---
    lv_obj_add_flag(ui_SkinPanelCont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SkinTempBarCont, LV_OBJ_FLAG_HIDDEN);

    
    lv_obj_add_flag(ui_MuteAlarm, LV_OBJ_FLAG_HIDDEN);



    // --- WIFI: hide at startup ---
    lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
    // --- KEYBOARD: hide at startup ---
    lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(ui_Keyboard1, NULL);


    // --- TextArea events for Wifi ---
    lv_obj_add_event_cb(ui_TextArea1, TextArea_focus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextArea2, TextArea_focus_cb, LV_EVENT_CLICKED, NULL);
    // --- KEYBOARD ---
    lv_obj_add_event_cb(ui_Keyboard1, Keyboard_cb, LV_EVENT_ALL, NULL);
    // Register Wifi button callback
    lv_obj_add_event_cb(ui_WifiButton, WifiButton_cb, LV_EVENT_CLICKED, NULL);

    // Register Language button callback
    lv_obj_add_event_cb(ui_LanguagesButton, LanguageButton_cb, LV_EVENT_CLICKED, NULL);

    // ===========================
    // Initialize panel colors
    // ===========================
    lv_obj_set_style_bg_color(ui_Panel2, COLOR_PANEL_WHITE, LV_PART_MAIN); // white
    lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel5, COLOR_PANEL_WHITE, LV_PART_MAIN); // white
    lv_obj_set_style_opa(ui_Panel5, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel6, COLOR_PANEL_WHITE, LV_PART_MAIN); // white
    lv_obj_set_style_opa(ui_Panel6, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel4, COLOR_PANEL_WHITE, LV_PART_MAIN); // white
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


    // hide alarm panels at startup
    lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_HIDDEN); 
    lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_HIDDEN);

    // Add clickability and callbacks to alarm containers
    lv_obj_add_flag(ui_Alarm1Cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Alarm2Cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Alarm3Cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Alarm4Cont, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(ui_Alarm1Cont, Alarm1Cont_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Alarm2Cont, Alarm2Cont_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Alarm3Cont, Alarm3Cont_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Alarm4Cont, Alarm4Cont_cb, LV_EVENT_CLICKED, NULL);

    // Make alarm labels clickable too
    lv_obj_add_flag(ui_Alarm1Label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Alarm1Panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Alarm1Label, Alarm1Cont_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Alarm1Panel, Alarm1Cont_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(ui_Alarm2Label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Alarm2Panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Alarm2Label, Alarm2Cont_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Alarm2Panel, Alarm2Cont_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(ui_Alarm3Label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Alarm3Panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Alarm3Label, Alarm3Cont_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Alarm3Panel, Alarm3Cont_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(ui_Alarm4Panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Alarm4Label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Alarm4Label, Alarm4Cont_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Alarm4Panel, Alarm4Cont_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_ImgButton7, AlarmsTabview_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_AlarmButton, AlarmButton_cb, LV_EVENT_CLICKED, NULL);


    // --- Adjust Alarm description Label ---
    lv_obj_set_width(ui_AlarmDetailLabel, lv_pct(100));                 // width 100% of parent
    lv_label_set_long_mode(ui_AlarmDetailLabel, LV_LABEL_LONG_WRAP);    // allow line breaks
    lv_obj_set_style_text_align(ui_AlarmDetailLabel,
                                LV_TEXT_ALIGN_CENTER,
                                0);                                     // text centered within the label
    // Optional: position the label in the tab (top center with a small Y margin)
    lv_obj_align(ui_AlarmDetailLabel, LV_ALIGN_TOP_MID, 0, 20);


    lv_obj_add_event_cb(ui_AlarmsTabview, AlarmsTabview_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // Initially, empty description
    lv_label_set_text(ui_AlarmDetailLabel, "");

    lv_obj_add_flag(ui_NumAlarm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);


    // ============================================================================
    // TempChart configuration
    // ============================================================================

     // ===== Limpiar series creadas por SquareLine =====
    lv_chart_series_t * s = lv_chart_get_series_next(ui_TempChart, NULL);
    while (s != NULL) {
        lv_chart_series_t * next = lv_chart_get_series_next(ui_TempChart, s);
        lv_chart_remove_series(ui_TempChart, s);
        s = next;
    }

    // ===== Configuration of TempChart =====
    lv_chart_set_type(ui_TempChart, LV_CHART_TYPE_LINE);

    // How many points to store (display) 
    lv_chart_set_point_count(ui_TempChart, 50);

    // Y-axis range (for example from 20ºC to 45ºC)
    lv_chart_set_range(ui_TempChart,
                       LV_CHART_AXIS_PRIMARY_Y,
                       20, 45);

    // Create the series (any color you want)
    tempSeries = lv_chart_add_series(ui_TempChart,
                                     lv_palette_main(LV_PALETTE_BLUE),
                                     LV_CHART_AXIS_PRIMARY_Y);

    // Hide secondary Y axis (right)
    lv_chart_set_axis_tick(ui_TempChart,
                        LV_CHART_AXIS_SECONDARY_Y,
                        0,    // major_len
                        0,    // minor_len
                        0,    // major_cnt
                        0,    // minor_cnt
                        false,// label_en -> no text
                        0);   // draw_size

    // Hide X axis ticks and labels
    lv_chart_set_axis_tick(ui_TempChart,
                        LV_CHART_AXIS_PRIMARY_X,
                        0, 0, 0, 0, false, 0);
            
    // Optional: initialize all points to 0 or a neutral value
    for (int i = 0; i < lv_chart_get_point_count(ui_TempChart); i++) {
        tempSeries->y_points[i] = LV_CHART_POINT_NONE;   // or a specific value
    }
    lv_chart_refresh(ui_TempChart);

     // ============================================================================
    // HumChart configuration
    // ============================================================================

    // Remove series created by SquareLine
    s = lv_chart_get_series_next(ui_HumChart, NULL);
    while (s != NULL) {
        lv_chart_series_t * next = lv_chart_get_series_next(ui_HumChart, s);
        lv_chart_remove_series(ui_HumChart, s);
        s = next;
    }

    lv_chart_set_type(ui_HumChart, LV_CHART_TYPE_LINE);

    // NNumber of points "compact" (you can put 30, 40, etc.)
    lv_chart_set_point_count(ui_HumChart, 50);

    // Range 0–100 %
    lv_chart_set_range(ui_HumChart,
                       LV_CHART_AXIS_PRIMARY_Y,
                       0, 100);

    // Series for humidity
    humSeries = lv_chart_add_series(ui_HumChart,
                                    lv_palette_main(LV_PALETTE_GREEN),
                                    LV_CHART_AXIS_PRIMARY_Y);

    // Hide secondary Y axis
    lv_chart_set_axis_tick(ui_HumChart,
                           LV_CHART_AXIS_SECONDARY_Y,
                           0, 0, 0, 0, false, 0);

    // Hide X axis (no ticks or labels)
    lv_chart_set_axis_tick(ui_HumChart,
                           LV_CHART_AXIS_PRIMARY_X,
                           0, 0, 0, 0, false, 0);

    // Initialize all points as empty
    for (int i = 0; i < lv_chart_get_point_count(ui_HumChart); i++) {
        humSeries->y_points[i] = LV_CHART_POINT_NONE;
    }
    lv_chart_refresh(ui_HumChart);


    //=================================================================================================

    lv_timer_handler();      // Process any initial LVGL tasks

    // ===========================
    // Temperature and humidity
    // ===========================
    init_values();           // Assign initial random values and update labels

    lv_obj_add_event_cb(ui_Switch1, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Switch2, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Switch3, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Switch4, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ===========================
    // Simulate switch ON/OFF via labels
    // ===========================

    // ----- SWITCH 1 (Temperature) -----
    // Label9 = ON  (activates Switch1)
    lv_obj_add_flag(ui_Label9, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Label9, [](lv_event_t * e){
        lv_obj_add_state(ui_Switch1, LV_STATE_CHECKED);              // we turn it ON
        lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED, NULL);     // triggers Switch_cb()
    }, LV_EVENT_CLICKED, NULL);

    // Label15 = OFF (deactivates Switch1)
    lv_obj_add_flag(ui_Label15, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Label15, [](lv_event_t * e){
        lv_obj_clear_state(ui_Switch1, LV_STATE_CHECKED);            // we turn it OFF
        lv_event_send(ui_Switch1, LV_EVENT_VALUE_CHANGED, NULL);     // triggers Switch_cb()
    }, LV_EVENT_CLICKED, NULL);

    // ----- SWITCH 2 (Humidity) -----
    // Label13 = ON
    lv_obj_add_flag(ui_Label13, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Label13, [](lv_event_t * e){
        lv_obj_add_state(ui_Switch2, LV_STATE_CHECKED);
        lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
    }, LV_EVENT_CLICKED, NULL);

    // Label16 = OFF
    lv_obj_add_flag(ui_Label16, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Label16, [](lv_event_t * e){
        lv_obj_clear_state(ui_Switch2, LV_STATE_CHECKED);
        lv_event_send(ui_Switch2, LV_EVENT_VALUE_CHANGED, NULL);
    }, LV_EVENT_CLICKED, NULL);

    // ----- SWITCH 3 (Phototherapy) -----
    // Label10 = ON
    lv_obj_add_flag(ui_Label10, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Label10, [](lv_event_t * e){
        lv_obj_add_state(ui_Switch3, LV_STATE_CHECKED);
        lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
    }, LV_EVENT_CLICKED, NULL);

    // Label17 = OFF
    lv_obj_add_flag(ui_Label17, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_Label17, [](lv_event_t * e){
        lv_obj_clear_state(ui_Switch3, LV_STATE_CHECKED);
        lv_event_send(ui_Switch3, LV_EVENT_VALUE_CHANGED, NULL);
    }, LV_EVENT_CLICKED, NULL);


    // Connect panel selection callbacks
    setup_panel_callbacks();
    // Connect callbacks for arrows
    setup_arrow_callbacks();
    setup_arrow_hum_callbacks();

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

    if (ReceiveMessageFromOtherESP()) {

    // if new alarm received
    if(ctrl_msg_alarm.id != 0) {
        processReceivedAlarm(ctrl_msg_alarm);

        // Clear structure for next alarm
        ctrl_msg_alarm.id = 0;
        ctrl_msg_alarm.state = false;
    } else if (error == false) {
        applyHMIData();
      }
    }


    /*if (alarmActive) {
        buzzerOn();
    } else {
        buzzerOff();
    }*/

}
    
