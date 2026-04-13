#ifndef ELEMENTS_CREATION_H
#define ELEMENTS_CREATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// --- UI HELPERS DEFINES & FUNCTIONS ---
#define _UI_TEMPORARY_STRING_BUFFER_SIZE 32
#define _UI_BAR_PROPERTY_VALUE 0
#define _UI_BAR_PROPERTY_VALUE_WITH_ANIM 1
void _ui_bar_set_property(lv_obj_t * target, int id, int val);

#define _UI_BASIC_PROPERTY_POSITION_X 0
#define _UI_BASIC_PROPERTY_POSITION_Y 1
#define _UI_BASIC_PROPERTY_WIDTH 2
#define _UI_BASIC_PROPERTY_HEIGHT 3
void _ui_basic_set_property(lv_obj_t * target, int id, int val);

#define _UI_DROPDOWN_PROPERTY_SELECTED 0
void _ui_dropdown_set_property(lv_obj_t * target, int id, int val);

#define _UI_IMAGE_PROPERTY_IMAGE 0
void _ui_image_set_property(lv_obj_t * target, int id, uint8_t * val);

#define _UI_LABEL_PROPERTY_TEXT 0
void _ui_label_set_property(lv_obj_t * target, int id, const char * val);

#define _UI_ROLLER_PROPERTY_SELECTED 0
#define _UI_ROLLER_PROPERTY_SELECTED_WITH_ANIM 1
void _ui_roller_set_property(lv_obj_t * target, int id, int val);

#define _UI_SLIDER_PROPERTY_VALUE 0
#define _UI_SLIDER_PROPERTY_VALUE_WITH_ANIM 1
void _ui_slider_set_property(lv_obj_t * target, int id, int val);

void _ui_screen_change(lv_obj_t ** target, lv_scr_load_anim_t fademode, int spd, int delay, void (*target_init)(void));
void _ui_screen_delete(lv_obj_t ** target);
void _ui_arc_increment(lv_obj_t * target, int val);
void _ui_bar_increment(lv_obj_t * target, int val, int anm);
void _ui_slider_increment(lv_obj_t * target, int val, int anm);
void _ui_keyboard_set_target(lv_obj_t * keyboard, lv_obj_t * textarea);

#define _UI_MODIFY_FLAG_ADD 0
#define _UI_MODIFY_FLAG_REMOVE 1
#define _UI_MODIFY_FLAG_TOGGLE 2
void _ui_flag_modify(lv_obj_t * target, int32_t flag, int value);

#define _UI_MODIFY_STATE_ADD 0
#define _UI_MODIFY_STATE_REMOVE 1
#define _UI_MODIFY_STATE_TOGGLE 2
void _ui_state_modify(lv_obj_t * target, int32_t state, int value);

#define UI_MOVE_CURSOR_UP 0
#define UI_MOVE_CURSOR_RIGHT 1
#define UI_MOVE_CURSOR_DOWN 2
#define UI_MOVE_CURSOR_LEFT 3
void _ui_textarea_move_cursor(lv_obj_t * target, int val);

void scr_unloaded_delete_cb(lv_event_t * e);
void _ui_opacity_set(lv_obj_t * target, int val);

void _ui_arc_set_text_value(lv_obj_t * trg, lv_obj_t * src, const char * prefix, const char * postfix);
void _ui_arc_set_text_value(lv_obj_t * trg, lv_obj_t * src, const char * prefix, const char * postfix);
void _ui_slider_set_text_value(lv_obj_t * trg, lv_obj_t * src, const char * prefix, const char * postfix);
void _ui_checked_set_text_value(lv_obj_t * trg, lv_obj_t * src, const char * txt_on, const char * txt_off);
void _ui_spinbox_step(lv_obj_t * target, int val);
void _ui_switch_theme(int val);

