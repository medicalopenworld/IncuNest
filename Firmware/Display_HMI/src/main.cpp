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

// Variables de temp y humedad:
double tempValueair, tempValueskin;
double humValue;
int marked = 0;
bool switch1 = false;
bool switch2 = false;
bool switchedTemp = false;
bool switchedHum = false;
bool flechas = false;
//bool alarma_activa = false;
bool prevTempAlarm = false;
bool prevHumAlarm = false;


struct alarmas
{
    int id;
    char tipo[30];
    char descripcion[100];
    bool estado;
}; alarmas lista_alarmas[10]; // Array para almacenar hasta 10 alarmas




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
LGFX lcd;//

//Pantalla táctil
#define TOUCH_SDA 19
#define TOUCH_SCL 20
#define TOUCH_INT 38
#define TOUCH_RST -1   // usa -1 si no tienes pin de reset

#define TOUCH_WIDTH  800
#define TOUCH_HEIGHT 480

TAMC_GT911 ts = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);

//UI
#define TFT_BL 2
int led;
//DHT20 dht20;
SPIClass& spi = SPI;


/* Change to your screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
//static lv_color_t *disp_draw_buf;
static lv_color_t disp_draw_buf[800 * 480 / 15];
static lv_disp_drv_t disp_drv;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{

  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);


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

// Temp y humedad:
void set_active_panel(lv_obj_t* active, lv_obj_t* inactive) {
    // Panel activo → azul y opacidad completa
    lv_obj_set_style_bg_color(active, lv_color_make(220,240,255), LV_PART_MAIN);
    lv_obj_set_style_opa(active, LV_OPA_COVER, LV_PART_MAIN);

    // Panel inactivo → gris oscuro
    lv_obj_set_style_bg_color(inactive, lv_color_make(100,100,100), LV_PART_MAIN);
    lv_obj_set_style_opa(inactive, LV_OPA_COVER, LV_PART_MAIN);
}

void Switch_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);  // switch que generó el evento
    lv_obj_t * panel = NULL;

    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);

    if (obj == ui_Switch1) {  // SWITCH TEMPERATURA
        switch1 = checked;
        switchedTemp = checked;
        panel = ui_Panel1;

        // Si se enciende temperatura y no hay panel seleccionado, activar Air por defecto
        if (checked && marked == 0) {
            marked = 1; // Air
            set_active_panel(ui_PanelAir, ui_PanelSkin);
        }

        // Activar o desactivar flechas de temperatura
        if (checked && marked != 0) {
            // Habilitar flechas
            flechas = true;
            lv_obj_add_flag(ui_ImgFlechaAbajoTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgFlechaArribaTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_FlechaAbajoTemp, lv_color_make(220,240,255), LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_FlechaArribaTemp, lv_color_make(220,240,255), LV_PART_MAIN);
        } else {
            // Deshabilitar flechas
            flechas = false;
            lv_obj_clear_flag(ui_ImgFlechaAbajoTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgFlechaArribaTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_FlechaAbajoTemp, lv_color_make(100,100,100), LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_FlechaArribaTemp, lv_color_make(100,100,100), LV_PART_MAIN);
        }

        lv_obj_set_style_opa(ui_FlechaAbajoTemp, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_FlechaArribaTemp, LV_OPA_COVER, LV_PART_MAIN);
    } 
    else if (obj == ui_Switch2) {  // SWITCH HUMEDAD
        switch2 = checked;
        switchedHum = checked;
        panel = ui_Panel3;

        if (checked) {
            lv_obj_add_flag(ui_ImgFlechaAbajoHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgFlechaArribaHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_FlechaAbajoHum, lv_color_make(220,240,255), LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_FlechaArribaHum, lv_color_make(220,240,255), LV_PART_MAIN);
        } else {
            lv_obj_clear_flag(ui_ImgFlechaAbajoHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgFlechaArribaHum, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(ui_FlechaAbajoHum, lv_color_make(100,100,100), LV_PART_MAIN);
            lv_obj_set_style_bg_color(ui_FlechaArribaHum, lv_color_make(100,100,100), LV_PART_MAIN);
        }

        lv_obj_set_style_opa(ui_FlechaAbajoHum, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_FlechaArribaHum, LV_OPA_COVER, LV_PART_MAIN);
    }

    // Si temperatura está apagada, desactivamos los paneles y flechas
    if (!switchedTemp) {
        marked = 0;   // ningún panel activo
        flechas = false;

        lv_obj_set_style_bg_color(ui_PanelAir, lv_color_make(100,100,100), LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_PanelSkin, lv_color_make(100,100,100), LV_PART_MAIN);
        lv_obj_set_style_opa(ui_PanelAir, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_opa(ui_PanelSkin, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_clear_flag(ui_ImgFlechaAbajoTemp, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(ui_ImgFlechaArribaTemp, LV_OBJ_FLAG_CLICKABLE);
    } 
    else {
        // Si hay un panel activo, habilitamos las flechas
        if (marked != 0) {
            flechas = true;
            lv_obj_add_flag(ui_ImgFlechaAbajoTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(ui_ImgFlechaArribaTemp, LV_OBJ_FLAG_CLICKABLE);
        } else {
            flechas = false;
            lv_obj_clear_flag(ui_ImgFlechaAbajoTemp, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(ui_ImgFlechaArribaTemp, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    // Cambiar color del panel asociado
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


// Se llama cuando se toca PanelAir
void PanelAir_cb(lv_event_t * e) {
    if (!switchedTemp) return;  // solo actúa si el switch temp está ON
    marked = 1;  // cambia tu variable para saber que se seleccionó Air
    flechas = true;
    set_active_panel(ui_PanelAir, ui_PanelSkin);
}

// Se llama cuando se toca PanelSkin
void PanelSkin_cb(lv_event_t * e) {
    if (!switchedTemp) return;  // solo actúa si el switch temp está ON
    marked = 2; // cambia tu variable para saber que se seleccionó Skin
    flechas = true;
    set_active_panel(ui_PanelSkin, ui_PanelAir);
}
// Configuración de los labels como objetos clicables
void setup_panel_callbacks() {
    // Panel Air
    lv_obj_add_event_cb(ui_PanelAir, PanelAir_cb, LV_EVENT_CLICKED, NULL);
    //lv_obj_clear_flag(ui_PanelAir, LV_OBJ_FLAG_CLICKABLE); // opcional: si no quieres que capture otros eventos

    // Panel Skin
    lv_obj_add_event_cb(ui_PanelSkin, PanelSkin_cb, LV_EVENT_CLICKED, NULL);
    //lv_obj_clear_flag(ui_PanelSkin, LV_OBJ_FLAG_CLICKABLE);
}


void init_values() {
    // valores iniciales aleatorios
    tempValueair = (double)random(200, 370) / 10.0;  // genera 20.0 a 36.9
    tempValueskin = (double)random(350, 376) / 10.0;  // genera 35.0 a 37.5
    humValue  = (double)random(400, 700) / 10.0;  // genera 40.0 a 69.9

    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%.1f°C", tempValueair);  // temperatura aire
    lv_label_set_text(ui_TempAirDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f°C", tempValueskin);  // temperatura piel
    lv_label_set_text(ui_TempSkinDesired, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f%%", humValue);   // humedad
    lv_label_set_text(ui_HumDesired, buffer);
}

void update_labels() {
    char buffer[10];
  
    // Actualiza temperatura Aire
    snprintf(buffer, sizeof(buffer), "%.1f°C", tempValueair);
    lv_label_set_text(ui_TempAirDesired, buffer);

    // Actualiza temperatura Piel
    snprintf(buffer, sizeof(buffer), "%.1f°C", tempValueskin);
    lv_label_set_text(ui_TempSkinDesired, buffer);

    // Actualiza humedad
    snprintf(buffer, sizeof(buffer), "%.1f%%", humValue);
    lv_label_set_text(ui_HumDesired, buffer);
}

// Callbacks de las flechas

void setup_arrow_callbacks() {
    // Flecha arriba (temperatura)
    
    lv_obj_add_event_cb(ui_ImgFlechaArribaTemp, [](lv_event_t * e){
        if (!switchedTemp || !flechas) return; // Si está apagado, no hacer nada
        if (marked == 1) { 
            tempValueair += 0.1; 
        } else if (marked == 2) { 
            tempValueskin += 0.1; 
        }
        update_labels();
    }, LV_EVENT_CLICKED, NULL);

    // Flecha abajo (temperatura)
    lv_obj_add_event_cb(ui_ImgFlechaAbajoTemp, [](lv_event_t * e){
        if (!switchedTemp || !flechas) return;
        if (marked == 2) { 
            tempValueskin -= 0.1; 
        } else if (marked == 1) { 
            tempValueair -= 0.1; 
        }
        update_labels();
    }, LV_EVENT_CLICKED, NULL);
}

void setup_arrow_hum_callbacks() {
    lv_obj_add_event_cb(ui_ImgFlechaArribaHum, [](lv_event_t * e){
        if (!switchedHum) return;
        humValue += 0.1; 
        update_labels();
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_ImgFlechaAbajoHum, [](lv_event_t * e){
        if (!switchedHum) return;
        humValue -= 0.1; 
        update_labels();
    }, LV_EVENT_CLICKED, NULL);
}

static void blink_cb(void * obj, int32_t v) {
    lv_obj_t * target = (lv_obj_t *)obj;
    lv_obj_set_style_opa(target, v, 0);
}

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


void update_alarm_panels() {
    int pos = 0;

    for (int i = 0; i < sizeof(lista_alarmas)/sizeof(lista_alarmas[0]); i++) {
        if (lista_alarmas[i].estado) {
            // mostrar alarma en el panel correspondiente
            switch (pos) {
                case 0:
                    lv_obj_clear_flag(ui_Alarm1Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm1Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm1Label, lista_alarmas[i].tipo);
                    start_alarm_blink(ui_Alarm1Panel);
                    start_alarm_blink(ui_Alarm1Label);
                    break;
                case 1:
                    lv_obj_clear_flag(ui_Alarm2Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm2Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm2Label, lista_alarmas[i].tipo);
                    start_alarm_blink(ui_Alarm2Panel);
                    start_alarm_blink(ui_Alarm2Label);
                    break;
                case 2:
                    lv_obj_clear_flag(ui_Alarm3Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm3Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm3Label, lista_alarmas[i].tipo);
                    start_alarm_blink(ui_Alarm3Panel);
                    start_alarm_blink(ui_Alarm3Label);
                    break;
                case 3:
                    lv_obj_clear_flag(ui_Alarm4Panel, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_Alarm4Label, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_Alarm4Label, lista_alarmas[i].tipo);
                    start_alarm_blink(ui_Alarm4Panel);
                    start_alarm_blink(ui_Alarm4Label);
                    break;
            }
            pos++;
            if (pos >= 4) break; // solo mostrar máximo 4 alarmas
        }
    }

    // ocultar paneles sobrantes si hay menos de 4 alarmas activas
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
    // Inicialización de comunicación serial y pines
    // ===========================
    Serial.begin(115200);          // Inicia el puerto serial para depuración
    pinMode(38, OUTPUT);         // Configura el pin 38 como salida (LED)
    digitalWrite(38, LOW);       // Inicializa LED apagado

    Wire.begin(19, 20);          // Inicializa I2C en los pines 19 (SDA) y 20 (SCL)
    /*Wire1.begin(15, 16);          // Inicializa I2C en los pines 15 (SDA) y 16 (SCL)

    buzzerOn();
    delay(3000);
    buzzerOff();*/
   // ===========================
    // Inicialización de la pantalla
    // ===========================
    lcd.begin();                 // Inicializa la pantalla
    lcd.fillScreen(TFT_BLACK);   // Limpia la pantalla con color negro
    lcd.setTextSize(2);          // Tamaño de texto predeterminado
    delay(200);                  // Pequeña pausa para estabilizar

    lv_init();                   // Inicializa la librería LVGL
    ts.begin();                  // Inicializa la pantalla táctil
    ts.setRotation(3);           // Configura la orientación táctil
    lcd.setRotation(2);          // Configura la orientación de la pantalla

    // ===========================
    // Configuración del buffer de dibujo de LVGL
    // ===========================
    screenWidth = lcd.width();   
    screenHeight = lcd.height();
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 15);

    // ===========================
    // Configuración del driver de pantalla para LVGL
    // ===========================
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;        // Resolución horizontal
    disp_drv.ver_res = screenHeight;       // Resolución vertical
    disp_drv.flush_cb = my_disp_flush;     // Función para enviar los píxeles a la pantalla
    disp_drv.draw_buf = &draw_buf;         // Buffer de dibujo
    lv_disp_drv_register(&disp_drv);       // Registra el driver en LVGL

    // ===========================
    // Inicialización del driver de entrada (touch)
    // ===========================
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;    // Tipo puntero (touch)
    indev_drv.read_cb = my_touchpad_read;      // Callback para leer el touch
    lv_indev_drv_register(&indev_drv);        // Registra el driver en LVGL

    // ===========================
    // Configuración del brillo de la pantalla
    // ===========================