// --- ASSETS (IMAGES) ---
LV_IMG_DECLARE(ui_img_296721678);    // assets/settings icon (2).png
LV_IMG_DECLARE(ui_img_1007688293);    // assets/alarms1 (2).png
LV_IMG_DECLARE(ui_img_1084506651);    // assets/wind-vector.png
LV_IMG_DECLARE(ui_img_1370137984);    // assets/termometro (2).png
LV_IMG_DECLARE(ui_img_bebe_icon_png);    // assets/bebe icon.png
LV_IMG_DECLARE(ui_img_triangulo_abajo_png);    // assets/triangulo_abajo.png
LV_IMG_DECLARE(ui_img_triangulo_arriba_png);    // assets/triangulo_arriba.png
LV_IMG_DECLARE(ui_img_gota_png);    // assets/gota.png
LV_IMG_DECLARE(ui_img_pulse_png);    // assets/pulse.png
LV_IMG_DECLARE(ui_img_chart_png);    // assets/chart.png
LV_IMG_DECLARE(ui_img_1508956403);    // assets/back icon (1).png
LV_IMG_DECLARE(ui_img_mute_icon_png);    // assets/mute_icon.png
LV_IMG_DECLARE(ui_img_candado_png);    // assets/candado.png
LV_IMG_DECLARE(ui_img_302897630);    // assets/water-drop-icon-transparent-free-png.png
LV_IMG_DECLARE(ui_img_windvector_png);    // assets/windVector.png
LV_IMG_DECLARE(ui_img_check_png);    // assets/check.png
LV_IMG_DECLARE(ui_img_flecha_png);    // assets/Flecha.png
LV_IMG_DECLARE(ui_img_incunest2_png);    // assets/INCUNEST2.png

// --- GLOBAL VARIABLES ---
extern lv_obj_t * ui____initial_actions0;

// --- SCREEN: Intro ---
extern lv_obj_t * ui_ScreenIntro;
extern lv_obj_t * ui_ImageLogoIncunest;
void ui_ScreenIntro_screen_init(void);
void ui_ScreenIntro_screen_destroy(void);

// --- SCREEN: Main ---
extern lv_obj_t * ui_ScreenMain;
extern lv_obj_t * ui_Incunest;
extern lv_obj_t * ui_Settings;
extern lv_obj_t * ui_AlarmButton;
extern lv_obj_t * ui_TempCont;
extern lv_obj_t * ui_Panel1;
extern lv_obj_t * ui_Panel4;
extern lv_obj_t * ui_Switch1;
extern lv_obj_t * ui_Label2;
extern lv_obj_t * ui_AirPanelCont;
extern lv_obj_t * ui_AirPanel;
extern lv_obj_t * ui_TempAirDetected;
extern lv_obj_t * ui_Label30;
extern lv_obj_t * ui_Image4;
extern lv_obj_t * ui_AirTempBarCont;
extern lv_obj_t * ui_TempAirDetectedRight;
extern lv_obj_t * ui_TempAirDesired;
extern lv_obj_t * ui_AirTempBar;
extern lv_obj_t * ui_Image6;
extern lv_obj_t * ui_SkinPanelCont;
extern lv_obj_t * ui_SkinPanel;
extern lv_obj_t * ui_Label31;
extern lv_obj_t * ui_TempSkinDetected;

// Heater Error Warning - Temp
extern lv_obj_t * ui_HeaterErrorTempCont;
extern lv_obj_t * ui_HeaterErrorTempImg;
extern lv_obj_t * ui_HeaterErrorTempLabel;
extern lv_obj_t * ui_Image2;
extern lv_obj_t * ui_SkinTempBarCont;
extern lv_obj_t * ui_SkinTempBar;
extern lv_obj_t * ui_Image1;
extern lv_obj_t * ui_TempSkinDetectedRight;
extern lv_obj_t * ui_TempSkinDesired;
extern lv_obj_t * ui_Label6;
extern lv_obj_t * ui_Label9;
extern lv_obj_t * ui_Label15;
extern lv_obj_t * ui_ArrowDownTemp;
extern lv_obj_t * ui_ArrowUpTemp;
extern lv_obj_t * ui_ImgArrowDownTemp;
extern lv_obj_t * ui_ImgArrowUpTemp;
extern lv_obj_t * ui_TempButton;
extern lv_obj_t * ui_HumCont;
extern lv_obj_t * ui_Panel3;
extern lv_obj_t * ui_Panel6;
extern lv_obj_t * ui_HumPanelCont;
extern lv_obj_t * ui_HumDetected;
extern lv_obj_t * ui_HumBar;
extern lv_obj_t * ui_Image7;
extern lv_obj_t * ui_ArrowUpHum;
extern lv_obj_t * ui_ArrowDownHum;
extern lv_obj_t * ui_HumDetectedRight;
extern lv_obj_t * ui_HumDesired;
extern lv_obj_t * ui_ImgArrowUpHum;
extern lv_obj_t * ui_ImgArrowDownHum;
extern lv_obj_t * ui_Label7;
extern lv_obj_t * ui_Label16;
extern lv_obj_t * ui_Label13;
extern lv_obj_t * ui_Switch2;
extern lv_obj_t * ui_HumidButton;
extern lv_obj_t * ui_HumidityLabel;

// Heater Error Warning - Hum
extern lv_obj_t * ui_HeaterErrorHumCont;
extern lv_obj_t * ui_HeaterErrorHumImg;
extern lv_obj_t * ui_HeaterErrorHumLabel;
extern lv_obj_t * ui_PhotoCont;
extern lv_obj_t * ui_Panel2;
extern lv_obj_t * ui_Switch3;
extern lv_obj_t * ui_PhototherapyLabel;
extern lv_obj_t * ui_Label17;
extern lv_obj_t * ui_Label10;
extern lv_obj_t * ui_Panel10;
extern lv_obj_t * ui_NumAlarm;

// Phototherapy Timer
extern lv_obj_t * ui_PhotoTimerCont;
extern lv_obj_t * ui_PhotoTimerPanel;
extern lv_obj_t * ui_PhotoTimeValueLabel; 
extern lv_obj_t * ui_PhotoTimeMinusBtn;
extern lv_obj_t * ui_PhotoTimePlusBtn;
extern lv_obj_t * ui_PhotoStartBtn;
extern lv_obj_t * ui_PhotoStartLabel;
extern lv_obj_t * ui_PhotoCancelBtn;
extern lv_obj_t * ui_PhotoCancelLabel;
extern lv_obj_t * ui_PhotoTimeMinusLabel;
extern lv_obj_t * ui_PhotoTimePlusLabel;

extern lv_obj_t * ui_SPO2Button;
extern lv_obj_t * ui_ChartButton;
extern lv_obj_t * ui_ImgButton1;
extern lv_obj_t * ui_CheckImgMain;
extern lv_obj_t * uic_Tempbutton;
extern lv_obj_t * uic_HumidButton;
void ui_ScreenMain_screen_init(void);
void ui_ScreenMain_screen_destroy(void);
void ui_event_PhotoCancelBtn(lv_event_t * e);

// --- AUTO AIR widgets (created in ElementsCreation.cpp) ---
extern lv_obj_t *ui_AutoAirBtn;
extern lv_obj_t *ui_AutoAirBtnLabel;
extern lv_obj_t *ui_AutoAirOverlay;
extern lv_obj_t *ui_AutoAirModal;
extern lv_obj_t *ui_AutoAirWeightVal;
extern lv_obj_t *ui_AutoAirGestVal;
extern lv_obj_t *ui_AutoAirDaysVal;
extern lv_obj_t *ui_AutoAirDaysUnitLbl;
extern lv_obj_t *ui_AutoAirErrLabel;
extern lv_obj_t *ui_AutoAirToast;
extern lv_obj_t *ui_AutoAirRowGest;
extern lv_obj_t *ui_AutoAirRowDays;
extern lv_obj_t *ui_AutoAirRowWeight;
extern lv_obj_t *ui_AutoAirHSep;
extern lv_obj_t *ui_AutoAirVSep;
extern lv_obj_t *ui_AutoAirTitle;
extern lv_obj_t *ui_AutoAirLeftHeader;
extern lv_obj_t *ui_AutoAirRightHeader;
extern lv_obj_t *ui_AutoAirGestLabel;
extern lv_obj_t *ui_AutoAirDaysLabel;
extern lv_obj_t *ui_AutoAirWeightLabel;
extern lv_obj_t *ui_AutoAirCancelLabel;
extern lv_obj_t *ui_AutoAirApplyLabel;
// --- AUTO AIR range display widgets ---
extern lv_obj_t *aa_range_bar;
extern lv_obj_t *aa_setpoint_marker;
extern lv_obj_t *aa_label_hi;
extern lv_obj_t *aa_label_mid;
extern lv_obj_t *aa_label_lo;
extern lv_obj_t *aa_setpoint_label;
void create_autoair_button(void);
void create_autoair_popup(void);