#ifdef TFT_BL
    ledcSetup(1, 300, 8);          // Configura PWM canal 1, 300 Hz, 8 bits
    ledcAttachPin(TFT_BL, 1);      // Asocia pin de backlight al canal PWM
    ledcWrite(1, 255);             // Brillo máximo
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);     
    delay(500);
    digitalWrite(TFT_BL, HIGH);
#endif

    // ===========================
    // Inicialización de la interfaz gráfica
    // ===========================
    ui_init();               // Inicializa los objetos de la UI



    // ===========================
    // Inicializar colores de paneles
    // ===========================
    
    lv_obj_set_style_bg_color(ui_Panel2, lv_color_make(220,240,255), LV_PART_MAIN); // azul
    lv_obj_set_style_opa(ui_Panel2, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel5, lv_color_make(220,240,255), LV_PART_MAIN); // azul
    lv_obj_set_style_opa(ui_Panel5, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel6, lv_color_make(220,240,255), LV_PART_MAIN); // azul
    lv_obj_set_style_opa(ui_Panel6, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel4, lv_color_make(220,240,255), LV_PART_MAIN); // azul
    lv_obj_set_style_opa(ui_Panel4, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel1, lv_color_make(100,100,100), LV_PART_MAIN); // gris
    lv_obj_set_style_opa(ui_Panel1, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Panel3, lv_color_make(100,100,100), LV_PART_MAIN); // gris
    lv_obj_set_style_opa(ui_Panel3, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_FlechaAbajoTemp, lv_color_make(100,100,100), LV_PART_MAIN); // panel flecha temp gris
    lv_obj_set_style_opa(ui_FlechaAbajoTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_FlechaArribaTemp, lv_color_make(100,100,100), LV_PART_MAIN); // panel flecha temp gris
    lv_obj_set_style_opa(ui_FlechaArribaTemp, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_FlechaAbajoHum, lv_color_make(100,100,100), LV_PART_MAIN); // panel flecha hum gris
    lv_obj_set_style_opa(ui_FlechaAbajoHum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_FlechaArribaHum, lv_color_make(100,100,100), LV_PART_MAIN); // panel flecha hum gris
    lv_obj_set_style_opa(ui_FlechaArribaHum, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_PanelAir, lv_color_make(100,100,100), LV_PART_MAIN); // panel Air gris
    lv_obj_set_style_opa(ui_PanelAir, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_PanelSkin, lv_color_make(100,100,100), LV_PART_MAIN); // panel Skin gris
    lv_obj_set_style_opa(ui_PanelSkin, LV_OPA_COVER, LV_PART_MAIN);

    lv_timer_handler();      // Procesa cualquier tarea inicial de LVGL

    // ===========================
    // Temperatura y humedad
    // ===========================
    init_values();           // Asigna valores aleatorios iniciales y actualiza labels

    
    lv_obj_add_event_cb(ui_Switch1, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Switch2, Switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Conectar callbacks de selección de paneles
    setup_panel_callbacks();
    // Conectar callbacks de flechas
    setup_arrow_callbacks();

    setup_arrow_hum_callbacks();

    // ===========================
    // Configuración de alarmas
    // ===========================



    lista_alarmas[0].id = 1;
    strcpy(lista_alarmas[0].tipo, "Temperatura alta");
    strcpy(lista_alarmas[0].descripcion, "Desc");
    lista_alarmas[0].estado = false;

    lista_alarmas[1].id = 2;
    strcpy(lista_alarmas[1].tipo, "Humedad baja");
    strcpy(lista_alarmas[1].descripcion, "Desc");
    lista_alarmas[1].estado = false;

    
  
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

    bool tempAlarm = tempValueskin > 37.0;
    bool humAlarm  = humValue < 50.0;

    if(tempAlarm != prevTempAlarm) {
        lista_alarmas[0].estado = tempAlarm;
        update_alarm_panels();
        prevTempAlarm = tempAlarm;
    }
    if(humAlarm != prevHumAlarm) {
        lista_alarmas[1].estado = humAlarm;
        update_alarm_panels();
        prevHumAlarm = humAlarm;
    }

    /*if (alarma_activa) {
        buzzerOn();
    } else {
        buzzerOff();
    }*/
}