// --- SCREEN: Settings ---
extern lv_obj_t * ui_ScreenSettings;
extern lv_obj_t * ui_Label8;
extern lv_obj_t * ui_ImgButton2;
extern lv_obj_t * ui_Container3;
extern lv_obj_t * ui_WifiCont;
extern lv_obj_t * ui_Panel7;
extern lv_obj_t * ui_WifiLabel;
extern lv_obj_t * ui_WifiButton;
extern lv_obj_t * ui_Label3;
extern lv_obj_t * ui_LanguagesCont;
extern lv_obj_t * ui_Panel8;
extern lv_obj_t * ui_LanguagesLabel;
extern lv_obj_t * ui_LanguagesButton;
extern lv_obj_t * ui_Label1;
extern lv_obj_t * ui_SkinModeCont;
extern lv_obj_t * ui_Panel9;
extern lv_obj_t * ui_SkinOptionLabel;
extern lv_obj_t * ui_Switch4;
extern lv_obj_t * ui_DarkModeCont;
extern lv_obj_t * ui_PanelDarkMode;
extern lv_obj_t * ui_DarkModeLabel;
extern lv_obj_t * ui_SwitchDarkMode;
extern lv_obj_t * ui_HumidityModeCont;
extern lv_obj_t * ui_PanelHumidityMode;
extern lv_obj_t * ui_HumidityModeLabel;
extern lv_obj_t * ui_SwitchHumidityMode;
extern lv_obj_t * ui_InfoCont;
extern lv_obj_t * ui_InfoPanel;
extern lv_obj_t * ui_InfoLabel;
extern lv_obj_t * ui_InfoButton;
extern lv_obj_t * ui_InfoArrow;
extern lv_obj_t * ui_InfoDetailsCont;
extern lv_obj_t * ui_InfoDetailsPanel;
extern lv_obj_t * ui_HMIVerTitle;
extern lv_obj_t * ui_HMIVerValue;
extern lv_obj_t * ui_MBVerTitle;
extern lv_obj_t * ui_MBVerValue;
extern lv_obj_t * ui_SNTitle;
extern lv_obj_t * ui_SNValue;
extern lv_obj_t * ui_ConnTitle;
extern lv_obj_t * ui_ConnValue;
extern lv_obj_t * ui_WifiConfigCont;
extern lv_obj_t * ui_Keyboard1;
extern lv_obj_t * ui_SSIDPanel;
extern lv_obj_t * ui_SSIDLabel;
extern lv_obj_t * ui_PassPanel;
extern lv_obj_t * ui_PassLabel;
extern lv_obj_t * ui_TextArea1;
extern lv_obj_t * ui_TextArea2;
extern lv_obj_t * ui_WifiConnectButton;
extern lv_obj_t * ui_ConnectLabel;
extern lv_obj_t * ui_WifiDisconnectButton;
extern lv_obj_t * ui_DisconnectLabel;
extern lv_obj_t * ui_LanguagesDropDown;
extern lv_obj_t * ui_WifiConnectedCont;
extern lv_obj_t * ui_WifiConnectedPanel;
extern lv_obj_t * ui_ArrowWifiConnected;
extern lv_obj_t * ui_WifiSSIDLabel;
extern lv_obj_t * ui_WifiConnectedToLabel;
void ui_ScreenSettings_screen_init(void);
void ui_ScreenSettings_screen_destroy(void);

// --- SCREEN: Alarms ---
extern lv_obj_t * ui_ScreenAlarms;
extern lv_obj_t * ui_ImgButton7;
extern lv_obj_t * ui_Panel5;
extern lv_obj_t * ui_AlarmsTabview;
extern lv_obj_t * ui_TabPage1;
extern lv_obj_t * ui_Alarm1Cont;
extern lv_obj_t * ui_Alarm1Panel;
extern lv_obj_t * ui_Alarm1Label;
extern lv_obj_t * ui_Alarm2Cont;
extern lv_obj_t * ui_Alarm2Panel;
extern lv_obj_t * ui_Alarm2Label;
extern lv_obj_t * ui_Alarm3Cont;
extern lv_obj_t * ui_Alarm3Panel;
extern lv_obj_t * ui_Alarm3Label;
extern lv_obj_t * ui_Alarm4Cont;
extern lv_obj_t * ui_Alarm4Panel;
extern lv_obj_t * ui_Alarm4Label;
extern lv_obj_t * ui_TabPage2;
extern lv_obj_t * ui_AlarmDetailLabel;
extern lv_obj_t * ui_MuteAlarm;
void ui_ScreenAlarms_screen_init(void);
void ui_ScreenAlarms_screen_destroy(void);

// --- SCREEN: Charts ---
extern lv_obj_t * ui_ScreenCharts;
extern lv_obj_t * ui_TabViewMainCharts;
extern lv_obj_t * ui_HistoryDropdown;
extern lv_obj_t * ui_TabView1;
extern lv_obj_t * ui_TempChartPage1;
extern lv_obj_t * ui_AirTempChartCont;
extern lv_obj_t * ui_AirTempChart;
extern lv_obj_t * ui_Label37;
extern lv_obj_t * ui_SkinTempChartCont;
extern lv_obj_t * ui_SkinTempChart;
extern lv_obj_t * ui_Label38;
extern lv_obj_t * ui_HumChartPage2;
extern lv_obj_t * ui_HumChartCont;
extern lv_obj_t * ui_HumChart;
extern lv_obj_t * ui_Label36;
extern lv_obj_t * ui_OxChartCont;
extern lv_obj_t * ui_OxChart;
extern lv_obj_t * ui_Label35;
extern lv_obj_t * ui_ImgButton8;

extern lv_obj_t * ui_HistoryChartAire;
extern lv_obj_t * ui_HistoryChartSkin;
extern lv_obj_t * ui_HistoryChartHum;

extern lv_obj_t * ui_HistoryTimeLabel;
extern lv_obj_t * ui_HistoryChartAireLabel;
extern lv_obj_t * ui_HistoryChartSkinLabel;
extern lv_obj_t * ui_HistoryChartHumLabel;

extern lv_obj_t * ui_HistoryValueAire;
extern lv_obj_t * ui_HistoryValueSkin;
extern lv_obj_t * ui_HistoryValueHum;


void ui_ScreenCharts_screen_init(void);
void ui_ScreenCharts_screen_destroy(void);

// --- SCREEN: PulseOxi ---
extern lv_obj_t * ui_ScreenPulseOxi;
extern lv_obj_t * ui_ImgButton9;
extern lv_obj_t * ui_OxCont;
extern lv_obj_t * ui_Panel15;
extern lv_obj_t * ui_Label39;
extern lv_obj_t * ui_Label5;
extern lv_obj_t * ui_DetectOxi;
extern lv_obj_t * ui_OxiButton2;
void ui_ScreenPulseOxi_screen_init(void);
void ui_ScreenPulseOxi_screen_destroy(void);

// --- SCREEN: Lock ---
extern lv_obj_t * ui_ScreenLock;
extern lv_obj_t * ui_LockButton;
extern lv_obj_t * ui_Container1;
extern lv_obj_t * ui_AirTempLockCont;
extern lv_obj_t * ui_Label11;
extern lv_obj_t * ui_Label18;
extern lv_obj_t * ui_ImageWindLS;
extern lv_obj_t * ui_SkinTempLockCont;
extern lv_obj_t * ui_Label12;
extern lv_obj_t * ui_Label14;
extern lv_obj_t * ui_ImageBabyLS;
extern lv_obj_t * ui_HumLockCont;
extern lv_obj_t * ui_Label19;
extern lv_obj_t * ui_Label20;
extern lv_obj_t * ui_ImagenWaterLS;
extern lv_obj_t * ui_HumLockDesiredCont;
extern lv_obj_t * ui_Label23;
extern lv_obj_t * ui_Label24;
extern lv_obj_t * ui_ArrowHumLock;
extern lv_obj_t * ui_UnlockCont;
extern lv_obj_t * ui_Panel11;
extern lv_obj_t * ui_Label4;
extern lv_obj_t * ui_LockButton2;
extern lv_obj_t * ui_TargetSkinTempCont;
extern lv_obj_t * ui_TargetSkinTempLabel;
extern lv_obj_t * ui_TargetSkinTempNumLabel;
extern lv_obj_t * ui_ArrowSkinLock;
extern lv_obj_t * ui_Spinner1;
extern lv_obj_t * ui_TargetAirTempCont;
extern lv_obj_t * ui_TargetAirTempLabel;
extern lv_obj_t * ui_TargetAirTempNumLabel;
extern lv_obj_t * ui_ArrowAirLock;
extern lv_obj_t * ui_StatusCont;
extern lv_obj_t * ui_StatusLabel;
extern lv_obj_t * ui_PhotoLockCont;
extern lv_obj_t * ui_PhotoLockLabel;
extern lv_obj_t * ui_PhotoLockTimeLabel;
extern lv_obj_t * ui_AlarmLockCont;
extern lv_obj_t * ui_AlarmLockImg;
extern lv_obj_t * ui_PanelLockAlarm;
extern lv_obj_t * ui_AlarmLockNumLabel;
extern lv_obj_t * ui_CheckImg;
void ui_ScreenLock_screen_init(void);
void ui_ScreenLock_screen_destroy(void);

// --- EVENT PROTOTYPES ---
void ui_event_Settings(lv_event_t * e);
void ui_event_AlarmButton(lv_event_t * e);
void ui_event_SPO2Button(lv_event_t * e);
void ui_event_ChartButton(lv_event_t * e);
void ui_event_ImgButton1(lv_event_t * e);
void ui_event_ImgButton7(lv_event_t * e);
void ui_event_ImgButton8(lv_event_t * e);
void ui_event_ImgButton9(lv_event_t * e);
void ui_event_ImgButton2(lv_event_t * e);
void ui_event_AlarmLockImg(lv_event_t * e);
void ui_event_MuteAlarm(lv_event_t * e);
void ui_event_TextArea1(lv_event_t * e);
void ui_event_TextArea2(lv_event_t * e);
void ui_event_Keyboard1(lv_event_t * e);
void ui_event_WifiButton(lv_event_t * e);
void ui_event_LanguagesButton(lv_event_t * e);
void ui_event_Alarm1Cont(lv_event_t * e);
void ui_event_Alarm2Cont(lv_event_t * e);
void ui_event_Alarm3Cont(lv_event_t * e);
void ui_event_Alarm4Cont(lv_event_t * e);
void ui_event_AlarmsTabview(lv_event_t * e);
void ui_event_ScreenLock(lv_event_t * e);
void ui_event_AlarmLockCont(lv_event_t * e);
void ui_event_LanguagesDropDown(lv_event_t * e);
void ui_event_WifiConnectButton(lv_event_t * e);
void ui_event_WifiDisconnectButton(lv_event_t * e);
void ui_event_Switch1(lv_event_t * e);
void ui_event_Switch2(lv_event_t * e);
void ui_event_Switch3(lv_event_t * e);
void ui_event_Switch4(lv_event_t * e);
void ui_event_SwitchDarkMode(lv_event_t * e);
void ui_event_Label9(lv_event_t * e);
void ui_event_Label15(lv_event_t * e);
void ui_event_Label13(lv_event_t * e);
void ui_event_Label16(lv_event_t * e);
void ui_event_Label10(lv_event_t * e);
void ui_event_Label17(lv_event_t * e);
void ui_event_InfoButton(lv_event_t * e);

// --- UI INIT ---
void ui_init(void);
void ui_destroy(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
