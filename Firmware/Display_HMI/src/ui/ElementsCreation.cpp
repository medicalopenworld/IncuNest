#include "ElementsCreation.h"
#include "CommTask.h"
#include "UITask.h"
#include "ui_helpers.h"
#include "main.h"
#include <Arduino.h>

// ============================================================================
// UI HELPERS IMPLEMENTATION
// ============================================================================

// (HMI Helpers are provided by ui_helpers.c)

// ============================================================================
// GLOBAL VARIABLE DEFINITIONS
// ============================================================================
lv_obj_t *ui____initial_actions0;

// Screen Intro
lv_obj_t *ui_ScreenIntro = NULL;
lv_obj_t *ui_ImageBabyLogo = NULL;
lv_obj_t *ui_ImageLogoIncunest = NULL;
lv_obj_t *ui_ImageSJD = NULL;
lv_obj_t *ui_ImageFlagTogo = NULL;
#if INTRO_FLAG != INTRO_FLAG_NONE
lv_obj_t *ui_ImageIntroFlag = NULL;
#endif

// Screen Main
lv_obj_t *ui_ScreenMain = NULL;
lv_obj_t *ui_Incunest = NULL;
lv_obj_t *ui_Settings = NULL;
lv_obj_t *ui_AlarmButton = NULL;
lv_obj_t *ui_TempCont = NULL;
lv_obj_t *ui_Panel1 = NULL;
lv_obj_t *ui_Panel4 = NULL;
lv_obj_t *ui_Switch1 = NULL;
lv_obj_t *ui_Label2 = NULL;
lv_obj_t *ui_AirPanelCont = NULL;
lv_obj_t *ui_AirPanel = NULL;
lv_obj_t *ui_TempAirDetected = NULL;
lv_obj_t *ui_Label30 = NULL;
lv_obj_t *ui_Image4 = NULL;
lv_obj_t *ui_AirTempBarCont = NULL;
lv_obj_t *ui_TempAirDetectedRight = NULL;
lv_obj_t *ui_TempAirDesired = NULL;
lv_obj_t *ui_AirTempBar = NULL;
lv_obj_t *ui_Image6 = NULL;
lv_obj_t *ui_SkinPanelCont = NULL;
lv_obj_t *ui_SkinPanel = NULL;
lv_obj_t *ui_Label31 = NULL;

lv_obj_t *ui_TempSkinDetected = NULL;

// Heater Error Warning - Temp
lv_obj_t *ui_HeaterErrorTempCont = NULL;
lv_obj_t *ui_HeaterErrorTempImg = NULL;
lv_obj_t *ui_HeaterErrorTempLabel = NULL;

lv_obj_t *ui_Image2 = NULL;
lv_obj_t *ui_SkinTempBarCont = NULL;
lv_obj_t *ui_SkinTempBar = NULL;
lv_obj_t *ui_Image1 = NULL;
lv_obj_t *ui_TempSkinDetectedRight = NULL;
lv_obj_t *ui_TempSkinDesired = NULL;
lv_obj_t *ui_Label6 = NULL;
lv_obj_t *ui_Label9 = NULL;
lv_obj_t *ui_Label15 = NULL;
lv_obj_t *ui_ArrowDownTemp = NULL;
lv_obj_t *ui_ArrowUpTemp = NULL;
lv_obj_t *ui_ImgArrowDownTemp = NULL;
lv_obj_t *ui_ImgArrowUpTemp = NULL;
lv_obj_t *ui_TempButton = NULL;
lv_obj_t *ui_HumCont = NULL;
lv_obj_t *ui_Panel3 = NULL;
lv_obj_t *ui_Panel6 = NULL;
lv_obj_t *ui_HumPanelCont = NULL;
lv_obj_t *ui_HumDetected = NULL;
lv_obj_t *ui_HumBar = NULL;
lv_obj_t *ui_Image7 = NULL;
lv_obj_t *ui_ArrowUpHum = NULL;
lv_obj_t *ui_ArrowDownHum = NULL;
lv_obj_t *ui_HumDetectedRight = NULL;
lv_obj_t *ui_HumDesired = NULL;
lv_obj_t *ui_ImgArrowUpHum = NULL;
lv_obj_t *ui_ImgArrowDownHum = NULL;
lv_obj_t *ui_Label7 = NULL;
lv_obj_t *ui_Label16 = NULL;
lv_obj_t *ui_Label13 = NULL;
lv_obj_t *ui_Switch2 = NULL;
lv_obj_t *ui_HumidButton = NULL;
lv_obj_t *ui_HumidityLabel = NULL;

// Heater Error Warning - Hum
lv_obj_t *ui_HeaterErrorHumCont = NULL;
lv_obj_t *ui_HeaterErrorHumImg = NULL;
lv_obj_t *ui_HeaterErrorHumLabel = NULL;

lv_obj_t *ui_PhotoCont = NULL;
lv_obj_t *ui_Panel2 = NULL;
lv_obj_t *ui_Switch3 = NULL;
lv_obj_t *ui_PhototherapyLabel = NULL;
lv_obj_t *ui_Label17 = NULL;
lv_obj_t *ui_Label10 = NULL;
lv_obj_t *ui_Panel10 = NULL;
lv_obj_t *ui_NumAlarm = NULL;
lv_obj_t *ui_SPO2Button = NULL;

// Phototherapy Timer
lv_obj_t *ui_PhotoTimerCont = NULL;
lv_obj_t *ui_PhotoTimerPanel = NULL;
lv_obj_t *ui_PhotoTimeValueLabel = NULL;
lv_obj_t *ui_PhotoTimeMinusBtn = NULL;
lv_obj_t *ui_PhotoTimePlusBtn = NULL;
lv_obj_t *ui_PhotoStartBtn = NULL;
lv_obj_t *ui_PhotoStartLabel = NULL;
lv_obj_t *ui_PhotoCancelBtn = NULL;
lv_obj_t *ui_PhotoCancelLabel = NULL;
lv_obj_t *ui_PhotoTimeMinusLabel = NULL;
lv_obj_t *ui_PhotoTimePlusLabel = NULL;

lv_obj_t *ui_ChartButton = NULL;
lv_obj_t *ui_ImgButton1 = NULL;
lv_obj_t *ui_CheckImgMain = NULL;
lv_obj_t *uic_Tempbutton = NULL;
lv_obj_t *uic_HumidButton = NULL;

// Screen Settings
lv_obj_t *ui_ScreenSettings = NULL;
lv_obj_t *ui_Label8 = NULL;
lv_obj_t *ui_ImgButton2 = NULL;
lv_obj_t *ui_Container3 = NULL;
lv_obj_t *ui_WifiCont = NULL;
lv_obj_t *ui_Panel7 = NULL;
lv_obj_t *ui_WifiLabel = NULL;
lv_obj_t *ui_WifiButton = NULL;
lv_obj_t *ui_Label3 = NULL;
lv_obj_t *ui_LanguagesCont = NULL;
lv_obj_t *ui_Panel8 = NULL;
lv_obj_t *ui_LanguagesLabel = NULL;
lv_obj_t *ui_LanguagesButton = NULL;
lv_obj_t *ui_Label1 = NULL;
lv_obj_t *ui_SkinModeCont = NULL;
lv_obj_t *ui_Panel9 = NULL;
lv_obj_t *ui_SkinOptionLabel = NULL;
lv_obj_t *ui_Switch4 = NULL;
lv_obj_t *ui_DarkModeCont = NULL;
lv_obj_t *ui_PanelDarkMode = NULL;
lv_obj_t *ui_DarkModeLabel = NULL;
lv_obj_t *ui_SwitchDarkMode = NULL;
lv_obj_t *ui_HumidityModeCont = NULL;
lv_obj_t *ui_PanelHumidityMode = NULL;
lv_obj_t *ui_HumidityModeLabel = NULL;
lv_obj_t *ui_SwitchHumidityMode = NULL;
lv_obj_t *ui_InfoCont = NULL;
lv_obj_t *ui_InfoPanel = NULL;
lv_obj_t *ui_InfoLabel = NULL;
lv_obj_t *ui_InfoButton = NULL;
lv_obj_t *ui_InfoArrow = NULL;
lv_obj_t *ui_InfoDetailsCont = NULL;
lv_obj_t *ui_InfoDetailsPanel = NULL;
lv_obj_t *ui_HMIVerTitle = NULL;
lv_obj_t *ui_HMIVerValue = NULL;
lv_obj_t *ui_MBVerTitle = NULL;
lv_obj_t *ui_MBVerValue = NULL;
lv_obj_t *ui_SNTitle = NULL;
lv_obj_t *ui_SNValue = NULL;
lv_obj_t *ui_ConnTitle = NULL;
lv_obj_t *ui_ConnValue = NULL;

lv_obj_t *ui_WifiConfigCont = NULL;
lv_obj_t *ui_Keyboard1 = NULL;
lv_obj_t *ui_SSIDPanel = NULL;
lv_obj_t *ui_SSIDLabel = NULL;
lv_obj_t *ui_PassPanel = NULL;
lv_obj_t *ui_PassLabel = NULL;
lv_obj_t *ui_TextArea1 = NULL;
lv_obj_t *ui_TextArea2 = NULL;
lv_obj_t *ui_WifiConnectButton = NULL;
lv_obj_t *ui_ConnectLabel = NULL;
lv_obj_t *ui_WifiDisconnectButton = NULL;
lv_obj_t *ui_DisconnectLabel = NULL;
lv_obj_t *ui_LanguagesDropDown = NULL;
lv_obj_t *ui_WifiConnectedCont = NULL;
lv_obj_t *ui_WifiConnectedPanel = NULL;
lv_obj_t *ui_ArrowWifiConnected = NULL;
lv_obj_t *ui_WifiSSIDLabel = NULL;
lv_obj_t *ui_WifiConnectedToLabel = NULL;

// Screen Alarms
lv_obj_t *ui_ScreenAlarms = NULL;
lv_obj_t *ui_ImgButton7 = NULL;
lv_obj_t *ui_Panel5 = NULL;
lv_obj_t *ui_AlarmsTabview = NULL;
lv_obj_t *ui_TabPage1 = NULL;
lv_obj_t *ui_Alarm1Cont = NULL;
lv_obj_t *ui_Alarm1Panel = NULL;
lv_obj_t *ui_Alarm1Label = NULL;
lv_obj_t *ui_Alarm2Cont = NULL;
lv_obj_t *ui_Alarm2Panel = NULL;
lv_obj_t *ui_Alarm2Label = NULL;
lv_obj_t *ui_Alarm3Cont = NULL;
lv_obj_t *ui_Alarm3Panel = NULL;
lv_obj_t *ui_Alarm3Label = NULL;
lv_obj_t *ui_Alarm4Cont = NULL;
lv_obj_t *ui_Alarm4Panel = NULL;
lv_obj_t *ui_Alarm4Label = NULL;
lv_obj_t *ui_TabPage2 = NULL;
lv_obj_t *ui_AlarmDetailLabel = NULL;
lv_obj_t *ui_MuteAlarm = NULL;

// Screen Charts
lv_obj_t *ui_ScreenCharts = NULL;
lv_obj_t *ui_TabViewMainCharts = NULL;
lv_obj_t *ui_HistoryDropdown = NULL;
lv_obj_t *ui_HistoryChartAire = NULL;
lv_obj_t *ui_HistoryChartSkin = NULL;
lv_obj_t *ui_HistoryChartHum = NULL;
lv_obj_t *ui_HistoryTimeLabel = NULL;
lv_obj_t *ui_HistoryChartAireLabel = NULL;
lv_obj_t *ui_HistoryChartSkinLabel = NULL;
lv_obj_t *ui_HistoryChartHumLabel = NULL;
lv_obj_t *ui_HistoryValueAire = NULL;
lv_obj_t *ui_HistoryValueSkin = NULL;
lv_obj_t *ui_HistoryValueHum = NULL;
lv_obj_t *ui_TabView1 = NULL;
extern lv_chart_series_t *historySeriesAire;
extern lv_chart_series_t *historySeriesSkin;
extern lv_chart_series_t *historySeriesHum;
lv_obj_t *ui_TempChartPage1 = NULL;
lv_obj_t *ui_AirTempChartCont = NULL;
lv_obj_t *ui_AirTempChart = NULL;
lv_obj_t *ui_Label37 = NULL;
lv_obj_t *ui_SkinTempChartCont = NULL;
lv_obj_t *ui_SkinTempChart = NULL;
lv_obj_t *ui_Label38 = NULL;
lv_obj_t *ui_HumChartPage2 = NULL;
lv_obj_t *ui_HumChartCont = NULL;
lv_obj_t *ui_HumChart = NULL;
lv_obj_t *ui_Label36 = NULL;
lv_obj_t *ui_OxChartCont = NULL;
lv_obj_t *ui_OxChart = NULL;
lv_obj_t *ui_Label35 = NULL;
lv_obj_t *ui_ImgButton8 = NULL;
void TabViewHistory_cb(lv_event_t *e);

// Screen PulseOxi
lv_obj_t *ui_ScreenPulseOxi = NULL;
lv_obj_t *ui_ImgButton9 = NULL;
lv_obj_t *ui_OxCont = NULL;
lv_obj_t *ui_Panel15 = NULL;
lv_obj_t *ui_Label39 = NULL;
lv_obj_t *ui_Label5 = NULL;
lv_obj_t *ui_DetectOxi = NULL;
lv_obj_t *ui_OxiButton2 = NULL;

// Screen Lock
lv_obj_t *ui_ScreenLock = NULL;
lv_obj_t *ui_LockButton = NULL;
lv_obj_t *ui_Container1 = NULL;
lv_obj_t *ui_AirTempLockCont = NULL;
lv_obj_t *ui_Label11 = NULL;
lv_obj_t *ui_Label18 = NULL;
lv_obj_t *ui_ImageWindLS = NULL;
lv_obj_t *ui_SkinTempLockCont = NULL;
lv_obj_t *ui_Label12 = NULL;
lv_obj_t *ui_Label14 = NULL;
lv_obj_t *ui_ImageBabyLS = NULL;
lv_obj_t *ui_HumLockCont = NULL;
lv_obj_t *ui_Label19 = NULL;
lv_obj_t *ui_Label20 = NULL;
lv_obj_t *ui_ImagenWaterLS = NULL;
lv_obj_t *ui_HumLockDesiredCont = NULL;
lv_obj_t *ui_Label23 = NULL;
lv_obj_t *ui_Label24 = NULL;
lv_obj_t *ui_ArrowHumLock = NULL;
lv_obj_t *ui_UnlockCont  = NULL;
lv_obj_t *ui_Panel11     = NULL;
lv_obj_t *ui_Label4      = NULL;
lv_obj_t *ui_LockButton2 = NULL;
lv_obj_t *ui_BorderTop   = NULL;
lv_obj_t *ui_BorderRight  = NULL;
lv_obj_t *ui_BorderBottom = NULL;
lv_obj_t *ui_BorderLeft   = NULL;
lv_obj_t *ui_TargetSkinTempCont = NULL;
lv_obj_t *ui_TargetSkinTempLabel = NULL;
lv_obj_t *ui_TargetSkinTempNumLabel = NULL;
lv_obj_t *ui_ArrowSkinLock = NULL;
lv_obj_t *ui_TargetAirTempCont = NULL;
lv_obj_t *ui_TargetAirTempLabel = NULL;
lv_obj_t *ui_TargetAirTempNumLabel = NULL;
lv_obj_t *ui_ArrowAirLock = NULL;
lv_obj_t *ui_StatusCont = NULL;
lv_obj_t *ui_StatusLabel = NULL;
lv_obj_t *ui_PhotoLockCont = NULL;
lv_obj_t *ui_PhotoLockLabel = NULL;
lv_obj_t *ui_PhotoLockTimeLabel = NULL;
lv_obj_t *ui_AlarmLockCont = NULL;
lv_obj_t *ui_AlarmLockImg = NULL;
lv_obj_t *ui_PanelLockAlarm = NULL;
lv_obj_t *ui_AlarmLockNumLabel = NULL;
lv_obj_t *ui_CheckImg = NULL;
lv_obj_t *ui_LockPPGChart = NULL;
lv_obj_t *ui_LockHRCont = NULL;
lv_obj_t *ui_LockHRLabel = NULL;
lv_obj_t *ui_LockPICont = NULL;
lv_obj_t *ui_LockPILabel = NULL;

// --- AUTO AIR UI objects ---
lv_obj_t *ui_AutoAirBtn = NULL;
lv_obj_t *ui_AutoAirBtnLabel = NULL;
lv_obj_t *ui_AutoAirOverlay = NULL;
lv_obj_t *ui_AutoAirModal = NULL;
lv_obj_t *ui_AutoAirWeightVal = NULL;
lv_obj_t *ui_AutoAirGestVal = NULL;
lv_obj_t *ui_AutoAirDaysVal = NULL;
lv_obj_t *ui_AutoAirDaysUnitLbl = NULL;
lv_obj_t *ui_AutoAirErrLabel = NULL;
lv_obj_t *ui_AutoAirToast = NULL;
lv_obj_t *ui_AutoAirRowGest = NULL;
lv_obj_t *ui_AutoAirRowDays = NULL;
lv_obj_t *ui_AutoAirRowWeight = NULL;
lv_obj_t *ui_AutoAirHSep = NULL;
lv_obj_t *ui_AutoAirVSep = NULL;
lv_obj_t *ui_AutoAirTitle = NULL;
lv_obj_t *ui_AutoAirLeftHeader = NULL;
lv_obj_t *ui_AutoAirRightHeader = NULL;
lv_obj_t *ui_AutoAirGestLabel = NULL;
lv_obj_t *ui_AutoAirDaysLabel = NULL;
lv_obj_t *ui_AutoAirWeightLabel = NULL;
lv_obj_t *ui_AutoAirCancelLabel = NULL;
lv_obj_t *ui_AutoAirApplyLabel = NULL;
// --- AUTO AIR range display widgets ---
lv_obj_t *aa_range_bar = NULL;
lv_obj_t *aa_setpoint_marker = NULL;
lv_obj_t *aa_label_hi = NULL;
lv_obj_t *aa_label_mid = NULL;
lv_obj_t *aa_label_lo = NULL;
lv_obj_t *aa_setpoint_label = NULL;

// Photo Safety Popup
lv_obj_t *ui_PhotoSafetyOverlay   = NULL;
lv_obj_t *ui_PhotoSafetyModal     = NULL;
lv_obj_t *ui_PhotoSafetyTitleLabel = NULL;
lv_obj_t *ui_PhotoSafetyBodyLabel  = NULL;
lv_obj_t *ui_PhotoSafetyTurnOnLabel = NULL;

// Main Screen Toggle Buttons
lv_obj_t *ui_TempToggleBtn  = NULL;
lv_obj_t *ui_HumToggleBtn   = NULL;
lv_obj_t *ui_PhotoToggleBtn = NULL;

// --- EXTERN CALLBACKS FROM UITASK.CPP ---
extern void Settings_cb(lv_event_t *e);
extern void AlarmButton_cb(lv_event_t *e);
extern void SPO2Button_cb(lv_event_t *e);
extern void ChartButton_cb(lv_event_t *e);
extern void ImgButton1_Lock_cb(lv_event_t *e);
extern void MuteAlarm_cb(lv_event_t *e);
extern void Switch_cb(lv_event_t *e);
extern void Label9_cb(lv_event_t *e);
extern void Label15_cb(lv_event_t *e);
extern void Label13_cb(lv_event_t *e);
extern void Label16_cb(lv_event_t *e);
extern void Label10_cb(lv_event_t *e);
extern void Label17_cb(lv_event_t *e);
extern void WifiButton_cb(lv_event_t *e);
extern void InfoButton_cb(lv_event_t *e);
void LanguageButton_cb(lv_event_t *e);
extern void TextArea_focus_cb(lv_event_t *e);
extern void TextArea_Change_cb(lv_event_t *e);
extern void Keyboard_cb(lv_event_t *e);
extern void WifiConnectButton_cb(lv_event_t *e);
extern void WifiDisconnectButton_cb(lv_event_t *e);
extern void LanguagesDropDown_cb(lv_event_t *e);
extern void AlarmsTabview_cb(lv_event_t *e);
extern void Alarm1Cont_cb(lv_event_t *e);
extern void Alarm2Cont_cb(lv_event_t *e);
extern void Alarm3Cont_cb(lv_event_t *e);
extern void Alarm4Cont_cb(lv_event_t *e);
extern void LockScreenAnyTouch_cb(lv_event_t *e);
extern void PhotoTimeMinusBtn_cb(lv_event_t *e);
extern void PhotoTimePlusBtn_cb(lv_event_t *e);
extern void PhotoStartBtn_cb(lv_event_t *e);
extern void PhotoCancelBtn_cb(lv_event_t *e);
extern void AutoAirBtn_cb(lv_event_t *e);
extern void aa_weight_dec_cb(lv_event_t *e);
extern void aa_weight_inc_cb(lv_event_t *e);
extern void aa_gest_dec_cb(lv_event_t *e);
extern void aa_gest_inc_cb(lv_event_t *e);
extern void aa_days_dec_cb(lv_event_t *e);
extern void aa_days_inc_cb(lv_event_t *e);
extern void aa_confirm_cb(lv_event_t *e);
extern void aa_cancel_cb(lv_event_t *e);
extern void aa_setpoint_up_cb(lv_event_t *e);
extern void aa_setpoint_down_cb(lv_event_t *e);
extern void aa_bar_drag_cb(lv_event_t *e);
extern void photo_turnon_cb(lv_event_t *e);
extern void photo_safety_overlay_cb(lv_event_t *e);
extern void TempToggleBtn_cb(lv_event_t *e);
extern void HumToggleBtn_cb(lv_event_t *e);
extern void PhotoToggleBtn_cb(lv_event_t *e);

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void ui_event_Settings(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Settings_cb(e);
    hmi_msg.shouldSendData = true;
  }
}

void ui_event_AlarmButton(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    hmi_msg.shouldSendData =
        true; // Beep en motherboard al tocar icono de alarmas
    _ui_screen_change(&ui_ScreenAlarms, LV_SCR_LOAD_ANIM_FADE_ON, ANIM_TIME_MS,
                      0, &ui_ScreenAlarms_screen_init);
    AlarmButton_cb(e);
  }
}

void ui_event_SPO2Button(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_change(&ui_ScreenPulseOxi, LV_SCR_LOAD_ANIM_FADE_ON,
                      ANIM_TIME_MS, 0, &ui_ScreenPulseOxi_screen_init);
    SPO2Button_cb(e);
  }
}

void ui_event_ChartButton(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_change(&ui_ScreenCharts, LV_SCR_LOAD_ANIM_FADE_ON, ANIM_TIME_MS,
                      0, &ui_ScreenCharts_screen_init);
    ChartButton_cb(e);
  }
}

void ui_event_ImgButton1(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_change(&ui_ScreenLock, LV_SCR_LOAD_ANIM_FADE_ON, ANIM_TIME_MS, 0,
                      &ui_ScreenLock_screen_init);
    ImgButton1_Lock_cb(e);
  }
}

void ui_event_ImgButton7(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, ANIM_TIME_MS, 0,
                      &ui_ScreenMain_screen_init);
    hmi_msg.shouldSendData = true;
  }
}

void ui_event_ImgButton8(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, ANIM_TIME_MS, 0,
                      &ui_ScreenMain_screen_init);
  }
}

void ui_event_ImgButton9(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, ANIM_TIME_MS, 0,
                      &ui_ScreenMain_screen_init);
  }
}

void ui_event_ImgButton2(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    _ui_screen_change(&ui_ScreenMain, LV_SCR_LOAD_ANIM_FADE_ON, ANIM_TIME_MS, 0,
                      &ui_ScreenMain_screen_init);
    hmi_msg.shouldSendData = true;
  }
}

void ui_event_AlarmLockImg(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    hmi_msg.shouldSendData =
        true; // Beep en motherboard al tocar icono de alarmas
    _ui_screen_change(&ui_ScreenAlarms, LV_SCR_LOAD_ANIM_FADE_ON, ANIM_TIME_MS,
                      0, &ui_ScreenAlarms_screen_init);
    AlarmButton_cb(e);
  }
}

void ui_event_MuteAlarm(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    MuteAlarm_cb(e);
  }
}

void ui_event_Switch1(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    Switch_cb(e);
  }
}

void ui_event_Switch2(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    Switch_cb(e);
  }
}

void ui_event_Switch3(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    Switch_cb(e);
  }
}

void ui_event_Label9(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Label9_cb(e);
  }
}

void ui_event_Label15(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Label15_cb(e);
  }
}

void ui_event_Label13(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Label13_cb(e);
  }
}

void ui_event_Label16(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Label16_cb(e);
  }
}

void ui_event_Label10(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Label10_cb(e);
  }
}

void ui_event_Label17(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Label17_cb(e);
  }
}

void ui_event_WifiButton(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    WifiButton_cb(e);
  }
}

void ui_event_LanguagesButton(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    LanguageButton_cb(e);
  }
}

void ui_event_InfoButton(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    InfoButton_cb(e);
  }
}

void ui_event_Switch4(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    Switch_cb(e);
  }
}

void ui_event_SwitchDarkMode(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    Switch_cb(e);
  }
}

void ui_event_SwitchHumidityMode(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    Switch_cb(e);
  }
}

void ui_event_TextArea1(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    TextArea_focus_cb(e);
  }
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    TextArea_Change_cb(e);
  }
}

void ui_event_TextArea2(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    TextArea_focus_cb(e);
  }
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    TextArea_Change_cb(e);
  }
}

void ui_event_Keyboard1(lv_event_t *e) { Keyboard_cb(e); }

void ui_event_WifiConnectButton(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    WifiConnectButton_cb(e);
  }
}

void ui_event_WifiDisconnectButton(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    WifiDisconnectButton_cb(e);
  }
}

void ui_event_LanguagesDropDown(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    LanguagesDropDown_cb(e);
  }
}

void ui_event_AlarmsTabview(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_VALUE_CHANGED) {
    AlarmsTabview_cb(e);
  }
}

void ui_event_Alarm1Cont(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Alarm1Cont_cb(e);
  }
}

void ui_event_Alarm2Cont(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Alarm2Cont_cb(e);
  }
}

void ui_event_Alarm3Cont(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Alarm3Cont_cb(e);
  }
}

void ui_event_Alarm4Cont(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    Alarm4Cont_cb(e);
  }
}

void ui_event_ScreenLock(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_PRESSED) {
    LockScreenAnyTouch_cb(e);
  }
}

void ui_event_AlarmLockCont(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    hmi_msg.shouldSendData =
        true; // Beep en motherboard al tocar icono de alarmas
    _ui_screen_change(&ui_ScreenAlarms, LV_SCR_LOAD_ANIM_FADE_ON, ANIM_TIME_MS,
                      0, &ui_ScreenAlarms_screen_init);
    AlarmButton_cb(e);
  }
}

void ui_event_PhotoTimeMinusBtn(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  lv_obj_t *target = lv_event_get_target(e);
  if (event_code == LV_EVENT_PRESSED) {
    lv_obj_set_style_transform_zoom(target, ZOOM_PRESSED,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
  } else if (event_code == LV_EVENT_RELEASED ||
             event_code == LV_EVENT_PRESS_LOST) {
    lv_obj_set_style_transform_zoom(target, ZOOM_NORMAL,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
  } else if (event_code == LV_EVENT_CLICKED) {
    PhotoTimeMinusBtn_cb(e);
  }
}

void ui_event_PhotoTimePlusBtn(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  lv_obj_t *target = lv_event_get_target(e);
  if (event_code == LV_EVENT_PRESSED) {
    lv_obj_set_style_transform_zoom(target, ZOOM_PRESSED,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
  } else if (event_code == LV_EVENT_RELEASED ||
             event_code == LV_EVENT_PRESS_LOST) {
    lv_obj_set_style_transform_zoom(target, ZOOM_NORMAL,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
  } else if (event_code == LV_EVENT_CLICKED) {
    PhotoTimePlusBtn_cb(e);
  }
}

void ui_event_PhotoStartBtn(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    PhotoStartBtn_cb(e);
  }
}

void ui_event_PhotoCancelBtn(lv_event_t *e) {
  lv_event_code_t event_code = lv_event_get_code(e);
  if (event_code == LV_EVENT_CLICKED) {
    PhotoCancelBtn_cb(e);
  }
}

// ============================================================================
// SCREEN INITIALIZATION
// ============================================================================

void ui_ScreenIntro_screen_init(void) {
  ui_ScreenIntro = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenIntro, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_ScreenIntro, lv_color_hex(0xBFBFBF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_ScreenIntro, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Baby logo (251x168): fills space above the centered incunest2 logo
  ui_ImageBabyLogo = lv_img_create(ui_ScreenIntro);
  lv_img_set_src(ui_ImageBabyLogo, &ui_img_incunest_baby_logo_png);
  lv_obj_set_width(ui_ImageBabyLogo, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ImageBabyLogo, LV_SIZE_CONTENT);
  lv_obj_align(ui_ImageBabyLogo, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_add_flag(ui_ImageBabyLogo, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ImageBabyLogo, LV_OBJ_FLAG_SCROLLABLE);

  // Existing logo (500x103): restored to original centered position
  ui_ImageLogoIncunest = lv_img_create(ui_ScreenIntro);
  lv_img_set_src(ui_ImageLogoIncunest, &ui_img_incunest2_png);
  lv_obj_set_width(ui_ImageLogoIncunest, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ImageLogoIncunest, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_ImageLogoIncunest, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_ImageLogoIncunest, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ImageLogoIncunest, LV_OBJ_FLAG_SCROLLABLE);

#if INTRO_FLAG != INTRO_FLAG_NONE
  ui_ImageIntroFlag = lv_img_create(ui_ScreenIntro);
#if INTRO_FLAG == INTRO_FLAG_RASD
  lv_img_set_src(ui_ImageIntroFlag, &ui_img_flag_rasd_jpg);
#elif INTRO_FLAG == INTRO_FLAG_TOGO
  lv_img_set_src(ui_ImageIntroFlag, &ui_img_flag_togo_png);
#elif INTRO_FLAG == INTRO_FLAG_SENEGAL
  lv_img_set_src(ui_ImageIntroFlag, &ui_img_flag_senegal_png);
#endif
  lv_obj_set_width(ui_ImageIntroFlag, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ImageIntroFlag, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_ImageIntroFlag, LV_ALIGN_CENTER);
  lv_obj_set_x(ui_ImageIntroFlag, 0);
  lv_obj_set_y(ui_ImageIntroFlag, 127);
  lv_obj_add_flag(ui_ImageIntroFlag, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ImageIntroFlag, LV_OBJ_FLAG_SCROLLABLE);
#endif

  // SJD logo – bottom-left below IncuNest logo (155x130 px)
  // ui_ImageSJD = lv_img_create(ui_ScreenIntro);
  // lv_img_set_src(ui_ImageSJD, &ui_img_sjd_png);
  // lv_obj_set_width(ui_ImageSJD, LV_SIZE_CONTENT);
  // lv_obj_set_height(ui_ImageSJD, LV_SIZE_CONTENT);
  // lv_obj_set_align(ui_ImageSJD, LV_ALIGN_CENTER);
  // lv_obj_set_x(ui_ImageSJD, -200);
  // lv_obj_set_y(ui_ImageSJD, 136);
  // lv_obj_add_flag(ui_ImageSJD, LV_OBJ_FLAG_ADV_HITTEST);
  // lv_obj_clear_flag(ui_ImageSJD, LV_OBJ_FLAG_SCROLLABLE);

  // Flag Togo – bottom-right below IncuNest logo (210x130 px)
  // ui_ImageFlagTogo = lv_img_create(ui_ScreenIntro);
  // lv_img_set_src(ui_ImageFlagTogo, &ui_img_flag_togo_png);
  // lv_obj_set_width(ui_ImageFlagTogo, LV_SIZE_CONTENT);
  // lv_obj_set_height(ui_ImageFlagTogo, LV_SIZE_CONTENT);
  // lv_obj_set_align(ui_ImageFlagTogo, LV_ALIGN_CENTER);
  // lv_obj_set_x(ui_ImageFlagTogo, 200);
  // lv_obj_set_y(ui_ImageFlagTogo, 136);
  // lv_obj_add_flag(ui_ImageFlagTogo, LV_OBJ_FLAG_ADV_HITTEST);
  // lv_obj_clear_flag(ui_ImageFlagTogo, LV_OBJ_FLAG_SCROLLABLE);

  // Firmware version label — bottom-right of intro screen
  lv_obj_t *ui_IntroFWLabel = lv_label_create(ui_ScreenIntro);
  lv_label_set_text(ui_IntroFWLabel, "v" FWversion);
  lv_obj_align(ui_IntroFWLabel, LV_ALIGN_BOTTOM_RIGHT, -10, -8);
  lv_obj_set_style_text_font(ui_IntroFWLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ui_IntroFWLabel, lv_color_hex(0x555555),
                              LV_PART_MAIN);
}

void ui_ScreenMain_screen_init(void) {
  ui_ScreenMain = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenMain, LV_OBJ_FLAG_SCROLLABLE);

  ui_Incunest = lv_label_create(ui_ScreenMain);
  lv_obj_set_width(ui_Incunest, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Incunest, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Incunest, -250);
  lv_obj_set_y(ui_Incunest, -213);
  lv_obj_set_align(ui_Incunest, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Incunest, "IncuNest");
  lv_obj_set_style_text_font(ui_Incunest, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Settings = lv_imgbtn_create(ui_ScreenMain);
  lv_imgbtn_set_src(ui_Settings, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_296721678, NULL);
  lv_imgbtn_set_src(ui_Settings, LV_IMGBTN_STATE_PRESSED, NULL,
                    &ui_img_296721678, NULL);
  lv_obj_set_width(ui_Settings, 50);
  lv_obj_set_height(ui_Settings, 48);
  lv_obj_set_x(ui_Settings, 343);
  lv_obj_set_y(ui_Settings, -215);
  lv_obj_set_align(ui_Settings, LV_ALIGN_CENTER);

  ui_AlarmButton = lv_imgbtn_create(ui_ScreenMain);
  lv_imgbtn_set_src(ui_AlarmButton, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_1007688293, NULL);
  lv_obj_set_width(ui_AlarmButton, 48);
  lv_obj_set_height(ui_AlarmButton, 47);
  lv_obj_set_x(ui_AlarmButton, 258);
  lv_obj_set_y(ui_AlarmButton, -212);
  lv_obj_set_align(ui_AlarmButton, LV_ALIGN_CENTER);

  ui_TempCont = lv_obj_create(ui_ScreenMain);
  lv_obj_remove_style_all(ui_TempCont);
  lv_obj_set_width(ui_TempCont, 381);
  lv_obj_set_height(ui_TempCont, 421);
  lv_obj_set_x(ui_TempCont, -200);
  lv_obj_set_y(ui_TempCont, 24);
  lv_obj_set_align(ui_TempCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_TempCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Panel1 = lv_obj_create(ui_TempCont);
  lv_obj_set_width(ui_Panel1, 376);
  lv_obj_set_height(ui_Panel1, 420);
  lv_obj_set_x(ui_Panel1, 0);
  lv_obj_set_y(ui_Panel1, 24);
  lv_obj_set_align(ui_Panel1, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel1, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_Panel1, COLOR_PANEL_GRAY,
                            LV_PART_MAIN); // Default Gray

  ui_Panel4 = lv_obj_create(ui_TempCont);
  lv_obj_set_width(ui_Panel4, 376);
  lv_obj_set_height(ui_Panel4, 50);
  lv_obj_set_x(ui_Panel4, 0);
  lv_obj_set_y(ui_Panel4, -186);
  lv_obj_set_align(ui_Panel4, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel4, LV_OBJ_FLAG_SCROLLABLE);

  ui_Switch1 = lv_switch_create(ui_TempCont);
  lv_obj_set_width(ui_Switch1, 100);
  lv_obj_set_height(ui_Switch1, 39);
  lv_obj_set_x(ui_Switch1, 95);
  lv_obj_set_y(ui_Switch1, -187);
  lv_obj_set_align(ui_Switch1, LV_ALIGN_CENTER);

  ui_Label2 = lv_label_create(ui_TempCont);
  lv_obj_set_width(ui_Label2, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label2, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label2, 20);
  lv_obj_set_y(ui_Label2, -186);
  lv_obj_set_align(ui_Label2, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_Label2, "Temperature control");

  ui_AirPanelCont = lv_obj_create(ui_TempCont);
  lv_obj_remove_style_all(ui_AirPanelCont);
  lv_obj_set_width(ui_AirPanelCont, 354);
  lv_obj_set_height(ui_AirPanelCont, 136);
  lv_obj_set_x(ui_AirPanelCont, 0);
  lv_obj_set_y(ui_AirPanelCont, -83);
  lv_obj_set_align(ui_AirPanelCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_AirPanelCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_AirPanel = lv_obj_create(ui_AirPanelCont);
  lv_obj_set_width(ui_AirPanel, 350);
  lv_obj_set_height(ui_AirPanel, 127);
  lv_obj_set_align(ui_AirPanel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_AirPanel, LV_OBJ_FLAG_SCROLLABLE);

  ui_TempAirDetected = lv_label_create(ui_AirPanelCont);
  lv_obj_set_width(ui_TempAirDetected, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TempAirDetected, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_TempAirDetected, -115);
  lv_obj_set_y(ui_TempAirDetected, 15);
  lv_obj_set_align(ui_TempAirDetected, LV_ALIGN_CENTER);
  lv_label_set_text(ui_TempAirDetected, "25.5");
  lv_obj_set_style_text_font(ui_TempAirDetected, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Label30 = lv_label_create(ui_AirPanelCont);
  lv_obj_set_width(ui_Label30, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label30, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label30, -50);
  lv_obj_set_y(ui_Label30, -42);
  lv_obj_set_align(ui_Label30, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label30, "Air");
  lv_obj_set_style_text_font(ui_Label30, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Image4 = lv_img_create(ui_AirPanelCont);
  lv_img_set_src(ui_Image4, &ui_img_1084506651);
  lv_obj_set_width(ui_Image4, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Image4, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Image4, 0);
  lv_obj_set_y(ui_Image4, -42);
  lv_obj_set_align(ui_Image4, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_Image4, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_Image4, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *ui_ArrowAirPanel = lv_img_create(ui_AirPanelCont);
  lv_img_set_src(ui_ArrowAirPanel, &ui_img_flecha_png);
  lv_obj_set_width(ui_ArrowAirPanel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ArrowAirPanel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ArrowAirPanel, -15);
  lv_obj_set_y(ui_ArrowAirPanel, 15);
  lv_obj_set_align(ui_ArrowAirPanel, LV_ALIGN_CENTER);
  lv_img_set_zoom(ui_ArrowAirPanel, 150);
  lv_obj_set_style_img_recolor(ui_ArrowAirPanel, lv_color_hex(0x303030),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(ui_ArrowAirPanel, LV_OPA_COVER,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui_ArrowAirPanel, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ArrowAirPanel, LV_OBJ_FLAG_SCROLLABLE);

  ui_AirTempBarCont = lv_obj_create(ui_AirPanelCont);
  lv_obj_remove_style_all(ui_AirTempBarCont);
  lv_obj_set_width(ui_AirTempBarCont, 120);
  lv_obj_set_height(ui_AirTempBarCont, 108);
  lv_obj_set_x(ui_AirTempBarCont, 105);
  lv_obj_set_y(ui_AirTempBarCont, -1);
  lv_obj_set_align(ui_AirTempBarCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_AirTempBarCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_TempAirDetectedRight = lv_label_create(ui_AirTempBarCont);
  lv_obj_set_width(ui_TempAirDetectedRight, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TempAirDetectedRight, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_TempAirDetectedRight, LV_ALIGN_CENTER);
  lv_label_set_text(ui_TempAirDetectedRight, "25.5");
  lv_obj_set_style_text_font(ui_TempAirDetectedRight, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui_TempAirDetectedRight, LV_OBJ_FLAG_HIDDEN);

  ui_TempAirDesired = lv_label_create(ui_AirTempBarCont);
  lv_obj_set_width(ui_TempAirDesired, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TempAirDesired, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_TempAirDesired, 0);
  lv_obj_set_y(ui_TempAirDesired, 16);
  lv_obj_set_align(ui_TempAirDesired, LV_ALIGN_CENTER);
  lv_label_set_text(ui_TempAirDesired, "25.1");
  lv_obj_set_style_text_font(ui_TempAirDesired, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_AirTempBar = lv_bar_create(ui_AirTempBarCont);
  lv_bar_set_range(ui_AirTempBar, HUM_BAR_MIN, (int)TEMP_BAR_DISPLAY_MAX);
  lv_bar_set_value(ui_AirTempBar, 25, LV_ANIM_OFF);
  lv_bar_set_start_value(ui_AirTempBar, HUM_BAR_MIN, LV_ANIM_OFF);
  lv_obj_set_width(ui_AirTempBar, 13);
  lv_obj_set_height(ui_AirTempBar, 55);
  lv_obj_set_align(ui_AirTempBar, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_AirTempBar, LV_OBJ_FLAG_HIDDEN);

  ui_Image6 = lv_img_create(ui_AirTempBarCont);
  lv_img_set_src(ui_Image6, &ui_img_1370137984);
  lv_obj_set_width(ui_Image6, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Image6, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_Image6, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_Image6, LV_OBJ_FLAG_HIDDEN);

  ui_SkinPanelCont = lv_obj_create(ui_TempCont);
  lv_obj_remove_style_all(ui_SkinPanelCont);
  lv_obj_set_width(ui_SkinPanelCont, 359);
  lv_obj_set_height(ui_SkinPanelCont, 134);
  lv_obj_set_x(ui_SkinPanelCont, 0);
  lv_obj_set_y(ui_SkinPanelCont, 138);
  lv_obj_set_align(ui_SkinPanelCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_SkinPanelCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_SkinPanel = lv_obj_create(ui_SkinPanelCont);
  lv_obj_set_width(ui_SkinPanel, 350);
  lv_obj_set_height(ui_SkinPanel, 127);
  lv_obj_set_align(ui_SkinPanel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_SkinPanel, LV_OBJ_FLAG_SCROLLABLE);

  ui_Label31 = lv_label_create(ui_SkinPanelCont);
  lv_obj_set_width(ui_Label31, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label31, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label31, -50);
  lv_obj_set_y(ui_Label31, -42);
  lv_obj_set_align(ui_Label31, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label31, "Skin");
  lv_obj_set_style_text_font(ui_Label31, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Image2 = lv_img_create(ui_SkinPanelCont);
  lv_img_set_src(ui_Image2, &ui_img_bebe_icon_png);
  lv_obj_set_width(ui_Image2, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Image2, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Image2, 25);
  lv_obj_set_y(ui_Image2, -42);
  lv_obj_set_align(ui_Image2, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_Image2, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_Image2, LV_OBJ_FLAG_SCROLLABLE);

  ui_TempSkinDetected = lv_label_create(ui_SkinPanelCont);
  lv_obj_set_width(ui_TempSkinDetected, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TempSkinDetected, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_TempSkinDetected, -115);
  lv_obj_set_y(ui_TempSkinDetected, 15);
  lv_obj_set_align(ui_TempSkinDetected, LV_ALIGN_CENTER);
  lv_label_set_text(ui_TempSkinDetected, "28.1");
  lv_obj_set_style_text_font(ui_TempSkinDetected, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *ui_ArrowSkinPanel = lv_img_create(ui_SkinPanelCont);
  lv_img_set_src(ui_ArrowSkinPanel, &ui_img_flecha_png);
  lv_obj_set_width(ui_ArrowSkinPanel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ArrowSkinPanel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ArrowSkinPanel, -15);
  lv_obj_set_y(ui_ArrowSkinPanel, 15);
  lv_obj_set_align(ui_ArrowSkinPanel, LV_ALIGN_CENTER);
  lv_img_set_zoom(ui_ArrowSkinPanel, 150);
  lv_obj_set_style_img_recolor(ui_ArrowSkinPanel, lv_color_hex(0x303030),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(ui_ArrowSkinPanel, LV_OPA_COVER,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui_ArrowSkinPanel, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ArrowSkinPanel, LV_OBJ_FLAG_SCROLLABLE);

  ui_SkinTempBarCont = lv_obj_create(ui_SkinPanelCont);
  lv_obj_remove_style_all(ui_SkinTempBarCont);
  lv_obj_set_width(ui_SkinTempBarCont, 120);
  lv_obj_set_height(ui_SkinTempBarCont, 108);
  lv_obj_set_x(ui_SkinTempBarCont, 105);
  lv_obj_set_y(ui_SkinTempBarCont, -1);
  lv_obj_set_align(ui_SkinTempBarCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_SkinTempBarCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_SkinTempBar = lv_bar_create(ui_SkinTempBarCont);
  lv_bar_set_range(ui_SkinTempBar, HUM_BAR_MIN, (int)TEMP_BAR_DISPLAY_MAX);
  lv_bar_set_value(ui_SkinTempBar, 25, LV_ANIM_OFF);
  lv_bar_set_start_value(ui_SkinTempBar, HUM_BAR_MIN, LV_ANIM_OFF);
  lv_obj_set_width(ui_SkinTempBar, 13);
  lv_obj_set_height(ui_SkinTempBar, 55);
  lv_obj_set_align(ui_SkinTempBar, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_color(ui_SkinTempBar, lv_color_hex(0xC8DDE0),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_SkinTempBar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_SkinTempBar, lv_color_hex(0x0075EE),
                            LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_SkinTempBar, 255,
                          LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui_SkinTempBar, LV_OBJ_FLAG_HIDDEN);

  ui_Image1 = lv_img_create(ui_SkinTempBarCont);
  lv_img_set_src(ui_Image1, &ui_img_1370137984);
  lv_obj_set_width(ui_Image1, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Image1, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_Image1, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_ADV_HITTEST | LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_Image1, LV_OBJ_FLAG_SCROLLABLE);

  ui_TempSkinDetectedRight = lv_label_create(ui_SkinTempBarCont);
  lv_obj_set_width(ui_TempSkinDetectedRight, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TempSkinDetectedRight, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_TempSkinDetectedRight, LV_ALIGN_CENTER);
  lv_label_set_text(ui_TempSkinDetectedRight, "28.1");
  lv_obj_set_style_text_font(ui_TempSkinDetectedRight, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui_TempSkinDetectedRight, LV_OBJ_FLAG_HIDDEN);

  // Reparented to ScreenMain for Z-Index priority
  ui_HeaterErrorTempCont = lv_obj_create(ui_ScreenMain);
  lv_obj_remove_style_all(ui_HeaterErrorTempCont);
  lv_obj_set_width(ui_HeaterErrorTempCont, 381);
  lv_obj_set_height(ui_HeaterErrorTempCont, 421);
  lv_obj_set_x(ui_HeaterErrorTempCont, -200);
  lv_obj_set_y(ui_HeaterErrorTempCont, 24);
  lv_obj_set_align(ui_HeaterErrorTempCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_HeaterErrorTempCont,
                  LV_OBJ_FLAG_HIDDEN); // Initially hidden
  lv_obj_add_flag(ui_HeaterErrorTempCont,
                  LV_OBJ_FLAG_CLICKABLE); // Enable clicking
  lv_obj_clear_flag(ui_HeaterErrorTempCont, LV_OBJ_FLAG_SCROLLABLE);

  ui_HeaterErrorTempImg = lv_img_create(ui_HeaterErrorTempCont);
  lv_img_set_src(ui_HeaterErrorTempImg, &ui_img_1007688293);
  lv_obj_set_width(ui_HeaterErrorTempImg, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HeaterErrorTempImg, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_HeaterErrorTempImg, LV_ALIGN_CENTER);
  lv_obj_set_style_img_recolor(ui_HeaterErrorTempImg, lv_color_hex(0xFF0000),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(ui_HeaterErrorTempImg, 0,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_img_set_zoom(ui_HeaterErrorTempImg, 1024); // Much larger (4x)

  ui_HeaterErrorTempLabel = lv_label_create(ui_HeaterErrorTempCont);
  lv_obj_set_width(ui_HeaterErrorTempLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HeaterErrorTempLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_HeaterErrorTempLabel, 0);
  lv_obj_set_y(ui_HeaterErrorTempLabel, 120); // Adjusted for larger image
  lv_obj_set_align(ui_HeaterErrorTempLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_HeaterErrorTempLabel,
                    "Heater error \nTouch for more information");
  lv_obj_set_style_text_color(ui_HeaterErrorTempLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT); // White text
  lv_obj_set_style_bg_color(ui_HeaterErrorTempLabel, lv_color_hex(0xFF0000),
                            LV_PART_MAIN | LV_STATE_DEFAULT); // Red background
  lv_obj_set_style_bg_opa(ui_HeaterErrorTempLabel, 255,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_HeaterErrorTempLabel, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(ui_HeaterErrorTempLabel, 10,
                           LV_PART_MAIN | LV_STATE_DEFAULT); // Padding
  lv_obj_set_style_text_align(ui_HeaterErrorTempLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(ui_HeaterErrorTempLabel, LV_OBJ_FLAG_CLICKABLE);

  ui_TempSkinDesired = lv_label_create(ui_SkinTempBarCont);
  lv_obj_set_width(ui_TempSkinDesired, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TempSkinDesired, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_TempSkinDesired, 0);
  lv_obj_set_y(ui_TempSkinDesired, 16);
  lv_obj_set_align(ui_TempSkinDesired, LV_ALIGN_CENTER);
  lv_label_set_text(ui_TempSkinDesired, "28.3");
  lv_obj_set_style_text_font(ui_TempSkinDesired, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Label6 = lv_label_create(ui_TempCont);
  lv_obj_set_width(ui_Label6, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label6, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label6, 80);
  lv_obj_set_y(ui_Label6, 25);
  lv_obj_set_align(ui_Label6, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label6, "Set");

  ui_Label9 = lv_label_create(ui_TempCont);
  lv_obj_set_width(ui_Label9, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label9, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label9, 165);
  lv_obj_set_y(ui_Label9, -187);
  lv_obj_set_align(ui_Label9, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label9, "ON");

  ui_Label15 = lv_label_create(ui_TempCont);
  lv_obj_set_width(ui_Label15, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label15, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label15, 17);
  lv_obj_set_y(ui_Label15, -187);
  lv_obj_set_align(ui_Label15, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label15, "OFF");

  ui_ArrowDownTemp = lv_obj_create(ui_TempCont);
  lv_obj_set_width(ui_ArrowDownTemp, 60);
  lv_obj_set_height(ui_ArrowDownTemp, 60);
  lv_obj_set_x(ui_ArrowDownTemp, 12);
  lv_obj_set_y(ui_ArrowDownTemp, 26);
  lv_obj_set_align(ui_ArrowDownTemp, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_ArrowDownTemp, LV_OBJ_FLAG_SCROLLABLE);

  ui_ArrowUpTemp = lv_obj_create(ui_TempCont);
  lv_obj_set_width(ui_ArrowUpTemp, 60);
  lv_obj_set_height(ui_ArrowUpTemp, 60);
  lv_obj_set_x(ui_ArrowUpTemp, 147);
  lv_obj_set_y(ui_ArrowUpTemp, 27);
  lv_obj_set_align(ui_ArrowUpTemp, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_ArrowUpTemp, LV_OBJ_FLAG_SCROLLABLE);

  ui_ImgArrowDownTemp = lv_imgbtn_create(ui_TempCont);
  lv_imgbtn_set_src(ui_ImgArrowDownTemp, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_triangulo_abajo_png, NULL);
  lv_obj_set_width(ui_ImgArrowDownTemp, 39);
  lv_obj_set_height(ui_ImgArrowDownTemp, 42);
  lv_obj_set_x(ui_ImgArrowDownTemp, 12);
  lv_obj_set_y(ui_ImgArrowDownTemp, 29);
  lv_obj_set_align(ui_ImgArrowDownTemp, LV_ALIGN_CENTER);

  ui_ImgArrowUpTemp = lv_imgbtn_create(ui_TempCont);
  lv_imgbtn_set_src(ui_ImgArrowUpTemp, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_triangulo_arriba_png, NULL);
  lv_obj_set_width(ui_ImgArrowUpTemp, 39);
  lv_obj_set_height(ui_ImgArrowUpTemp, 41);
  lv_obj_set_x(ui_ImgArrowUpTemp, 146);
  lv_obj_set_y(ui_ImgArrowUpTemp, 25);
  lv_obj_set_align(ui_ImgArrowUpTemp, LV_ALIGN_CENTER);

  ui_TempButton = lv_btn_create(ui_TempCont);
  lv_obj_set_width(ui_TempButton, 186);
  lv_obj_set_height(ui_TempButton, 30);
  lv_obj_set_x(ui_TempButton, -92);
  lv_obj_set_y(ui_TempButton, -186);
  lv_obj_set_align(ui_TempButton, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_TempButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(ui_TempButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_opa(ui_TempButton, 0,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_opa(ui_TempButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_HumCont = lv_obj_create(ui_ScreenMain);
  lv_obj_remove_style_all(ui_HumCont);
  lv_obj_set_width(ui_HumCont, 378);
  lv_obj_set_height(ui_HumCont, 248);
  lv_obj_set_x(ui_HumCont, 190);
  lv_obj_set_y(ui_HumCont, 111);
  lv_obj_set_align(ui_HumCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_HumCont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ui_HumCont, LV_OBJ_FLAG_HIDDEN); // Humidity UI disabled

  ui_Panel3 = lv_obj_create(ui_HumCont);
  lv_obj_set_width(ui_Panel3, 376);
  lv_obj_set_height(ui_Panel3, 236);
  lv_obj_set_x(ui_Panel3, 2);
  lv_obj_set_y(ui_Panel3, -4);
  lv_obj_set_align(ui_Panel3, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel3, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_Panel3, COLOR_PANEL_GRAY,
                            LV_PART_MAIN); // Default Gray

  ui_Panel6 = lv_obj_create(ui_HumCont);
  lv_obj_set_width(ui_Panel6, 376);
  lv_obj_set_height(ui_Panel6, 51);
  lv_obj_set_x(ui_Panel6, 2);
  lv_obj_set_y(ui_Panel6, -98);
  lv_obj_set_align(ui_Panel6, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel6, LV_OBJ_FLAG_SCROLLABLE);

  ui_HumPanelCont = lv_obj_create(ui_HumCont);
  lv_obj_remove_style_all(ui_HumPanelCont);
  lv_obj_set_width(ui_HumPanelCont, 358);
  lv_obj_set_height(ui_HumPanelCont, 163);
  lv_obj_set_x(ui_HumPanelCont, 0);
  lv_obj_set_y(ui_HumPanelCont, 18);
  lv_obj_set_align(ui_HumPanelCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_HumPanelCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_HumDetected = lv_label_create(ui_HumPanelCont);
  lv_obj_set_width(ui_HumDetected, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HumDetected, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_HumDetected, -145);
  lv_obj_set_y(ui_HumDetected, 14);
  lv_obj_set_align(ui_HumDetected, LV_ALIGN_CENTER);
  lv_label_set_text(ui_HumDetected, "50");
  lv_obj_set_style_text_font(ui_HumDetected, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_HumBar = lv_bar_create(ui_HumPanelCont);
  lv_bar_set_value(ui_HumBar, 25, LV_ANIM_OFF);
  lv_bar_set_start_value(ui_HumBar, 0, LV_ANIM_OFF);
  lv_obj_set_width(ui_HumBar, 47);
  lv_obj_set_height(ui_HumBar, 66);
  lv_obj_set_x(ui_HumBar, -60);
  lv_obj_set_y(ui_HumBar, 0);
  lv_obj_set_align(ui_HumBar, LV_ALIGN_CENTER);
  lv_obj_set_style_border_color(ui_HumBar, lv_color_hex(0x040404),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_opa(ui_HumBar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui_HumBar, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_side(ui_HumBar, LV_BORDER_SIDE_FULL,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Image7 = lv_img_create(ui_HumPanelCont);
  lv_img_set_src(ui_Image7, &ui_img_gota_png);
  lv_obj_set_width(ui_Image7, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Image7, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Image7, -60);
  lv_obj_set_y(ui_Image7, 0);
  lv_obj_set_align(ui_Image7, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_Image7, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_Image7, LV_OBJ_FLAG_SCROLLABLE);

  ui_ArrowUpHum = lv_obj_create(ui_HumPanelCont);
  lv_obj_set_width(ui_ArrowUpHum, 60);
  lv_obj_set_height(ui_ArrowUpHum, 60);
  lv_obj_set_x(ui_ArrowUpHum, 127);
  lv_obj_set_y(ui_ArrowUpHum, -42);
  lv_obj_set_align(ui_ArrowUpHum, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_ArrowUpHum, LV_OBJ_FLAG_SCROLLABLE);

  ui_ArrowDownHum = lv_obj_create(ui_HumPanelCont);
  lv_obj_set_width(ui_ArrowDownHum, 60);
  lv_obj_set_height(ui_ArrowDownHum, 60);
  lv_obj_set_x(ui_ArrowDownHum, 127);
  lv_obj_set_y(ui_ArrowDownHum, 53);
  lv_obj_set_align(ui_ArrowDownHum, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_ArrowDownHum, LV_OBJ_FLAG_SCROLLABLE);

  // Reparented to ScreenMain for Z-Index priority
  ui_HeaterErrorHumCont = lv_obj_create(ui_ScreenMain);
  lv_obj_remove_style_all(ui_HeaterErrorHumCont);
  lv_obj_set_width(ui_HeaterErrorHumCont, 378);
  lv_obj_set_height(ui_HeaterErrorHumCont, 320);
  lv_obj_set_x(ui_HeaterErrorHumCont, 193);
  lv_obj_set_y(ui_HeaterErrorHumCont, -64);
  lv_obj_set_align(ui_HeaterErrorHumCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_HeaterErrorHumCont,
                  LV_OBJ_FLAG_HIDDEN); // Initially hidden
  lv_obj_add_flag(ui_HeaterErrorHumCont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(ui_HeaterErrorHumCont, LV_OBJ_FLAG_SCROLLABLE);

  ui_HeaterErrorHumImg = lv_img_create(ui_HeaterErrorHumCont);
  lv_img_set_src(ui_HeaterErrorHumImg, &ui_img_1007688293);
  lv_obj_set_width(ui_HeaterErrorHumImg, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HeaterErrorHumImg, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_HeaterErrorHumImg, LV_ALIGN_CENTER);
  lv_img_set_zoom(ui_HeaterErrorHumImg, 1024); // Much larger

  ui_HeaterErrorHumLabel = lv_label_create(ui_HeaterErrorHumCont);
  lv_obj_set_width(ui_HeaterErrorHumLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HeaterErrorHumLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_HeaterErrorHumLabel, 0);
  lv_obj_set_y(ui_HeaterErrorHumLabel, 120); // Below image
  lv_obj_set_align(ui_HeaterErrorHumLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_HeaterErrorHumLabel,
                    "Heater error \nTouch for more information");
  lv_obj_set_style_text_color(ui_HeaterErrorHumLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT); // White text
  lv_obj_set_style_bg_color(ui_HeaterErrorHumLabel, lv_color_hex(0xFF0000),
                            LV_PART_MAIN | LV_STATE_DEFAULT); // Red background
  lv_obj_set_style_bg_opa(ui_HeaterErrorHumLabel, 255,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_HeaterErrorHumLabel, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(ui_HeaterErrorHumLabel, 10,
                           LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(ui_HeaterErrorHumLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(ui_HeaterErrorHumLabel, LV_OBJ_FLAG_CLICKABLE);

  ui_HumDetectedRight = lv_label_create(ui_HumPanelCont);
  lv_obj_set_width(ui_HumDetectedRight, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HumDetectedRight, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_HumDetectedRight, 13);
  lv_obj_set_y(ui_HumDetectedRight, 33);
  lv_obj_set_align(ui_HumDetectedRight, LV_ALIGN_CENTER);
  lv_label_set_text(ui_HumDetectedRight, "50");
  lv_obj_set_style_text_font(ui_HumDetectedRight, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_HumDesired = lv_label_create(ui_HumPanelCont);
  lv_obj_set_width(ui_HumDesired, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HumDesired, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_HumDesired, 13);
  lv_obj_set_y(ui_HumDesired, -24);
  lv_obj_set_align(ui_HumDesired, LV_ALIGN_CENTER);
  lv_label_set_text(ui_HumDesired, "55");
  lv_obj_set_style_text_font(ui_HumDesired, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ImgArrowUpHum = lv_imgbtn_create(ui_HumPanelCont);
  lv_imgbtn_set_src(ui_ImgArrowUpHum, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_triangulo_arriba_png, NULL);
  lv_obj_set_width(ui_ImgArrowUpHum, 40);
  lv_obj_set_height(ui_ImgArrowUpHum, 38);
  lv_obj_set_x(ui_ImgArrowUpHum, 127);
  lv_obj_set_y(ui_ImgArrowUpHum, -46);
  lv_obj_set_align(ui_ImgArrowUpHum, LV_ALIGN_CENTER);

  ui_ImgArrowDownHum = lv_imgbtn_create(ui_HumPanelCont);
  lv_imgbtn_set_src(ui_ImgArrowDownHum, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_triangulo_abajo_png, NULL);
  lv_obj_set_width(ui_ImgArrowDownHum, 39);
  lv_obj_set_height(ui_ImgArrowDownHum, 45);
  lv_obj_set_x(ui_ImgArrowDownHum, 127);
  lv_obj_set_y(ui_ImgArrowDownHum, 55);
  lv_obj_set_align(ui_ImgArrowDownHum, LV_ALIGN_CENTER);

  ui_Label7 = lv_label_create(ui_HumPanelCont);
  lv_obj_set_width(ui_Label7, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label7, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label7, 128);
  lv_obj_set_y(ui_Label7, 4);
  lv_obj_set_align(ui_Label7, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label7, "Set");

  ui_Label16 = lv_label_create(ui_HumCont);
  lv_obj_set_width(ui_Label16, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label16, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label16, 11);
  lv_obj_set_y(ui_Label16, -98);
  lv_obj_set_align(ui_Label16, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label16, "OFF");

  ui_Label13 = lv_label_create(ui_HumCont);
  lv_obj_set_width(ui_Label13, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label13, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label13, 160);
  lv_obj_set_y(ui_Label13, -98);
  lv_obj_set_align(ui_Label13, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label13, "ON");

  ui_Switch2 = lv_switch_create(ui_HumCont);
  lv_obj_set_width(ui_Switch2, 100);
  lv_obj_set_height(ui_Switch2, 39);
  lv_obj_set_x(ui_Switch2, 90);
  lv_obj_set_y(ui_Switch2, -98);
  lv_obj_set_align(ui_Switch2, LV_ALIGN_CENTER);

  ui_HumidButton = lv_btn_create(ui_HumCont);
  lv_obj_set_width(ui_HumidButton, 162);
  lv_obj_set_height(ui_HumidButton, 33);
  lv_obj_set_x(ui_HumidButton, -94);
  lv_obj_set_y(ui_HumidButton, -97);
  lv_obj_set_align(ui_HumidButton, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_HumidButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(ui_HumidButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_opa(ui_HumidButton, 0,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_opa(ui_HumidButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_HumidityLabel = lv_label_create(ui_HumCont);
  lv_obj_set_width(ui_HumidityLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HumidityLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_HumidityLabel, 20);
  lv_obj_set_y(ui_HumidityLabel, -98);
  lv_obj_set_align(ui_HumidityLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_HumidityLabel, "Humidity control");

  ui_PhotoTimerCont = lv_obj_create(ui_ScreenMain);
  lv_obj_remove_style_all(ui_PhotoTimerCont);
  lv_obj_set_width(ui_PhotoTimerCont, 384);
  lv_obj_set_height(ui_PhotoTimerCont, 120);
  lv_obj_set_x(ui_PhotoTimerCont, 191);
  lv_obj_set_y(ui_PhotoTimerCont, -88);
  lv_obj_set_align(ui_PhotoTimerCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_PhotoTimerCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_PhotoTimerCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_PhotoTimerPanel = lv_obj_create(ui_PhotoTimerCont);
  lv_obj_set_width(ui_PhotoTimerPanel, 376);
  lv_obj_set_height(ui_PhotoTimerPanel, 110);
  lv_obj_set_align(ui_PhotoTimerPanel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_PhotoTimerPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_PhotoTimerPanel, COLOR_PANEL_GRAY,
                            LV_PART_MAIN); // Default Gray

  // Minus Button
  ui_PhotoTimeMinusBtn = lv_btn_create(ui_PhotoTimerCont);
  lv_obj_set_width(ui_PhotoTimeMinusBtn, 50);
  lv_obj_set_height(ui_PhotoTimeMinusBtn, 40);
  lv_obj_set_x(ui_PhotoTimeMinusBtn, -100);
  lv_obj_set_y(ui_PhotoTimeMinusBtn, -15);
  lv_obj_set_align(ui_PhotoTimeMinusBtn, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_color(ui_PhotoTimeMinusBtn, COLOR_PANEL_LIGHT_GRAY,
                            LV_PART_MAIN |
                                LV_STATE_DEFAULT); // Default Light Gray
  lv_obj_clear_flag(ui_PhotoTimeMinusBtn,
                    LV_OBJ_FLAG_CLICKABLE); // Default Locked

  ui_PhotoTimeMinusLabel = lv_label_create(ui_PhotoTimeMinusBtn);
  lv_obj_set_align(ui_PhotoTimeMinusLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_PhotoTimeMinusLabel, "-");
  lv_obj_set_style_text_font(ui_PhotoTimeMinusLabel, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Value Label
  ui_PhotoTimeValueLabel = lv_label_create(ui_PhotoTimerCont);
  lv_obj_set_width(ui_PhotoTimeValueLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_PhotoTimeValueLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_PhotoTimeValueLabel, 0);
  lv_obj_set_y(ui_PhotoTimeValueLabel, -15);
  lv_obj_set_align(ui_PhotoTimeValueLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_PhotoTimeValueLabel, "30 min"); // Default
  lv_obj_set_style_text_font(ui_PhotoTimeValueLabel, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Plus Button
  ui_PhotoTimePlusBtn = lv_btn_create(ui_PhotoTimerCont);
  lv_obj_set_width(ui_PhotoTimePlusBtn, 50);
  lv_obj_set_height(ui_PhotoTimePlusBtn, 40);
  lv_obj_set_x(ui_PhotoTimePlusBtn, 100);
  lv_obj_set_y(ui_PhotoTimePlusBtn, -15);
  lv_obj_set_align(ui_PhotoTimePlusBtn, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_color(ui_PhotoTimePlusBtn, COLOR_PANEL_LIGHT_GRAY,
                            LV_PART_MAIN |
                                LV_STATE_DEFAULT); // Default Light Gray
  lv_obj_clear_flag(ui_PhotoTimePlusBtn,
                    LV_OBJ_FLAG_CLICKABLE); // Default Locked

  ui_PhotoTimePlusLabel = lv_label_create(ui_PhotoTimePlusBtn);
  lv_obj_set_align(ui_PhotoTimePlusLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_PhotoTimePlusLabel, "+");
  lv_obj_set_style_text_font(ui_PhotoTimePlusLabel, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Start Button
  ui_PhotoStartBtn = lv_btn_create(ui_PhotoTimerCont);
  lv_obj_set_width(ui_PhotoStartBtn, 150);
  lv_obj_set_height(ui_PhotoStartBtn, 30);
  lv_obj_set_x(ui_PhotoStartBtn, 0);
  lv_obj_set_y(ui_PhotoStartBtn, 25);
  lv_obj_set_align(ui_PhotoStartBtn, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_color(ui_PhotoStartBtn, COLOR_PANEL_LIGHT_GRAY,
                            LV_PART_MAIN |
                                LV_STATE_DEFAULT); // Default Light Gray
  lv_obj_clear_flag(ui_PhotoStartBtn, LV_OBJ_FLAG_CLICKABLE); // Default Locked

  ui_PhotoStartLabel = lv_label_create(ui_PhotoStartBtn);
  lv_obj_set_align(ui_PhotoStartLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_PhotoStartLabel, "EMPEZAR"); // Default Spanish
  lv_obj_set_style_text_font(ui_PhotoStartLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Cancel Button (Red X)
  ui_PhotoCancelBtn = lv_btn_create(ui_PhotoTimerCont);
  lv_obj_set_width(ui_PhotoCancelBtn, 35);
  lv_obj_set_height(ui_PhotoCancelBtn, 30);
  lv_obj_set_x(ui_PhotoCancelBtn, 160); // Movido al borde derecho
  lv_obj_set_y(ui_PhotoCancelBtn, 25);
  lv_obj_set_align(ui_PhotoCancelBtn, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_color(ui_PhotoCancelBtn, lv_color_hex(0xFF0000),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui_PhotoCancelBtn, LV_OBJ_FLAG_HIDDEN); // Initially hidden

  ui_PhotoCancelLabel = lv_label_create(ui_PhotoCancelBtn);
  lv_obj_set_align(ui_PhotoCancelLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_PhotoCancelLabel, "X");
  lv_obj_set_style_text_font(ui_PhotoCancelLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_PhotoCont = lv_obj_create(ui_ScreenMain);
  lv_obj_remove_style_all(ui_PhotoCont);
  lv_obj_set_width(ui_PhotoCont, 384);
  lv_obj_set_height(ui_PhotoCont, 54);
  lv_obj_set_x(ui_PhotoCont, 193);
  lv_obj_set_y(ui_PhotoCont, -160);
  lv_obj_set_align(ui_PhotoCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_PhotoCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Panel2 = lv_obj_create(ui_PhotoCont);
  lv_obj_set_width(ui_Panel2, 376);
  lv_obj_set_height(ui_Panel2, 52);
  lv_obj_set_x(ui_Panel2, -2);
  lv_obj_set_y(ui_Panel2, 1);
  lv_obj_set_align(ui_Panel2, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel2, LV_OBJ_FLAG_SCROLLABLE);

  ui_Switch3 = lv_switch_create(ui_PhotoCont);
  lv_obj_set_width(ui_Switch3, 100);
  lv_obj_set_height(ui_Switch3, 39);
  lv_obj_set_x(ui_Switch3, 84);
  lv_obj_set_y(ui_Switch3, 1);
  lv_obj_set_align(ui_Switch3, LV_ALIGN_CENTER);

  ui_PhototherapyLabel = lv_label_create(ui_PhotoCont);
  lv_obj_set_width(ui_PhototherapyLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_PhototherapyLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_PhototherapyLabel, 20);
  lv_obj_set_y(ui_PhototherapyLabel, 1);
  lv_obj_set_align(ui_PhototherapyLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_PhototherapyLabel, "Phototherapy");

  ui_Label17 = lv_label_create(ui_PhotoCont);
  lv_obj_set_width(ui_Label17, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label17, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label17, 9);
  lv_obj_set_y(ui_Label17, 1);
  lv_obj_set_align(ui_Label17, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label17, "OFF");

  ui_Label10 = lv_label_create(ui_PhotoCont);
  lv_obj_set_width(ui_Label10, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label10, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label10, 156);
  lv_obj_set_y(ui_Label10, 1);
  lv_obj_set_align(ui_Label10, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label10, "ON");

  ui_Panel10 = lv_obj_create(ui_ScreenMain);
  lv_obj_set_width(ui_Panel10, 24);
  lv_obj_set_height(ui_Panel10, 27);
  lv_obj_set_x(ui_Panel10, 275);
  lv_obj_set_y(ui_Panel10, -228);
  lv_obj_set_align(ui_Panel10, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel10, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_Panel10, lv_color_hex(0xFF0000),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Panel10, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_NumAlarm = lv_label_create(ui_ScreenMain);
  lv_obj_set_width(ui_NumAlarm, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_NumAlarm, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_NumAlarm, 275);
  lv_obj_set_y(ui_NumAlarm, -228);
  lv_obj_set_align(ui_NumAlarm, LV_ALIGN_CENTER);
  lv_label_set_text(ui_NumAlarm, "1");
  lv_obj_set_style_text_color(ui_NumAlarm, lv_color_hex(0x000000),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_NumAlarm, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_NumAlarm, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_SPO2Button = lv_imgbtn_create(ui_ScreenMain);
  lv_imgbtn_set_src(ui_SPO2Button, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_pulse_png, NULL);
  lv_obj_set_width(ui_SPO2Button, 51);
  lv_obj_set_height(ui_SPO2Button, 47);
  lv_obj_set_x(ui_SPO2Button, 194);
  lv_obj_set_y(ui_SPO2Button, -213);
  lv_obj_set_align(ui_SPO2Button, LV_ALIGN_CENTER);

  ui_ChartButton = lv_imgbtn_create(ui_ScreenMain);
  lv_imgbtn_set_src(ui_ChartButton, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_chart_png, NULL);
  lv_obj_set_width(ui_ChartButton, 52);
  lv_obj_set_height(ui_ChartButton, 51);
  lv_obj_set_x(ui_ChartButton, 138);
  lv_obj_set_y(ui_ChartButton, -214);
  lv_obj_set_align(ui_ChartButton, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_ChartButton, LV_OBJ_FLAG_HIDDEN);

  ui_ImgButton1 = lv_imgbtn_create(ui_ScreenMain);
  lv_imgbtn_set_src(ui_ImgButton1, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_candado_png, NULL);
  lv_obj_set_width(ui_ImgButton1, 38);
  lv_obj_set_height(ui_ImgButton1, 46);
  lv_obj_set_x(ui_ImgButton1, -4);
  lv_obj_set_y(ui_ImgButton1, -214);
  lv_obj_set_align(ui_ImgButton1, LV_ALIGN_CENTER);

  ui_CheckImgMain = lv_img_create(ui_ScreenMain);
  lv_img_set_src(ui_CheckImgMain, &ui_img_check_png);
  lv_obj_set_width(ui_CheckImgMain, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_CheckImgMain, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_CheckImgMain, 259);
  lv_obj_set_y(ui_CheckImgMain, -212);
  lv_obj_set_align(ui_CheckImgMain, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_CheckImgMain, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_CheckImgMain, LV_OBJ_FLAG_SCROLLABLE);
  lv_img_set_zoom(ui_CheckImgMain, 200);

  lv_obj_add_event_cb(ui_Settings, ui_event_Settings, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_AlarmButton, ui_event_AlarmButton, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_SPO2Button, ui_event_SPO2Button, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_ChartButton, ui_event_ChartButton, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_ImgButton1, ui_event_ImgButton1, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Switch1, ui_event_Switch1, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Switch2, ui_event_Switch2, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Switch3, ui_event_Switch3, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Label9, ui_event_Label9, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Label15, ui_event_Label15, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Label13, ui_event_Label13, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Label16, ui_event_Label16, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Label10, ui_event_Label10, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Label17, ui_event_Label17, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_PhotoTimeMinusBtn, ui_event_PhotoTimeMinusBtn,
                      LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_PhotoTimePlusBtn, ui_event_PhotoTimePlusBtn,
                      LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_PhotoStartBtn, ui_event_PhotoStartBtn, LV_EVENT_ALL,
                      NULL);
  lv_obj_add_event_cb(ui_PhotoCancelBtn, ui_event_PhotoCancelBtn, LV_EVENT_ALL,
                      NULL);
  uic_Tempbutton = ui_TempButton;
  uic_HumidButton = ui_HumidButton;

  // Temperature content hidden at boot — shown when Switch1 is turned ON
  lv_obj_add_flag(ui_Panel1,           LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_AirPanelCont,     LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_SkinPanelCont,    LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowDownTemp,    LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ArrowUpTemp,      LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ImgArrowDownTemp, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ImgArrowUpTemp,   LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Label6,           LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_TempButton,       LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_ext_click_area(ui_Settings, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_AlarmButton, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_SPO2Button, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_ChartButton, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_ImgButton1, TOUCH_EXT_SMALL);
  lv_obj_set_ext_click_area(ui_ArrowDownTemp, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_ArrowUpTemp, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_ArrowUpHum, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_ArrowDownHum, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_Switch1, TOUCH_EXT_NARROW);
  lv_obj_set_ext_click_area(ui_Switch2, TOUCH_EXT_NARROW);
  lv_obj_set_ext_click_area(ui_Switch3, TOUCH_EXT_NARROW);
  lv_obj_set_ext_click_area(ui_PhotoTimeMinusBtn, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_PhotoTimePlusBtn, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_PhotoStartBtn, TOUCH_EXT_NARROW);
  lv_obj_set_ext_click_area(ui_PhotoCancelBtn, TOUCH_EXT_SMALL);
  lv_obj_set_ext_click_area(ui_Label9, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_Label15, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_Label13, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_Label16, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_Label10, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_Label17, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_TempButton, TOUCH_EXT_NARROW);
  lv_obj_set_ext_click_area(ui_HumidButton, TOUCH_EXT_NARROW);
}

void ui_ScreenAlarms_screen_init(void) {
  ui_ScreenAlarms = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenAlarms, LV_OBJ_FLAG_SCROLLABLE);

  ui_ImgButton7 = lv_imgbtn_create(ui_ScreenAlarms);
  lv_imgbtn_set_src(ui_ImgButton7, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_1508956403, NULL);
  lv_obj_set_width(ui_ImgButton7, 50);
  lv_obj_set_height(ui_ImgButton7, 52);
  lv_obj_set_x(ui_ImgButton7, -360);
  lv_obj_set_y(ui_ImgButton7, -204);
  lv_obj_set_align(ui_ImgButton7, LV_ALIGN_CENTER);

  ui_Panel5 = lv_obj_create(ui_ScreenAlarms);
  lv_obj_set_width(ui_Panel5, 768);
  lv_obj_set_height(ui_Panel5, 396);
  lv_obj_set_x(ui_Panel5, 17);
  lv_obj_set_y(ui_Panel5, 71);
  lv_obj_clear_flag(ui_Panel5, LV_OBJ_FLAG_SCROLLABLE);

  ui_AlarmsTabview = lv_tabview_create(ui_ScreenAlarms, LV_DIR_TOP, 30);
  lv_obj_set_width(ui_AlarmsTabview, 743);
  lv_obj_set_height(ui_AlarmsTabview, 364);
  lv_obj_set_x(ui_AlarmsTabview, 0);
  lv_obj_set_y(ui_AlarmsTabview, 32);
  lv_obj_set_align(ui_AlarmsTabview, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_AlarmsTabview, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(ui_AlarmsTabview, 1,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_side(ui_AlarmsTabview, LV_BORDER_SIDE_FULL,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_TabPage1 = lv_tabview_add_tab(ui_AlarmsTabview, "");

  ui_Alarm1Cont = lv_obj_create(ui_TabPage1);
  lv_obj_remove_style_all(ui_Alarm1Cont);
  lv_obj_set_width(ui_Alarm1Cont, 712);
  lv_obj_set_height(ui_Alarm1Cont, 50);
  lv_obj_set_x(ui_Alarm1Cont, 0);
  lv_obj_set_y(ui_Alarm1Cont, -115);
  lv_obj_set_align(ui_Alarm1Cont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Alarm1Cont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Alarm1Panel = lv_obj_create(ui_Alarm1Cont);
  lv_obj_set_width(ui_Alarm1Panel, 737);
  lv_obj_set_height(ui_Alarm1Panel, 50);
  lv_obj_set_align(ui_Alarm1Panel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Alarm1Panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_Alarm1Panel, lv_color_hex(0xCD3C3C),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Alarm1Panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Alarm1Label = lv_label_create(ui_Alarm1Cont);
  lv_obj_set_width(ui_Alarm1Label, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Alarm1Label, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Alarm1Label, 1);
  lv_obj_set_y(ui_Alarm1Label, 0);
  lv_obj_set_align(ui_Alarm1Label, LV_ALIGN_CENTER);
  lv_obj_set_style_text_font(ui_Alarm1Label, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Alarm2Cont = lv_obj_create(ui_TabPage1);
  lv_obj_remove_style_all(ui_Alarm2Cont);
  lv_obj_set_width(ui_Alarm2Cont, 712);
  lv_obj_set_height(ui_Alarm2Cont, 50);
  lv_obj_set_x(ui_Alarm2Cont, 0);
  lv_obj_set_y(ui_Alarm2Cont, -59);
  lv_obj_set_align(ui_Alarm2Cont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Alarm2Cont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Alarm2Panel = lv_obj_create(ui_Alarm2Cont);
  lv_obj_set_width(ui_Alarm2Panel, 737);
  lv_obj_set_height(ui_Alarm2Panel, 50);
  lv_obj_set_align(ui_Alarm2Panel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Alarm2Panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_Alarm2Panel, lv_color_hex(0xCD3C3C),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Alarm2Panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Alarm2Label = lv_label_create(ui_Alarm2Cont);
  lv_obj_set_width(ui_Alarm2Label, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Alarm2Label, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_Alarm2Label, LV_ALIGN_CENTER);
  lv_obj_set_style_text_font(ui_Alarm2Label, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Alarm3Cont = lv_obj_create(ui_TabPage1);
  lv_obj_remove_style_all(ui_Alarm3Cont);
  lv_obj_set_width(ui_Alarm3Cont, 712);
  lv_obj_set_height(ui_Alarm3Cont, 50);
  lv_obj_set_align(ui_Alarm3Cont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Alarm3Cont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Alarm3Panel = lv_obj_create(ui_Alarm3Cont);
  lv_obj_set_width(ui_Alarm3Panel, 737);
  lv_obj_set_height(ui_Alarm3Panel, 50);
  lv_obj_set_align(ui_Alarm3Panel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Alarm3Panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_Alarm3Panel, lv_color_hex(0xCD3C3C),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Alarm3Panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Alarm3Label = lv_label_create(ui_Alarm3Cont);
  lv_obj_set_width(ui_Alarm3Label, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Alarm3Label, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_Alarm3Label, LV_ALIGN_CENTER);
  lv_obj_set_style_text_font(ui_Alarm3Label, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Alarm4Cont = lv_obj_create(ui_TabPage1);
  lv_obj_remove_style_all(ui_Alarm4Cont);
  lv_obj_set_width(ui_Alarm4Cont, 712);
  lv_obj_set_height(ui_Alarm4Cont, 50);
  lv_obj_set_x(ui_Alarm4Cont, 0);
  lv_obj_set_y(ui_Alarm4Cont, 59);
  lv_obj_set_align(ui_Alarm4Cont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Alarm4Cont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Alarm4Panel = lv_obj_create(ui_Alarm4Cont);
  lv_obj_set_width(ui_Alarm4Panel, 737);
  lv_obj_set_height(ui_Alarm4Panel, 50);
  lv_obj_set_align(ui_Alarm4Panel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Alarm4Panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_Alarm4Panel, lv_color_hex(0xCD3C3C),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Alarm4Panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Alarm4Label = lv_label_create(ui_Alarm4Cont);
  lv_obj_set_width(ui_Alarm4Label, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Alarm4Label, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_Alarm4Label, LV_ALIGN_CENTER);
  lv_obj_set_style_text_font(ui_Alarm4Label, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_TabPage2 = lv_tabview_add_tab(ui_AlarmsTabview, "");

  ui_AlarmDetailLabel = lv_label_create(ui_TabPage2);
  lv_obj_set_width(ui_AlarmDetailLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_AlarmDetailLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_AlarmDetailLabel, -277);
  lv_obj_set_y(ui_AlarmDetailLabel, -139);
  lv_obj_set_align(ui_AlarmDetailLabel, LV_ALIGN_CENTER);

  ui_MuteAlarm = lv_imgbtn_create(ui_ScreenAlarms);
  lv_imgbtn_set_src(ui_MuteAlarm, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_mute_icon_png, NULL);
  lv_obj_set_width(ui_MuteAlarm, 44);
  lv_obj_set_height(ui_MuteAlarm, 45);
  lv_obj_set_x(ui_MuteAlarm, 9);
  lv_obj_set_y(ui_MuteAlarm, 171);
  lv_obj_set_align(ui_MuteAlarm, LV_ALIGN_CENTER);

  lv_obj_add_event_cb(ui_ImgButton7, ui_event_ImgButton7, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_AlarmsTabview, ui_event_AlarmsTabview, LV_EVENT_ALL,
                      NULL);
  lv_obj_add_event_cb(ui_Alarm1Cont, ui_event_Alarm1Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm2Cont, ui_event_Alarm2Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm3Cont, ui_event_Alarm3Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm4Cont, ui_event_Alarm4Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm1Label, ui_event_Alarm1Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm1Panel, ui_event_Alarm1Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm2Label, ui_event_Alarm2Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm2Panel, ui_event_Alarm2Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm3Label, ui_event_Alarm3Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm3Panel, ui_event_Alarm3Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm4Label, ui_event_Alarm4Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Alarm4Panel, ui_event_Alarm4Cont, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_MuteAlarm, ui_event_MuteAlarm, LV_EVENT_ALL, NULL);

  lv_obj_set_ext_click_area(ui_ImgButton7, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_MuteAlarm, TOUCH_EXT_MEDIUM);
}

void ui_apply_sparkline_style(lv_obj_t *chart, lv_color_t color) {
  // 1. Fondo y Borde: Totalmente transparentes
  lv_obj_set_style_bg_opa(chart, 0, LV_PART_MAIN);
  // Panel background styling for medical look
  lv_obj_set_style_bg_color(chart, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(chart, 20, LV_PART_MAIN);
  lv_obj_set_style_radius(chart, 10,
                          LV_PART_MAIN); // Bordes redondeados del contenedor

  lv_obj_set_style_border_side(chart, LV_BORDER_SIDE_NONE, LV_PART_MAIN);
  lv_obj_set_style_pad_all(chart, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_left(chart, 30, LV_PART_MAIN);

  // 2. Línea Principal: Muy fina y con bordes redondeados
  lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
  lv_obj_set_style_line_color(chart, color, LV_PART_ITEMS);
  lv_obj_set_style_line_rounded(
      chart, true, LV_PART_ITEMS); // Bordes redondeados en la línea

  // 3. Relleno inferior: Gradiente suave (fade a transparente)
  lv_obj_set_style_bg_opa(chart, 40, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(chart, color, LV_PART_ITEMS);
  lv_obj_set_style_bg_grad_color(chart, lv_color_hex(0x000000), LV_PART_ITEMS);
  lv_obj_set_style_bg_grad_dir(chart, LV_GRAD_DIR_VER, LV_PART_ITEMS);
  lv_obj_set_style_bg_main_stop(chart, 0, LV_PART_ITEMS);
  lv_obj_set_style_bg_grad_stop(chart, 200,
                                LV_PART_ITEMS); // Fade out mas rápido

  // 4. Grid: Casi invisible
  lv_obj_set_style_line_color(chart, lv_color_hex(0x404040), LV_PART_MAIN);
  lv_obj_set_style_line_opa(
      chart, 15, LV_PART_MAIN); // Reducido de 30 a 15 para minimalismo
  lv_chart_set_div_line_count(chart, 0,
                              0); // Quitar líneas divisorias explicitas si se
                                  // quiere minimalismo extremo
  // O dejar unas muy tenues
  lv_chart_set_div_line_count(chart, 4, 0);

  // 5. Configuración de Ejes
  lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR); // Sin puntos

  // Eje Y - Números muy discretos
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 2, 1, 4, 1, true, 25);
  lv_obj_set_style_text_color(chart, lv_color_hex(0x808080), LV_PART_TICKS);
  lv_obj_set_style_text_font(chart, &lv_font_montserrat_12, LV_PART_TICKS);

  // Glow sutil (Simulado con sombra de línea si fuera posible, pero usamos opa
  // y width) LVGL no tiene glow nativo para líneas de chart fácilmente, pero el
  // gradiente ayuda.
}

void ui_add_chart_safe_zone(lv_obj_t *chart, float min_val, float max_val,
                            float range_min, float range_max) {
  lv_obj_t *parent = lv_obj_get_parent(chart);
  lv_obj_t *safe_zone = lv_obj_create(parent);

  lv_obj_set_size(safe_zone, lv_obj_get_width(chart), 0);
  lv_obj_align_to(safe_zone, chart, LV_ALIGN_TOP_MID, 0, 0);

  float chart_h = lv_obj_get_height(chart);
  float total_range = range_max - range_min;

  if (total_range > 0) {
    float h = (max_val - min_val) * (chart_h / total_range);
    float y = (range_max - max_val) * (chart_h / total_range);
    lv_obj_set_height(safe_zone, (int)h);
    lv_obj_set_y(safe_zone, (int)y);
  }

  lv_obj_set_style_bg_color(safe_zone, lv_color_hex(0x00FF00), 0);
  lv_obj_set_style_bg_opa(safe_zone, 20, 0);
  lv_obj_set_style_border_side(safe_zone, LV_BORDER_SIDE_NONE, 0);
  lv_obj_set_style_radius(safe_zone, 0, 0);
  lv_obj_move_background(safe_zone);
}

void ui_ScreenCharts_screen_init(void) {
  ui_ScreenCharts = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenCharts, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(ui_ScreenCharts, ScreenCharts_load_cb,
                      LV_EVENT_SCREEN_LOADED, NULL);

  // TabView Principal: Tiempo Real vs Historial
  ui_TabViewMainCharts = lv_tabview_create(ui_ScreenCharts, LV_DIR_TOP, 40);
  lv_obj_set_width(ui_TabViewMainCharts, 800);
  lv_obj_set_height(ui_TabViewMainCharts,
                    430); // Reducido para dejar espacio arriba
  lv_obj_set_align(ui_TabViewMainCharts, LV_ALIGN_BOTTOM_MID); // Alineado abajo
  lv_obj_clear_flag(ui_TabViewMainCharts, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *ui_TabRealTime =
      lv_tabview_add_tab(ui_TabViewMainCharts, "REAL TIME");
  lv_obj_t *ui_TabHistory = lv_tabview_add_tab(ui_TabViewMainCharts, "HISTORY");

  // --- SECCIÓN TIEMPO REAL ---
  ui_TabView1 = lv_tabview_create(ui_TabRealTime, LV_DIR_TOP, 30);
  lv_obj_set_width(ui_TabView1, 769);
  lv_obj_set_height(ui_TabView1, 403);
  lv_obj_set_x(ui_TabView1, 0);
  lv_obj_set_y(ui_TabView1, 28);
  lv_obj_set_align(ui_TabView1, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_TabView1, LV_OBJ_FLAG_SCROLLABLE);

  ui_TempChartPage1 = lv_tabview_add_tab(ui_TabView1, "");

  ui_AirTempChartCont = lv_obj_create(ui_TempChartPage1);
  lv_obj_remove_style_all(ui_AirTempChartCont);
  lv_obj_set_width(ui_AirTempChartCont, 771);
  lv_obj_set_height(ui_AirTempChartCont, 389);
  lv_obj_set_align(ui_AirTempChartCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_AirTempChartCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_AirTempChart = lv_chart_create(ui_AirTempChartCont);
  lv_obj_set_width(ui_AirTempChart, 700);
  lv_obj_set_height(ui_AirTempChart, 280);
  lv_obj_set_align(ui_AirTempChart, LV_ALIGN_CENTER);
  lv_chart_set_type(ui_AirTempChart, LV_CHART_TYPE_LINE);
  lv_chart_set_range(ui_AirTempChart, LV_CHART_AXIS_PRIMARY_Y, TEMP_CHART_MIN,
                     TEMP_CHART_MAX);
  ui_apply_sparkline_style(ui_AirTempChart,
                           lv_color_hex(0x00FF00)); // Verde para incubadora
  ui_add_chart_safe_zone(ui_AirTempChart, AIR_SAFE_ZONE_MIN, AIR_SAFE_ZONE_MAX,
                         TEMP_BAR_DISPLAY_MIN,
                         TEMP_BAR_DISPLAY_MAX); // Banda 34-37C

  lv_chart_series_t *ui_AirTempChart_series_1 = lv_chart_add_series(
      ui_AirTempChart, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y);
  static lv_coord_t ui_AirTempChart_series_1_array[] = {0,  10, 20, 40, 80,
                                                        80, 40, 20, 10, 0};
  lv_chart_set_ext_y_array(ui_AirTempChart, ui_AirTempChart_series_1,
                           ui_AirTempChart_series_1_array);

  ui_Label37 = lv_label_create(ui_AirTempChartCont);
  lv_obj_set_width(ui_Label37, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label37, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label37, -2);
  lv_obj_set_y(ui_Label37, -165);
  lv_obj_set_align(ui_Label37, LV_ALIGN_CENTER);
  lv_obj_set_style_text_font(ui_Label37, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_SkinTempChartCont = lv_obj_create(ui_TempChartPage1);
  lv_obj_remove_style_all(ui_SkinTempChartCont);
  lv_obj_set_width(ui_SkinTempChartCont, 771);
  lv_obj_set_height(ui_SkinTempChartCont, 389);
  lv_obj_set_align(ui_SkinTempChartCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_SkinTempChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_SkinTempChartCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_SkinTempChart = lv_chart_create(ui_SkinTempChartCont);
  lv_obj_set_width(ui_SkinTempChart, 700);
  lv_obj_set_height(ui_SkinTempChart, 280);
  lv_obj_set_align(ui_SkinTempChart, LV_ALIGN_CENTER);
  lv_chart_set_type(ui_SkinTempChart, LV_CHART_TYPE_LINE);
  lv_chart_set_range(ui_SkinTempChart, LV_CHART_AXIS_PRIMARY_Y, TEMP_CHART_MIN,
                     TEMP_CHART_MAX);
  ui_apply_sparkline_style(ui_SkinTempChart,
                           lv_color_hex(0x00E0E0)); // Cyan para bebé
  ui_add_chart_safe_zone(ui_SkinTempChart, SKIN_SAFE_ZONE_MIN,
                         SKIN_SAFE_ZONE_MAX, TEMP_BAR_DISPLAY_MIN,
                         TEMP_BAR_DISPLAY_MAX); // Banda 36-37.5C

  lv_chart_series_t *ui_SkinTempChart_series_1 = lv_chart_add_series(
      ui_SkinTempChart, lv_color_hex(0x00E0E0), LV_CHART_AXIS_PRIMARY_Y);
  static lv_coord_t ui_SkinTempChart_series_1_array[] = {0,  10, 20, 40, 80,
                                                         80, 40, 20, 10, 0};
  lv_chart_set_ext_y_array(ui_SkinTempChart, ui_SkinTempChart_series_1,
                           ui_SkinTempChart_series_1_array);

  ui_Label38 = lv_label_create(ui_SkinTempChartCont);
  lv_obj_set_width(ui_Label38, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label38, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label38, -2);
  lv_obj_set_y(ui_Label38, -165);
  lv_obj_set_align(ui_Label38, LV_ALIGN_CENTER);
  lv_obj_set_style_text_font(ui_Label38, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_HumChartPage2 = lv_tabview_add_tab(ui_TabView1, "");

  ui_HumChartCont = lv_obj_create(ui_HumChartPage2);
  lv_obj_remove_style_all(ui_HumChartCont);
  lv_obj_set_width(ui_HumChartCont, 776);
  lv_obj_set_height(ui_HumChartCont, 400);
  lv_obj_set_x(ui_HumChartCont, 1);
  lv_obj_set_y(ui_HumChartCont, 0);
  lv_obj_set_align(ui_HumChartCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_HumChartCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_HumChart = lv_chart_create(ui_HumChartCont);
  lv_obj_set_width(ui_HumChart, 700);
  lv_obj_set_height(ui_HumChart, 280);
  lv_obj_set_align(ui_HumChart, LV_ALIGN_CENTER);
  lv_chart_set_type(ui_HumChart, LV_CHART_TYPE_LINE);
  lv_chart_set_range(ui_HumChart, LV_CHART_AXIS_PRIMARY_Y, HUM_CHART_MIN,
                     HUM_CHART_MAX);
  ui_apply_sparkline_style(ui_HumChart,
                           lv_color_hex(0x3B82F6)); // Azul para humedad
  ui_add_chart_safe_zone(ui_HumChart, HUM_SAFE_ZONE_MIN, HUM_SAFE_ZONE_MAX,
                         (double)HUM_CHART_MIN,
                         (double)HUM_CHART_MAX); // Banda 40-70%

  lv_chart_series_t *ui_HumChart_series_1 = lv_chart_add_series(
      ui_HumChart, lv_color_hex(0x3B82F6), LV_CHART_AXIS_PRIMARY_Y);
  static lv_coord_t ui_HumChart_series_1_array[] = {0,  10, 20, 40, 80,
                                                    80, 40, 20, 10, 0};
  lv_chart_set_ext_y_array(ui_HumChart, ui_HumChart_series_1,
                           ui_HumChart_series_1_array);

  ui_Label36 = lv_label_create(ui_HumChartCont);
  lv_obj_set_width(ui_Label36, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label36, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label36, -2);
  lv_obj_set_y(ui_Label36, -165);
  lv_obj_set_align(ui_Label36, LV_ALIGN_CENTER);
  lv_obj_set_style_text_font(ui_Label36, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // --- SECCIÓN HISTORIAL ---
  ui_HistoryDropdown = lv_dropdown_create(ui_TabHistory);
  lv_dropdown_set_options(ui_HistoryDropdown, "5 min\n30 min\n1 h\n2 h");
  lv_obj_set_width(ui_HistoryDropdown, 120);
  lv_obj_set_align(ui_HistoryDropdown, LV_ALIGN_TOP_RIGHT);
  lv_obj_set_x(ui_HistoryDropdown, -10);
  lv_obj_set_y(ui_HistoryDropdown, -12);
  lv_obj_add_event_cb(ui_HistoryDropdown, HistoryDropdown_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  ui_HistoryTimeLabel = lv_label_create(ui_TabHistory);
  lv_label_set_text(ui_HistoryTimeLabel, "RANGE:");
  lv_obj_set_align(ui_HistoryTimeLabel, LV_ALIGN_TOP_RIGHT);
  lv_obj_set_x(ui_HistoryTimeLabel, -140);
  lv_obj_set_y(ui_HistoryTimeLabel, 0);

  // Gráfica Historial Aire (Slot Superior)
  ui_HistoryChartAireLabel = lv_label_create(ui_TabHistory);
  lv_label_set_text(ui_HistoryChartAireLabel, "AIR TEMP");
  lv_obj_set_style_text_font(ui_HistoryChartAireLabel, &lv_font_montserrat_18,
                             0);
  lv_obj_set_pos(ui_HistoryChartAireLabel, 40, 10);

  ui_HistoryValueAire = lv_label_create(ui_TabHistory);
  lv_label_set_text(ui_HistoryValueAire, "---.-°C");
  lv_obj_set_style_text_font(ui_HistoryValueAire, &lv_font_montserrat_20, 0);
  lv_obj_set_pos(ui_HistoryValueAire, 200, 8);

  ui_HistoryChartAire = lv_chart_create(ui_TabHistory);
  lv_obj_set_size(ui_HistoryChartAire, 720, 140);
  lv_obj_set_pos(ui_HistoryChartAire, 10, 35);
  lv_chart_set_type(ui_HistoryChartAire, LV_CHART_TYPE_LINE);
  lv_chart_set_range(ui_HistoryChartAire, LV_CHART_AXIS_PRIMARY_Y,
                     TEMP_CHART_MIN, TEMP_CHART_MAX);
  ui_apply_sparkline_style(ui_HistoryChartAire, lv_color_hex(0x00FF00));
  ui_add_chart_safe_zone(ui_HistoryChartAire, AIR_SAFE_ZONE_MIN,
                         AIR_SAFE_ZONE_MAX, TEMP_BAR_DISPLAY_MIN,
                         TEMP_BAR_DISPLAY_MAX);
  historySeriesAire = lv_chart_add_series(
      ui_HistoryChartAire, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y);

  // Gráfica Historial Piel (Slot Superior - Superpuesta o oculta según modo)
  ui_HistoryChartSkinLabel = lv_label_create(ui_TabHistory);
  lv_label_set_text(ui_HistoryChartSkinLabel, "SKIN TEMP");
  lv_obj_set_style_text_font(ui_HistoryChartSkinLabel, &lv_font_montserrat_18,
                             0);
  lv_obj_set_pos(ui_HistoryChartSkinLabel, 40, 10);
  lv_obj_add_flag(ui_HistoryChartSkinLabel, LV_OBJ_FLAG_HIDDEN);

  ui_HistoryValueSkin = lv_label_create(ui_TabHistory);
  lv_label_set_text(ui_HistoryValueSkin, "---.-°C");
  lv_obj_set_style_text_font(ui_HistoryValueSkin, &lv_font_montserrat_20, 0);
  lv_obj_set_pos(ui_HistoryValueSkin, 200, 8);
  lv_obj_add_flag(ui_HistoryValueSkin, LV_OBJ_FLAG_HIDDEN);

  ui_HistoryChartSkin = lv_chart_create(ui_TabHistory);
  lv_obj_set_size(ui_HistoryChartSkin, 720, 140);
  lv_obj_set_pos(ui_HistoryChartSkin, 10, 35);
  lv_chart_set_type(ui_HistoryChartSkin, LV_CHART_TYPE_LINE);
  lv_chart_set_range(ui_HistoryChartSkin, LV_CHART_AXIS_PRIMARY_Y,
                     TEMP_CHART_MIN, TEMP_CHART_MAX);
  ui_apply_sparkline_style(ui_HistoryChartSkin, lv_color_hex(0x00E0E0));
  ui_add_chart_safe_zone(ui_HistoryChartSkin, SKIN_SAFE_ZONE_MIN,
                         SKIN_SAFE_ZONE_MAX, TEMP_BAR_DISPLAY_MIN,
                         TEMP_BAR_DISPLAY_MAX);
  historySeriesSkin = lv_chart_add_series(
      ui_HistoryChartSkin, lv_color_hex(0x00E0E0), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_add_flag(ui_HistoryChartSkin, LV_OBJ_FLAG_HIDDEN);

  // Gráfica Historial Humedad (Slot Inferior)
  ui_HistoryChartHumLabel = lv_label_create(ui_TabHistory);
  lv_label_set_text(ui_HistoryChartHumLabel, "HUMIDITY");
  lv_obj_set_style_text_font(ui_HistoryChartHumLabel, &lv_font_montserrat_18,
                             0);
  lv_obj_set_pos(ui_HistoryChartHumLabel, 40, 190);

  ui_HistoryValueHum = lv_label_create(ui_TabHistory);
  lv_label_set_text(ui_HistoryValueHum, "--%");
  lv_obj_set_style_text_font(ui_HistoryValueHum, &lv_font_montserrat_20, 0);
  lv_obj_set_pos(ui_HistoryValueHum, 200, 188);

  ui_HistoryChartHum = lv_chart_create(ui_TabHistory);
  lv_obj_set_size(ui_HistoryChartHum, 720, 140);
  lv_obj_set_pos(ui_HistoryChartHum, 10, 215);
  lv_chart_set_type(ui_HistoryChartHum, LV_CHART_TYPE_LINE);
  lv_chart_set_range(ui_HistoryChartHum, LV_CHART_AXIS_PRIMARY_Y, HUM_CHART_MIN,
                     HUM_CHART_MAX);
  ui_apply_sparkline_style(ui_HistoryChartHum, lv_color_hex(0x3B82F6));
  ui_add_chart_safe_zone(ui_HistoryChartHum, HUM_SAFE_ZONE_MIN,
                         HUM_SAFE_ZONE_MAX, (double)HUM_CHART_MIN,
                         (double)HUM_CHART_MAX);
  historySeriesHum = lv_chart_add_series(
      ui_HistoryChartHum, lv_color_hex(0x3B82F6), LV_CHART_AXIS_PRIMARY_Y);

  ui_OxChartCont = lv_obj_create(ui_ScreenCharts);
  lv_obj_remove_style_all(ui_OxChartCont);
  lv_obj_set_width(ui_OxChartCont, 775);
  lv_obj_set_height(ui_OxChartCont, 468);
  lv_obj_set_align(ui_OxChartCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_OxChartCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_OxChartCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_OxChart = lv_chart_create(ui_OxChartCont);
  lv_obj_set_width(ui_OxChart, 637);
  lv_obj_set_height(ui_OxChart, 336);
  lv_obj_set_x(ui_OxChart, 0);
  lv_obj_set_y(ui_OxChart, 10);
  lv_obj_set_align(ui_OxChart, LV_ALIGN_CENTER);
  lv_chart_set_type(ui_OxChart, LV_CHART_TYPE_LINE);
  lv_chart_set_div_line_count(ui_OxChart, 10, 10); // Cuadrícula
  lv_chart_set_axis_tick(ui_OxChart, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 10, 1,
                         true, 40); // Etiquetas eje Y
  lv_obj_set_style_pad_left(ui_OxChart, 50,
                            LV_PART_MAIN); // Margen para números
  lv_chart_set_range(ui_OxChart, LV_CHART_AXIS_PRIMARY_Y, HUM_BAR_MIN,
                     HUM_BAR_MAX);
  lv_chart_series_t *ui_OxChart_series_1 = lv_chart_add_series(
      ui_OxChart, lv_color_hex(0x808080), LV_CHART_AXIS_PRIMARY_Y);
  static lv_coord_t ui_OxChart_series_1_array[] = {0,  10, 20, 40, 80,
                                                   80, 40, 20, 10, 0};
  lv_chart_set_ext_y_array(ui_OxChart, ui_OxChart_series_1,
                           ui_OxChart_series_1_array);

  ui_Label35 = lv_label_create(ui_OxChartCont);
  lv_obj_set_width(ui_Label35, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label35, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label35, -2);
  lv_obj_set_y(ui_Label35, -202);
  lv_obj_set_align(ui_Label35, LV_ALIGN_CENTER);
  lv_obj_set_style_text_font(ui_Label35, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ImgButton8 = lv_imgbtn_create(ui_ScreenCharts);
  lv_imgbtn_set_src(ui_ImgButton8, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_1508956403, NULL);
  lv_obj_set_width(ui_ImgButton8, 50);
  lv_obj_set_height(ui_ImgButton8, 52);
  lv_obj_set_x(ui_ImgButton8, 10);
  lv_obj_set_y(ui_ImgButton8, 5);
  lv_obj_set_align(ui_ImgButton8, LV_ALIGN_TOP_LEFT);

  lv_obj_add_event_cb(ui_ImgButton8, ui_event_ImgButton8, LV_EVENT_ALL, NULL);

  lv_obj_set_ext_click_area(ui_ImgButton8, TOUCH_EXT_MEDIUM);
}

void ui_ScreenPulseOxi_screen_init(void) {
  ui_ScreenPulseOxi = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenPulseOxi, LV_OBJ_FLAG_SCROLLABLE);

  ui_ImgButton9 = lv_imgbtn_create(ui_ScreenPulseOxi);
  lv_imgbtn_set_src(ui_ImgButton9, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_1508956403, NULL);
  lv_obj_set_width(ui_ImgButton9, 50);
  lv_obj_set_height(ui_ImgButton9, 52);
  lv_obj_set_x(ui_ImgButton9, -360);
  lv_obj_set_y(ui_ImgButton9, -204);
  lv_obj_set_align(ui_ImgButton9, LV_ALIGN_CENTER);

  ui_OxCont = lv_obj_create(ui_ScreenPulseOxi);
  lv_obj_remove_style_all(ui_OxCont);
  lv_obj_set_width(ui_OxCont, 785);
  lv_obj_set_height(ui_OxCont, 443);
  lv_obj_set_x(ui_OxCont, -6);
  lv_obj_set_y(ui_OxCont, 54);
  lv_obj_set_align(ui_OxCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_OxCont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Panel15 = lv_obj_create(ui_OxCont);
  lv_obj_set_width(ui_Panel15, 336);
  lv_obj_set_height(ui_Panel15, 129);
  lv_obj_set_x(ui_Panel15, -198);
  lv_obj_set_y(ui_Panel15, -140);
  lv_obj_set_align(ui_Panel15, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel15, LV_OBJ_FLAG_SCROLLABLE);

  ui_Label39 = lv_label_create(ui_OxCont);
  lv_obj_set_width(ui_Label39, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label39, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label39, -296);
  lv_obj_set_y(ui_Label39, -183);
  lv_obj_set_align(ui_Label39, LV_ALIGN_CENTER);

  ui_Label5 = lv_label_create(ui_OxCont);
  lv_obj_set_width(ui_Label5, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label5, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label5, -316);
  lv_obj_set_y(ui_Label5, -128);
  lv_obj_set_align(ui_Label5, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label5, "25.1");
  lv_obj_set_style_text_font(ui_Label5, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_DetectOxi = lv_label_create(ui_OxCont);
  lv_obj_set_width(ui_DetectOxi, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_DetectOxi, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_DetectOxi, -202);
  lv_obj_set_y(ui_DetectOxi, -129);
  lv_obj_set_align(ui_DetectOxi, LV_ALIGN_CENTER);
  lv_label_set_text(ui_DetectOxi, "25.1");
  lv_obj_set_style_text_font(ui_DetectOxi, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_OxiButton2 = lv_btn_create(ui_OxCont);
  lv_obj_set_width(ui_OxiButton2, 310);
  lv_obj_set_height(ui_OxiButton2, 30);
  lv_obj_set_x(ui_OxiButton2, -206);
  lv_obj_set_y(ui_OxiButton2, -183);
  lv_obj_set_align(ui_OxiButton2, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_OxiButton2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(ui_OxiButton2, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_opa(ui_OxiButton2, 0,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_opa(ui_OxiButton2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_add_event_cb(ui_ImgButton9, ui_event_ImgButton9, LV_EVENT_ALL, NULL);

  lv_obj_set_ext_click_area(ui_ImgButton9, TOUCH_EXT_MEDIUM);
}

#define LVGL_INIT_GUARD_ROOT(ptr, name)                                      \
    do {                                                                     \
        if (!(ptr)) {                                                        \
            ESP_LOGE("UI_INIT", "LVGL OOM: " name " == NULL");              \
            return;                                                          \
        }                                                                    \
    } while (0)

#define LVGL_INIT_GUARD_CHILD(ptr, name)                                     \
    do {                                                                     \
        if (!(ptr)) {                                                        \
            ESP_LOGE("UI_INIT", "LVGL OOM: " name " == NULL");              \
            lv_obj_del(ui_ScreenSettings);                                   \
            ui_ScreenSettings = NULL;                                        \
            return;                                                          \
        }                                                                    \
    } while (0)

void ui_ScreenSettings_screen_init(void) {
  ui_ScreenSettings = lv_obj_create(NULL);
  LVGL_INIT_GUARD_ROOT(ui_ScreenSettings, "ui_ScreenSettings");
  lv_obj_clear_flag(ui_ScreenSettings, LV_OBJ_FLAG_SCROLLABLE);

  ui_Label8 = lv_label_create(ui_ScreenSettings);
  lv_obj_set_width(ui_Label8, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label8, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label8, -112);
  lv_obj_set_y(ui_Label8, -198);
  lv_obj_set_align(ui_Label8, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label8, "Settings");
  lv_obj_set_style_text_font(ui_Label8, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ImgButton2 = lv_imgbtn_create(ui_ScreenSettings);
  lv_imgbtn_set_src(ui_ImgButton2, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_1508956403, NULL);
  lv_obj_set_width(ui_ImgButton2, 50);
  lv_obj_set_height(ui_ImgButton2, 52);
  lv_obj_set_x(ui_ImgButton2, -360);
  lv_obj_set_y(ui_ImgButton2, -204);
  lv_obj_set_align(ui_ImgButton2, LV_ALIGN_CENTER);

  ui_Container3 = lv_obj_create(ui_ScreenSettings);
  lv_obj_remove_style_all(ui_Container3);
  lv_obj_set_width(ui_Container3, 331);
  lv_obj_set_height(ui_Container3, 420);
  lv_obj_set_x(ui_Container3, -200);
  lv_obj_set_y(ui_Container3, -10);
  lv_obj_set_align(ui_Container3, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Container3,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  // --- INFO CONTAINER ---
  ui_InfoCont = lv_obj_create(ui_Container3);
  lv_obj_remove_style_all(ui_InfoCont);
  lv_obj_set_width(ui_InfoCont, 331);
  lv_obj_set_height(ui_InfoCont, 45);
  lv_obj_set_x(ui_InfoCont, 0);
  lv_obj_set_y(ui_InfoCont, -100);
  lv_obj_set_align(ui_InfoCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_InfoCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_InfoPanel = lv_obj_create(ui_InfoCont);
  lv_obj_set_width(ui_InfoPanel, 331);
  lv_obj_set_height(ui_InfoPanel, 45);
  lv_obj_set_align(ui_InfoPanel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_InfoPanel, LV_OBJ_FLAG_SCROLLABLE);

  ui_InfoLabel = lv_label_create(ui_InfoCont);
  lv_obj_set_width(ui_InfoLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_InfoLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_InfoLabel, 20);
  lv_obj_set_y(ui_InfoLabel, 0);
  lv_obj_set_align(ui_InfoLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_InfoLabel, "Info");
  lv_obj_set_style_text_font(ui_InfoLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_InfoButton = lv_btn_create(ui_InfoCont);
  lv_obj_set_width(ui_InfoButton, 321);
  lv_obj_set_height(ui_InfoButton, 40);
  lv_obj_set_align(ui_InfoButton, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_InfoButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(ui_InfoButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_opa(ui_InfoButton, 0,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_opa(ui_InfoButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_InfoArrow = lv_label_create(ui_InfoCont);
  lv_obj_set_width(ui_InfoArrow, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_InfoArrow, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_InfoArrow, 114);
  lv_obj_set_y(ui_InfoArrow, 0);
  lv_obj_set_align(ui_InfoArrow, LV_ALIGN_CENTER);
  lv_label_set_text(ui_InfoArrow, ">");
  lv_obj_set_style_text_font(ui_InfoArrow, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // --- WIFI CONTAINER (Positioned below Info) ---
  ui_WifiCont = lv_obj_create(ui_Container3);
  lv_obj_remove_style_all(ui_WifiCont);
  lv_obj_set_width(ui_WifiCont, 331);
  lv_obj_set_height(ui_WifiCont, 45);
  lv_obj_set_x(ui_WifiCont, 0);
  lv_obj_set_y(ui_WifiCont, -45);
  lv_obj_set_align(ui_WifiCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_WifiCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Panel7 = lv_obj_create(ui_WifiCont);
  lv_obj_set_width(ui_Panel7, 331);
  lv_obj_set_height(ui_Panel7, 45);
  lv_obj_set_align(ui_Panel7, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel7, LV_OBJ_FLAG_SCROLLABLE);

  ui_WifiLabel = lv_label_create(ui_WifiCont);
  lv_obj_set_width(ui_WifiLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_WifiLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_WifiLabel, 20);
  lv_obj_set_y(ui_WifiLabel, 0);
  lv_obj_set_align(ui_WifiLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_WifiLabel, "WiFi");
  lv_obj_set_style_text_font(ui_WifiLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_WifiButton = lv_btn_create(ui_WifiCont);
  lv_obj_set_width(ui_WifiButton, 321);
  lv_obj_set_height(ui_WifiButton, 40);
  lv_obj_set_align(ui_WifiButton, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_WifiButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(ui_WifiButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_opa(ui_WifiButton, 0,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_opa(ui_WifiButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Label3 = lv_label_create(ui_WifiCont);
  lv_obj_set_width(ui_Label3, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label3, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label3, 114);
  lv_obj_set_y(ui_Label3, 0);
  lv_obj_set_align(ui_Label3, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label3, ">");
  lv_obj_set_style_text_font(ui_Label3, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // --- LANGUAGES CONTAINER (Positioned below WiFi) ---
  ui_LanguagesCont = lv_obj_create(ui_Container3);
  lv_obj_remove_style_all(ui_LanguagesCont);
  lv_obj_set_width(ui_LanguagesCont, 331);
  lv_obj_set_height(ui_LanguagesCont, 45);
  lv_obj_set_x(ui_LanguagesCont, 0);
  lv_obj_set_y(ui_LanguagesCont, 10);
  lv_obj_set_align(ui_LanguagesCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_LanguagesCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Panel8 = lv_obj_create(ui_LanguagesCont);
  lv_obj_set_width(ui_Panel8, 331);
  lv_obj_set_height(ui_Panel8, 45);
  lv_obj_set_align(ui_Panel8, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel8, LV_OBJ_FLAG_SCROLLABLE);

  ui_LanguagesLabel = lv_label_create(ui_LanguagesCont);
  lv_obj_set_width(ui_LanguagesLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LanguagesLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_LanguagesLabel, 20);
  lv_obj_set_y(ui_LanguagesLabel, 0);
  lv_obj_set_align(ui_LanguagesLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_LanguagesLabel, "Languages");
  lv_obj_set_style_text_font(ui_LanguagesLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LanguagesButton = lv_btn_create(ui_LanguagesCont);
  lv_obj_set_width(ui_LanguagesButton, 321);
  lv_obj_set_height(ui_LanguagesButton, 40);
  lv_obj_set_align(ui_LanguagesButton, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_LanguagesButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(ui_LanguagesButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_opa(ui_LanguagesButton, 0,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_opa(ui_LanguagesButton, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Label1 = lv_label_create(ui_LanguagesCont);
  lv_obj_set_width(ui_Label1, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label1, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label1, 114);
  lv_obj_set_y(ui_Label1, 0);
  lv_obj_set_align(ui_Label1, LV_ALIGN_CENTER);
  lv_label_set_text(ui_Label1, ">");
  lv_obj_set_style_text_font(ui_Label1, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // --- SKIN MODE CONTAINER (Positioned below Languages) ---
  ui_SkinModeCont = lv_obj_create(ui_Container3);
  lv_obj_remove_style_all(ui_SkinModeCont);
  lv_obj_set_width(ui_SkinModeCont, 331);
  lv_obj_set_height(ui_SkinModeCont, 45);
  lv_obj_set_x(ui_SkinModeCont, 0);
  lv_obj_set_y(ui_SkinModeCont, 65);
  lv_obj_set_align(ui_SkinModeCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_SkinModeCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Panel9 = lv_obj_create(ui_SkinModeCont);
  lv_obj_set_width(ui_Panel9, 331);
  lv_obj_set_height(ui_Panel9, 45);
  lv_obj_set_align(ui_Panel9, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel9, LV_OBJ_FLAG_SCROLLABLE);

  ui_SkinOptionLabel = lv_label_create(ui_SkinModeCont);
  lv_obj_set_width(ui_SkinOptionLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_SkinOptionLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_SkinOptionLabel, 20);
  lv_obj_set_y(ui_SkinOptionLabel, 0);
  lv_obj_set_align(ui_SkinOptionLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_SkinOptionLabel, "SKIN MODE");
  lv_obj_set_style_text_font(ui_SkinOptionLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Switch4 = lv_switch_create(ui_SkinModeCont);
  lv_obj_set_width(ui_Switch4, 90);
  lv_obj_set_height(ui_Switch4, 35);
  lv_obj_set_x(ui_Switch4, 111);
  lv_obj_set_y(ui_Switch4, 0);
  lv_obj_set_align(ui_Switch4, LV_ALIGN_CENTER);

  // --- DARK MODE CONTAINER (Positioned below Skin Mode) ---
  ui_DarkModeCont = lv_obj_create(ui_Container3);
  lv_obj_remove_style_all(ui_DarkModeCont);
  lv_obj_set_width(ui_DarkModeCont, 331);
  lv_obj_set_height(ui_DarkModeCont, 45);
  lv_obj_set_x(ui_DarkModeCont, 0);
  lv_obj_set_y(ui_DarkModeCont, 175);
  lv_obj_set_align(ui_DarkModeCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_DarkModeCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_PanelDarkMode = lv_obj_create(ui_DarkModeCont);
  lv_obj_set_width(ui_PanelDarkMode, 331);
  lv_obj_set_height(ui_PanelDarkMode, 45);
  lv_obj_set_align(ui_PanelDarkMode, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_PanelDarkMode, LV_OBJ_FLAG_SCROLLABLE);

  ui_DarkModeLabel = lv_label_create(ui_DarkModeCont);
  lv_obj_set_width(ui_DarkModeLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_DarkModeLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_DarkModeLabel, 20);
  lv_obj_set_y(ui_DarkModeLabel, 0);
  lv_obj_set_align(ui_DarkModeLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_DarkModeLabel, "DARK MODE");
  lv_obj_set_style_text_font(ui_DarkModeLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_SwitchDarkMode = lv_switch_create(ui_DarkModeCont);
  lv_obj_set_width(ui_SwitchDarkMode, 90);
  lv_obj_set_height(ui_SwitchDarkMode, 35);
  lv_obj_set_x(ui_SwitchDarkMode, 111);
  lv_obj_set_y(ui_SwitchDarkMode, 0);
  lv_obj_set_align(ui_SwitchDarkMode, LV_ALIGN_CENTER);

  // Humidity Mode Toggle (Settings)
  ui_HumidityModeCont = lv_obj_create(ui_Container3);
  lv_obj_remove_style_all(ui_HumidityModeCont);
  lv_obj_set_width(ui_HumidityModeCont, 331);
  lv_obj_set_height(ui_HumidityModeCont, 45);
  lv_obj_set_x(ui_HumidityModeCont, 0);
  lv_obj_set_y(ui_HumidityModeCont, 120);
  lv_obj_set_align(ui_HumidityModeCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_HumidityModeCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_PanelHumidityMode = lv_obj_create(ui_HumidityModeCont);
  lv_obj_set_width(ui_PanelHumidityMode, 331);
  lv_obj_set_height(ui_PanelHumidityMode, 45);
  lv_obj_set_align(ui_PanelHumidityMode, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_PanelHumidityMode, LV_OBJ_FLAG_SCROLLABLE);

  ui_HumidityModeLabel = lv_label_create(ui_HumidityModeCont);
  lv_obj_set_width(ui_HumidityModeLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HumidityModeLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_HumidityModeLabel, 20);
  lv_obj_set_y(ui_HumidityModeLabel, 0);
  lv_obj_set_align(ui_HumidityModeLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_HumidityModeLabel, "HUMIDITY CONTROL");
  lv_obj_set_style_text_font(ui_HumidityModeLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_SwitchHumidityMode = lv_switch_create(ui_HumidityModeCont);
  lv_obj_set_width(ui_SwitchHumidityMode, 90);
  lv_obj_set_height(ui_SwitchHumidityMode, 35);
  lv_obj_set_x(ui_SwitchHumidityMode, 111);
  lv_obj_set_y(ui_SwitchHumidityMode, 0);
  lv_obj_set_align(ui_SwitchHumidityMode, LV_ALIGN_CENTER);

  ui_WifiConfigCont = lv_obj_create(ui_ScreenSettings);
  LVGL_INIT_GUARD_CHILD(ui_WifiConfigCont, "ui_WifiConfigCont");
  lv_obj_remove_style_all(ui_WifiConfigCont);
  lv_obj_set_width(ui_WifiConfigCont, 770);
  lv_obj_set_height(ui_WifiConfigCont, 361);
  lv_obj_set_x(ui_WifiConfigCont, 0);
  lv_obj_set_y(ui_WifiConfigCont, 20);
  lv_obj_set_align(ui_WifiConfigCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_WifiConfigCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_WifiConfigCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Keyboard1 = lv_keyboard_create(ui_WifiConfigCont);
  lv_obj_set_width(ui_Keyboard1, 750);
  lv_obj_set_height(ui_Keyboard1, 185);
  lv_obj_set_x(ui_Keyboard1, 0);
  lv_obj_set_y(ui_Keyboard1, 100);
  lv_obj_set_align(ui_Keyboard1, LV_ALIGN_CENTER);

  ui_SSIDPanel = lv_obj_create(ui_WifiConfigCont);
  lv_obj_set_width(ui_SSIDPanel, 400);
  lv_obj_set_height(ui_SSIDPanel, 55);
  lv_obj_set_x(ui_SSIDPanel, 185);
  lv_obj_set_y(ui_SSIDPanel, -75);
  lv_obj_set_align(ui_SSIDPanel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_SSIDPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_SSIDPanel, lv_color_hex(0xFFFFFF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_SSIDPanel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(ui_SSIDPanel, lv_color_hex(0xDDDDDD),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui_SSIDPanel, 1,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_SSIDLabel = lv_label_create(ui_SSIDPanel);
  lv_obj_set_width(ui_SSIDLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_SSIDLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_SSIDLabel, -10);
  lv_obj_set_y(ui_SSIDLabel, 0);
  lv_obj_set_align(ui_SSIDLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_SSIDLabel, "SSID");
  lv_obj_set_style_text_color(ui_SSIDLabel, lv_color_hex(0x333333),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_PassPanel = lv_obj_create(ui_WifiConfigCont);
  lv_obj_set_width(ui_PassPanel, 400);
  lv_obj_set_height(ui_PassPanel, 55);
  lv_obj_set_x(ui_PassPanel, 185);
  lv_obj_set_y(ui_PassPanel, -15);
  lv_obj_set_align(ui_PassPanel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_PassPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_PassPanel, lv_color_hex(0xFFFFFF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_PassPanel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(ui_PassPanel, lv_color_hex(0xDDDDDD),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui_PassPanel, 1,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_PassLabel = lv_label_create(ui_PassPanel);
  lv_obj_set_width(ui_PassLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_PassLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_PassLabel, -10);
  lv_obj_set_y(ui_PassLabel, 0);
  lv_obj_set_align(ui_PassLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_PassLabel, "PASSWORD");
  lv_obj_set_style_text_color(ui_PassLabel, lv_color_hex(0x333333),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_TextArea1 = lv_textarea_create(ui_SSIDPanel);
  lv_obj_set_width(ui_TextArea1, 260);
  lv_obj_set_height(ui_TextArea1, 20);
  lv_obj_set_x(ui_TextArea1, 10);
  lv_obj_set_y(ui_TextArea1, 0);
  lv_obj_set_align(ui_TextArea1, LV_ALIGN_RIGHT_MID);
  lv_textarea_set_placeholder_text(ui_TextArea1, "Placeholder...");
  lv_textarea_set_one_line(ui_TextArea1, true);

  ui_TextArea2 = lv_textarea_create(ui_PassPanel);
  lv_obj_set_width(ui_TextArea2, 260);
  lv_obj_set_height(ui_TextArea2, 20);
  lv_obj_set_x(ui_TextArea2, 10);
  lv_obj_set_y(ui_TextArea2, 0);
  lv_obj_set_align(ui_TextArea2, LV_ALIGN_RIGHT_MID);
  lv_textarea_set_placeholder_text(ui_TextArea2, "Placeholder...");
  lv_textarea_set_one_line(ui_TextArea2, true);
  lv_textarea_set_password_mode(ui_TextArea2, true);

  ui_WifiConnectButton = lv_btn_create(ui_WifiConfigCont);
  LVGL_INIT_GUARD_CHILD(ui_WifiConnectButton, "ui_WifiConnectButton");
  lv_obj_set_width(ui_WifiConnectButton, 130);
  lv_obj_set_height(ui_WifiConnectButton, 45);
  lv_obj_set_x(ui_WifiConnectButton, 320);
  lv_obj_set_y(ui_WifiConnectButton, 45);
  lv_obj_set_align(ui_WifiConnectButton, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_WifiConnectButton, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(ui_WifiConnectButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_WifiConnectButton, lv_color_hex(0x2196F3),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_WifiConnectButton, 10,
                          LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ConnectLabel = lv_label_create(ui_WifiConnectButton);
  lv_obj_set_width(ui_ConnectLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ConnectLabel, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_ConnectLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_ConnectLabel, "Connect");
  lv_obj_set_style_text_color(ui_ConnectLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_WifiDisconnectButton = lv_btn_create(ui_WifiConfigCont);
  LVGL_INIT_GUARD_CHILD(ui_WifiDisconnectButton, "ui_WifiDisconnectButton");
  lv_obj_set_width(ui_WifiDisconnectButton, 130);
  lv_obj_set_height(ui_WifiDisconnectButton, 45);
  lv_obj_set_x(ui_WifiDisconnectButton, 320);
  lv_obj_set_y(ui_WifiDisconnectButton, 45);
  lv_obj_set_align(ui_WifiDisconnectButton, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_WifiDisconnectButton,
                  LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_WifiDisconnectButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_WifiDisconnectButton, lv_color_hex(0xF44336),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_WifiDisconnectButton, 10,
                          LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_DisconnectLabel = lv_label_create(ui_WifiDisconnectButton);
  lv_obj_set_width(ui_DisconnectLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_DisconnectLabel, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_DisconnectLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_DisconnectLabel, "Disconnect");
  lv_obj_set_style_text_color(ui_DisconnectLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_InfoDetailsCont = lv_obj_create(ui_ScreenSettings);
  lv_obj_remove_style_all(ui_InfoDetailsCont);
  lv_obj_set_width(ui_InfoDetailsCont, 411);
  lv_obj_set_height(ui_InfoDetailsCont, 190);
  lv_obj_set_x(ui_InfoDetailsCont, 185);
  lv_obj_set_y(ui_InfoDetailsCont, -50);
  lv_obj_set_align(ui_InfoDetailsCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_InfoDetailsCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_InfoDetailsCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_InfoDetailsPanel = lv_obj_create(ui_InfoDetailsCont);
  lv_obj_set_width(ui_InfoDetailsPanel, 411);
  lv_obj_set_height(ui_InfoDetailsPanel, 190);
  lv_obj_set_align(ui_InfoDetailsPanel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_InfoDetailsPanel, LV_OBJ_FLAG_SCROLLABLE);

  ui_HMIVerTitle = lv_label_create(ui_InfoDetailsCont);
  lv_obj_set_width(ui_HMIVerTitle, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HMIVerTitle, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_HMIVerTitle, -100);
  lv_obj_set_y(ui_HMIVerTitle, -60);
  lv_obj_set_align(ui_HMIVerTitle, LV_ALIGN_CENTER);
  lv_label_set_text(ui_HMIVerTitle, "DISPLAY VER:");
  lv_obj_set_style_text_font(ui_HMIVerTitle, &lv_font_montserrat_14,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_HMIVerValue = lv_label_create(ui_InfoDetailsCont);
  lv_obj_set_width(ui_HMIVerValue, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_HMIVerValue, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_HMIVerValue, 50);
  lv_obj_set_y(ui_HMIVerValue, -60);
  lv_obj_set_align(ui_HMIVerValue, LV_ALIGN_CENTER);
  lv_label_set_text(ui_HMIVerValue, "1.0.0");
  lv_obj_set_style_text_color(ui_HMIVerValue, lv_color_hex(0x2196F3),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_HMIVerValue, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_MBVerTitle = lv_label_create(ui_InfoDetailsCont);
  lv_obj_set_width(ui_MBVerTitle, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_MBVerTitle, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_MBVerTitle, -100);
  lv_obj_set_y(ui_MBVerTitle, -20);
  lv_obj_set_align(ui_MBVerTitle, LV_ALIGN_CENTER);
  lv_label_set_text(ui_MBVerTitle, "MOTHERBOARD VER:");
  lv_obj_set_style_text_font(ui_MBVerTitle, &lv_font_montserrat_14,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_MBVerValue = lv_label_create(ui_InfoDetailsCont);
  lv_obj_set_width(ui_MBVerValue, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_MBVerValue, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_MBVerValue, 50);
  lv_obj_set_y(ui_MBVerValue, -20);
  lv_obj_set_align(ui_MBVerValue, LV_ALIGN_CENTER);
  lv_label_set_text(ui_MBVerValue, "0.0.0");
  lv_obj_set_style_text_color(ui_MBVerValue, lv_color_hex(0x2196F3),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_MBVerValue, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_SNTitle = lv_label_create(ui_InfoDetailsCont);
  lv_obj_set_width(ui_SNTitle, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_SNTitle, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_SNTitle, -100);
  lv_obj_set_y(ui_SNTitle, 20);
  lv_obj_set_align(ui_SNTitle, LV_ALIGN_CENTER);
  lv_label_set_text(ui_SNTitle, "S/N:");
  lv_obj_set_style_text_font(ui_SNTitle, &lv_font_montserrat_14,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_SNValue = lv_label_create(ui_InfoDetailsCont);
  lv_obj_set_width(ui_SNValue, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_SNValue, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_SNValue, 50);
  lv_obj_set_y(ui_SNValue, 20);
  lv_obj_set_align(ui_SNValue, LV_ALIGN_CENTER);
  lv_label_set_text(ui_SNValue, "0000");
  lv_obj_set_style_text_color(ui_SNValue, lv_color_hex(0x2196F3),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_SNValue, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ConnTitle = lv_label_create(ui_InfoDetailsCont);
  lv_obj_set_width(ui_ConnTitle, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ConnTitle, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ConnTitle, -100);
  lv_obj_set_y(ui_ConnTitle, 60);
  lv_obj_set_align(ui_ConnTitle, LV_ALIGN_CENTER);
  lv_label_set_text(ui_ConnTitle, "CONNECTIVITY:");
  lv_obj_set_style_text_font(ui_ConnTitle, &lv_font_montserrat_14,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ConnValue = lv_label_create(ui_InfoDetailsCont);
  lv_obj_set_width(ui_ConnValue, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ConnValue, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ConnValue, 50);
  lv_obj_set_y(ui_ConnValue, 60);
  lv_obj_set_align(ui_ConnValue, LV_ALIGN_CENTER);
  lv_label_set_text(ui_ConnValue, "-");
  lv_obj_set_style_text_color(ui_ConnValue, lv_color_hex(0x2196F3),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_ConnValue, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LanguagesDropDown = lv_dropdown_create(ui_ScreenSettings);
  lv_dropdown_set_options(ui_LanguagesDropDown,
                          "English\nEspañol\nPortuguês\nItaliano\nDeutsch\nРусс"
                          "кий\nTürkçe\nاردو\nMelayu\n中文");
  lv_obj_set_width(ui_LanguagesDropDown, 221);
  lv_obj_set_height(ui_LanguagesDropDown, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_LanguagesDropDown, 110);
  lv_obj_set_y(ui_LanguagesDropDown, -55);
  lv_obj_set_align(ui_LanguagesDropDown, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_LanguagesDropDown,
                  LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_SCROLL_ON_FOCUS);

  ui_WifiConnectedCont = lv_obj_create(ui_ScreenSettings);
  lv_obj_remove_style_all(ui_WifiConnectedCont);
  lv_obj_set_width(ui_WifiConnectedCont, 411);
  lv_obj_set_height(ui_WifiConnectedCont, 45);
  lv_obj_set_x(ui_WifiConnectedCont, 185);
  lv_obj_set_y(ui_WifiConnectedCont, -110);
  lv_obj_set_align(ui_WifiConnectedCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_WifiConnectedCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_WifiConnectedCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_WifiConnectedPanel = lv_obj_create(ui_WifiConnectedCont);
  lv_obj_set_width(ui_WifiConnectedPanel, 411);
  lv_obj_set_height(ui_WifiConnectedPanel, 45);
  lv_obj_set_align(ui_WifiConnectedPanel, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_WifiConnectedPanel, LV_OBJ_FLAG_SCROLLABLE);

  ui_ArrowWifiConnected = lv_label_create(ui_WifiConnectedCont);
  lv_obj_set_width(ui_ArrowWifiConnected, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ArrowWifiConnected, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ArrowWifiConnected, 155);
  lv_obj_set_y(ui_ArrowWifiConnected, 0);
  lv_obj_set_align(ui_ArrowWifiConnected, LV_ALIGN_CENTER);
  lv_label_set_text(ui_ArrowWifiConnected, ">");
  lv_obj_set_style_text_font(ui_ArrowWifiConnected, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_WifiSSIDLabel = lv_label_create(ui_WifiConnectedCont);
  lv_obj_set_width(ui_WifiSSIDLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_WifiSSIDLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_WifiSSIDLabel, 29);
  lv_obj_set_y(ui_WifiSSIDLabel, 0);
  lv_obj_set_align(ui_WifiSSIDLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_WifiSSIDLabel, "SSID NAME");
  lv_obj_set_style_text_color(ui_WifiSSIDLabel, lv_color_hex(0x27D331),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_WifiSSIDLabel, 255,
                            LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_WifiConnectedToLabel = lv_label_create(ui_WifiConnectedCont);
  lv_obj_set_width(ui_WifiConnectedToLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_WifiConnectedToLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_WifiConnectedToLabel, -111);
  lv_obj_set_y(ui_WifiConnectedToLabel, 0);
  lv_obj_set_align(ui_WifiConnectedToLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_WifiConnectedToLabel, "Connected to: ");

  lv_obj_add_event_cb(ui_ImgButton2, ui_event_ImgButton2, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_InfoButton, ui_event_InfoButton, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_WifiButton, ui_event_WifiButton, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_LanguagesButton, ui_event_LanguagesButton,
                      LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Switch4, ui_event_Switch4, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_SwitchDarkMode, ui_event_SwitchDarkMode, LV_EVENT_ALL,
                      NULL);
  lv_obj_add_event_cb(ui_SwitchHumidityMode, ui_event_SwitchHumidityMode,
                      LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_TextArea1, ui_event_TextArea1, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_TextArea2, ui_event_TextArea2, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_Keyboard1, ui_event_Keyboard1, LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_WifiConnectButton, ui_event_WifiConnectButton,
                      LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_WifiDisconnectButton, ui_event_WifiDisconnectButton,
                      LV_EVENT_ALL, NULL);
  lv_obj_add_event_cb(ui_LanguagesDropDown, ui_event_LanguagesDropDown,
                      LV_EVENT_ALL, NULL);

  lv_obj_set_ext_click_area(ui_ImgButton2, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_Switch4, TOUCH_EXT_NARROW);
  lv_obj_set_ext_click_area(ui_SwitchDarkMode, TOUCH_EXT_NARROW);
  lv_obj_set_ext_click_area(ui_SwitchHumidityMode, TOUCH_EXT_NARROW);
  lv_obj_set_ext_click_area(ui_WifiConnectButton, TOUCH_EXT_NARROW);
  lv_obj_set_ext_click_area(ui_WifiDisconnectButton, TOUCH_EXT_NARROW);
}
#undef LVGL_INIT_GUARD_ROOT
#undef LVGL_INIT_GUARD_CHILD

void ui_ScreenLock_screen_init(void) {
  ui_ScreenLock = lv_obj_create(NULL);
  lv_obj_clear_flag(ui_ScreenLock, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_ScreenLock, lv_color_hex(0x242323),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_ScreenLock, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LockButton = lv_imgbtn_create(ui_ScreenLock);
  lv_imgbtn_set_src(ui_LockButton, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_candado_png, NULL);
  lv_obj_set_width(ui_LockButton, 38);
  lv_obj_set_height(ui_LockButton, 44);
  lv_obj_set_x(ui_LockButton, -3);
  lv_obj_set_y(ui_LockButton, -213);
  lv_obj_set_align(ui_LockButton, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_LockButton, LV_OBJ_FLAG_HIDDEN);

  ui_Container1 = lv_obj_create(ui_ScreenLock);
  lv_obj_remove_style_all(ui_Container1);
  lv_obj_set_width(ui_Container1, 790);
  lv_obj_set_height(ui_Container1, 300);
  lv_obj_set_x(ui_Container1, 0);
  lv_obj_set_y(ui_Container1, -30);
  lv_obj_set_align(ui_Container1, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Container1,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_AirTempLockCont = lv_obj_create(ui_Container1);
  lv_obj_remove_style_all(ui_AirTempLockCont);
  lv_obj_set_width(ui_AirTempLockCont, 300);
  lv_obj_set_height(ui_AirTempLockCont, 100);
  lv_obj_set_x(ui_AirTempLockCont, -250);
  lv_obj_set_y(ui_AirTempLockCont, -90);
  lv_obj_set_align(ui_AirTempLockCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_AirTempLockCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Label11 = lv_label_create(ui_AirTempLockCont);
  lv_obj_set_width(ui_Label11, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label11, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label11, 30);
  lv_obj_set_y(ui_Label11, -30);
  lv_obj_set_align(ui_Label11, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_Label11, "AIR TEMPERATURE:");
  lv_obj_set_style_text_color(ui_Label11, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label11, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ImageWindLS = lv_img_create(ui_AirTempLockCont);
  lv_img_set_src(ui_ImageWindLS, &ui_img_windvector_png);
  lv_obj_set_width(ui_ImageWindLS, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ImageWindLS, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ImageWindLS, 30);
  lv_obj_set_y(ui_ImageWindLS, 25);
  lv_obj_set_align(ui_ImageWindLS, LV_ALIGN_LEFT_MID);
  lv_obj_add_flag(ui_ImageWindLS, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ImageWindLS, LV_OBJ_FLAG_SCROLLABLE);

  ui_Label18 = lv_label_create(ui_AirTempLockCont);
  lv_obj_set_width(ui_Label18, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label18, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label18, 130);
  lv_obj_set_y(ui_Label18, 25);
  lv_obj_set_align(ui_Label18, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_Label18, "37");
  lv_obj_set_style_text_color(ui_Label18, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label18, &lv_font_montserrat_48,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_SkinTempLockCont = lv_obj_create(ui_Container1);
  lv_obj_remove_style_all(ui_SkinTempLockCont);
  lv_obj_set_width(ui_SkinTempLockCont, 300);
  lv_obj_set_height(ui_SkinTempLockCont, 100);
  lv_obj_set_x(ui_SkinTempLockCont, -250);
  lv_obj_set_y(ui_SkinTempLockCont, 0);
  lv_obj_set_align(ui_SkinTempLockCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_SkinTempLockCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Label12 = lv_label_create(ui_SkinTempLockCont);
  lv_obj_set_width(ui_Label12, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label12, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label12, 30);
  lv_obj_set_y(ui_Label12, -30);
  lv_obj_set_align(ui_Label12, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_Label12, "BABY TEMPERATURE:");
  lv_obj_set_style_text_color(ui_Label12, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label12, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ImageBabyLS = lv_img_create(ui_SkinTempLockCont);
  lv_img_set_src(ui_ImageBabyLS, &ui_img_bebe_icon_png);
  lv_obj_set_width(ui_ImageBabyLS, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ImageBabyLS, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ImageBabyLS, 30);
  lv_obj_set_y(ui_ImageBabyLS, 25);
  lv_obj_set_align(ui_ImageBabyLS, LV_ALIGN_LEFT_MID);
  lv_obj_add_flag(ui_ImageBabyLS, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ImageBabyLS, LV_OBJ_FLAG_SCROLLABLE);

  ui_Label14 = lv_label_create(ui_SkinTempLockCont);
  lv_obj_set_width(ui_Label14, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label14, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label14, 130);
  lv_obj_set_y(ui_Label14, 25);
  lv_obj_set_align(ui_Label14, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_Label14, "37");
  lv_obj_set_style_text_color(ui_Label14, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label14, &lv_font_montserrat_48,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_HumLockCont = lv_obj_create(ui_Container1);
  lv_obj_remove_style_all(ui_HumLockCont);
  lv_obj_set_width(ui_HumLockCont, 300);
  lv_obj_set_height(ui_HumLockCont, 100);
  lv_obj_set_x(ui_HumLockCont, -250);
  lv_obj_set_y(ui_HumLockCont, 90);
  lv_obj_set_align(ui_HumLockCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_HumLockCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Label19 = lv_label_create(ui_HumLockCont);
  lv_obj_set_width(ui_Label19, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label19, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label19, 30);
  lv_obj_set_y(ui_Label19, -30);
  lv_obj_set_align(ui_Label19, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_Label19, "HUMIDITY:");
  lv_obj_set_style_text_color(ui_Label19, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label19, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ImagenWaterLS = lv_img_create(ui_HumLockCont);
  lv_img_set_src(ui_ImagenWaterLS, &ui_img_302897630);
  lv_obj_set_width(ui_ImagenWaterLS, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ImagenWaterLS, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ImagenWaterLS, 30);
  lv_obj_set_y(ui_ImagenWaterLS, 25);
  lv_obj_set_align(ui_ImagenWaterLS, LV_ALIGN_LEFT_MID);
  lv_obj_add_flag(ui_ImagenWaterLS, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ImagenWaterLS, LV_OBJ_FLAG_SCROLLABLE);

  ui_Label20 = lv_label_create(ui_HumLockCont);
  lv_obj_set_width(ui_Label20, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label20, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label20, 130);
  lv_obj_set_y(ui_Label20, 25);
  lv_obj_set_align(ui_Label20, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_Label20, "37");
  lv_obj_set_style_text_color(ui_Label20, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label20, &lv_font_montserrat_48,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_HumLockDesiredCont = lv_obj_create(ui_Container1);
  lv_obj_remove_style_all(ui_HumLockDesiredCont);
  lv_obj_set_width(ui_HumLockDesiredCont, 220);
  lv_obj_set_height(ui_HumLockDesiredCont, 100);
  lv_obj_set_x(ui_HumLockDesiredCont, 40);
  lv_obj_set_y(ui_HumLockDesiredCont, 90);
  lv_obj_set_align(ui_HumLockDesiredCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_HumLockDesiredCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Label23 = lv_label_create(ui_HumLockDesiredCont);
  lv_obj_set_width(ui_Label23, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label23, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label23, 20); // Reajustado a la derecha (era 5)
  lv_obj_set_y(ui_Label23, -30);
  lv_obj_set_align(ui_Label23, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_Label23, "TARGET HUMIDITY:");
  lv_obj_set_style_text_color(ui_Label23, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label23, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Label24 = lv_label_create(ui_HumLockDesiredCont);
  lv_obj_set_width(ui_Label24, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label24, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_Label24, 20); // Reajustado a la derecha (era 5)
  lv_obj_set_y(ui_Label24, 25);
  lv_obj_set_align(ui_Label24, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_Label24, "37");
  lv_obj_set_style_text_color(ui_Label24, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label24, &lv_font_montserrat_48,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ArrowHumLock = lv_img_create(ui_Container1);
  lv_img_set_src(ui_ArrowHumLock, &ui_img_flecha_png);
  lv_obj_set_width(ui_ArrowHumLock, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ArrowHumLock, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ArrowHumLock, -90); // Movido a la izquierda (era -70)
  lv_obj_set_y(ui_ArrowHumLock, 115);
  lv_obj_set_align(ui_ArrowHumLock, LV_ALIGN_CENTER);
  lv_img_set_zoom(ui_ArrowHumLock, 100);
  lv_obj_add_flag(ui_ArrowHumLock, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ArrowHumLock, LV_OBJ_FLAG_SCROLLABLE);

  ui_TargetSkinTempCont = lv_obj_create(ui_Container1);
  lv_obj_remove_style_all(ui_TargetSkinTempCont);
  lv_obj_set_width(ui_TargetSkinTempCont, 220);
  lv_obj_set_height(ui_TargetSkinTempCont, 100);
  lv_obj_set_x(ui_TargetSkinTempCont, 40);
  lv_obj_set_y(ui_TargetSkinTempCont, 0);
  lv_obj_set_align(ui_TargetSkinTempCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_TargetSkinTempCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_TargetSkinTempLabel = lv_label_create(ui_TargetSkinTempCont);
  lv_obj_set_width(ui_TargetSkinTempLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TargetSkinTempLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_TargetSkinTempLabel, 20); // Reajustado a la derecha (era 5)
  lv_obj_set_y(ui_TargetSkinTempLabel, -30);
  lv_obj_set_align(ui_TargetSkinTempLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_TargetSkinTempLabel, "TARGET TEMPERATURE:");
  lv_obj_set_style_text_color(ui_TargetSkinTempLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_TargetSkinTempLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_TargetSkinTempNumLabel = lv_label_create(ui_TargetSkinTempCont);
  lv_obj_set_width(ui_TargetSkinTempNumLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TargetSkinTempNumLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_TargetSkinTempNumLabel,
               20); // Reajustado a la derecha (era 5)
  lv_obj_set_y(ui_TargetSkinTempNumLabel, 25);
  lv_obj_set_align(ui_TargetSkinTempNumLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_TargetSkinTempNumLabel, "37");
  lv_obj_set_style_text_color(ui_TargetSkinTempNumLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_TargetSkinTempNumLabel, &lv_font_montserrat_48,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ArrowSkinLock = lv_img_create(ui_Container1);
  lv_img_set_src(ui_ArrowSkinLock, &ui_img_flecha_png);
  lv_obj_set_width(ui_ArrowSkinLock, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ArrowSkinLock, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ArrowSkinLock, -90); // Movido a la izquierda (era -70)
  lv_obj_set_y(ui_ArrowSkinLock, 25);
  lv_obj_set_align(ui_ArrowSkinLock, LV_ALIGN_CENTER);
  lv_img_set_zoom(ui_ArrowSkinLock, 100);
  lv_obj_add_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ArrowSkinLock, LV_OBJ_FLAG_SCROLLABLE);

  ui_TargetAirTempCont = lv_obj_create(ui_Container1);
  lv_obj_remove_style_all(ui_TargetAirTempCont);
  lv_obj_set_width(ui_TargetAirTempCont, 220);
  lv_obj_set_height(ui_TargetAirTempCont, 100);
  lv_obj_set_x(ui_TargetAirTempCont, 40);
  lv_obj_set_y(ui_TargetAirTempCont, -90);
  lv_obj_set_align(ui_TargetAirTempCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_TargetAirTempCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_TargetAirTempLabel = lv_label_create(ui_TargetAirTempCont);
  lv_obj_set_width(ui_TargetAirTempLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TargetAirTempLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_TargetAirTempLabel, 20); // Reajustado a la derecha (era 5)
  lv_obj_set_y(ui_TargetAirTempLabel, -30);
  lv_obj_set_align(ui_TargetAirTempLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_TargetAirTempLabel, "TARGET TEMPERATURE:");
  lv_obj_set_style_text_color(ui_TargetAirTempLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_TargetAirTempLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_TargetAirTempNumLabel = lv_label_create(ui_TargetAirTempCont);
  lv_obj_set_width(ui_TargetAirTempNumLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_TargetAirTempNumLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_TargetAirTempNumLabel, 20); // Reajustado a la derecha (era 5)
  lv_obj_set_y(ui_TargetAirTempNumLabel, 25);
  lv_obj_set_align(ui_TargetAirTempNumLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_TargetAirTempNumLabel, "37");
  lv_obj_set_style_text_color(ui_TargetAirTempNumLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_TargetAirTempNumLabel, &lv_font_montserrat_48,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_ArrowAirLock = lv_img_create(ui_Container1);
  lv_img_set_src(ui_ArrowAirLock, &ui_img_flecha_png);
  lv_obj_set_width(ui_ArrowAirLock, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_ArrowAirLock, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_ArrowAirLock, -90); // Movido a la izquierda (era -70)
  lv_obj_set_y(ui_ArrowAirLock, -65);
  lv_obj_set_align(ui_ArrowAirLock, LV_ALIGN_CENTER);
  lv_img_set_zoom(ui_ArrowAirLock, 100);
  lv_obj_add_flag(ui_ArrowAirLock, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_ArrowAirLock, LV_OBJ_FLAG_SCROLLABLE);

  ui_StatusCont = lv_obj_create(ui_ScreenLock);
  lv_obj_remove_style_all(ui_StatusCont);
  lv_obj_set_width(ui_StatusCont, 350); // Unificado con PhotoLockCont
  lv_obj_set_height(ui_StatusCont, 50);
  lv_obj_set_x(ui_StatusCont, 240);
  lv_obj_set_y(ui_StatusCont, -120);
  lv_obj_set_align(ui_StatusCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_StatusCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_StatusLabel = lv_label_create(ui_StatusCont);
  lv_obj_set_width(ui_StatusLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_StatusLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_StatusLabel, 100); // Movido a la derecha
  lv_obj_set_y(ui_StatusLabel, 0);
  lv_obj_set_align(ui_StatusLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_StatusLabel, "STATUS:");
  lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_StatusLabel, &lv_font_montserrat_20,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // Phototherapy Lock Screen Container
  ui_PhotoLockCont = lv_obj_create(ui_ScreenLock);
  lv_obj_remove_style_all(ui_PhotoLockCont);
  lv_obj_set_width(ui_PhotoLockCont, 350);
  lv_obj_set_height(ui_PhotoLockCont, 60);
  lv_obj_set_x(ui_PhotoLockCont, 240);
  lv_obj_set_y(ui_PhotoLockCont, -60); // Debajo de Status
  lv_obj_set_align(ui_PhotoLockCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_PhotoLockCont, LV_OBJ_FLAG_HIDDEN); // Initially hidden
  lv_obj_clear_flag(ui_PhotoLockCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_PhotoLockLabel = lv_label_create(ui_PhotoLockCont);
  lv_obj_set_width(ui_PhotoLockLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_PhotoLockLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_PhotoLockLabel, 100); // Alineación exacta con StatusLabel
  lv_obj_set_y(ui_PhotoLockLabel, -10);
  lv_obj_set_align(ui_PhotoLockLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_PhotoLockLabel, "FOTOTERAPIA:");
  lv_obj_set_style_text_color(ui_PhotoLockLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_PhotoLockLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_PhotoLockTimeLabel = lv_label_create(ui_PhotoLockCont);
  lv_obj_set_width(ui_PhotoLockTimeLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_PhotoLockTimeLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_PhotoLockTimeLabel, 100); // Movido a la derecha
  lv_obj_set_y(ui_PhotoLockTimeLabel, 15);
  lv_obj_set_align(ui_PhotoLockTimeLabel, LV_ALIGN_LEFT_MID);
  lv_label_set_text(ui_PhotoLockTimeLabel, "0:00");
  lv_obj_set_style_text_color(ui_PhotoLockTimeLabel, lv_color_hex(0x00FF00),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_PhotoLockTimeLabel, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_AlarmLockCont = lv_obj_create(ui_ScreenLock);
  lv_obj_remove_style_all(ui_AlarmLockCont);
  lv_obj_set_width(ui_AlarmLockCont, 100);
  lv_obj_set_height(ui_AlarmLockCont, 100);
  lv_obj_set_x(ui_AlarmLockCont, 350);
  lv_obj_set_y(ui_AlarmLockCont, -120);
  lv_obj_set_align(ui_AlarmLockCont, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_AlarmLockCont, LV_OBJ_FLAG_SCROLLABLE);

  ui_AlarmLockImg = lv_imgbtn_create(ui_AlarmLockCont);
  lv_imgbtn_set_src(ui_AlarmLockImg, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_1007688293, NULL);
  lv_obj_set_width(ui_AlarmLockImg, 48);
  lv_obj_set_height(ui_AlarmLockImg, 47);
  lv_obj_set_align(ui_AlarmLockImg, LV_ALIGN_CENTER);

  ui_PanelLockAlarm = lv_obj_create(ui_AlarmLockCont);
  lv_obj_set_width(ui_PanelLockAlarm, 24);
  lv_obj_set_height(ui_PanelLockAlarm, 27);
  lv_obj_set_x(ui_PanelLockAlarm, 15);
  lv_obj_set_y(ui_PanelLockAlarm, -15);
  lv_obj_set_align(ui_PanelLockAlarm, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_PanelLockAlarm, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_PanelLockAlarm, lv_color_hex(0xFF0000),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_PanelLockAlarm, 255,
                          LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_AlarmLockNumLabel = lv_label_create(ui_AlarmLockCont);
  lv_obj_set_width(ui_AlarmLockNumLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_AlarmLockNumLabel, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_AlarmLockNumLabel, 15);
  lv_obj_set_y(ui_AlarmLockNumLabel, -15);
  lv_obj_set_align(ui_AlarmLockNumLabel, LV_ALIGN_CENTER);
  lv_label_set_text(ui_AlarmLockNumLabel, "1");
  lv_obj_set_style_text_font(ui_AlarmLockNumLabel, &lv_font_montserrat_18,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_CheckImg = lv_img_create(ui_ScreenLock);
  lv_img_set_src(ui_CheckImg, &ui_img_check_png);
  lv_obj_set_width(ui_CheckImg, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_CheckImg, LV_SIZE_CONTENT);
  lv_obj_set_x(ui_CheckImg, 350);
  lv_obj_set_y(ui_CheckImg, -120);
  lv_obj_set_align(ui_CheckImg, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_CheckImg, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_CheckImg, LV_OBJ_FLAG_SCROLLABLE);
  lv_img_set_zoom(ui_CheckImg, 200);

  // --- UNLOCK POPUP (on top layer, border-fill progress) ---
  ui_UnlockCont = lv_obj_create(ui_ScreenLock);
  lv_obj_remove_style_all(ui_UnlockCont);
  lv_obj_set_size(ui_UnlockCont, UNLOCK_POPUP_W, UNLOCK_POPUP_H);
  lv_obj_set_align(ui_UnlockCont, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_UnlockCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui_UnlockCont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

  ui_Panel11 = lv_obj_create(ui_UnlockCont);
  lv_obj_set_size(ui_Panel11, UNLOCK_POPUP_W, UNLOCK_POPUP_H);
  lv_obj_set_align(ui_Panel11, LV_ALIGN_CENTER);
  lv_obj_clear_flag(ui_Panel11, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_Panel11, lv_color_hex(0x000000),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_Panel11, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui_Panel11, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_Panel11, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(ui_Panel11, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_Label4 = lv_label_create(ui_UnlockCont);
  lv_obj_set_width(ui_Label4, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_Label4, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_Label4, LV_ALIGN_CENTER);
  lv_obj_set_y(ui_Label4, -50);
  lv_label_set_text(ui_Label4, "Keep pressed\nto unlock");
  lv_obj_set_style_text_color(ui_Label4, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_opa(ui_Label4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_Label4, &lv_font_montserrat_26,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(ui_Label4, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LockButton2 = lv_imgbtn_create(ui_UnlockCont);
  lv_imgbtn_set_src(ui_LockButton2, LV_IMGBTN_STATE_RELEASED, NULL,
                    &ui_img_candado_png, NULL);
  lv_obj_set_size(ui_LockButton2, 52, 60);
  lv_obj_set_align(ui_LockButton2, LV_ALIGN_CENTER);
  lv_obj_set_y(ui_LockButton2, 50);
  lv_obj_set_style_img_recolor(ui_LockButton2, lv_color_hex(0xFFFFFF),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_img_recolor_opa(ui_LockButton2, 255,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);

  // Border-fill bars: grow clockwise (top→right→bottom→left) as user holds
  // All start at 0 size; UITask's lock_progress_timer_cb drives the growth.
  ui_BorderTop = lv_obj_create(ui_Panel11);
  lv_obj_remove_style_all(ui_BorderTop);
  lv_obj_set_size(ui_BorderTop, 0, UNLOCK_BORDER_T);
  lv_obj_set_align(ui_BorderTop, LV_ALIGN_TOP_LEFT);
  lv_obj_clear_flag(ui_BorderTop, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_BorderTop, lv_color_hex(0x4EC7FF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_BorderTop, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_BorderTop, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_BorderRight = lv_obj_create(ui_Panel11);
  lv_obj_remove_style_all(ui_BorderRight);
  lv_obj_set_size(ui_BorderRight, UNLOCK_BORDER_T, 0);
  lv_obj_set_align(ui_BorderRight, LV_ALIGN_TOP_LEFT);
  lv_obj_set_x(ui_BorderRight, UNLOCK_POPUP_W - UNLOCK_BORDER_T);
  lv_obj_clear_flag(ui_BorderRight, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_BorderRight, lv_color_hex(0x4EC7FF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_BorderRight, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_BorderRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_BorderBottom = lv_obj_create(ui_Panel11);
  lv_obj_remove_style_all(ui_BorderBottom);
  lv_obj_set_size(ui_BorderBottom, 0, UNLOCK_BORDER_T);
  lv_obj_set_align(ui_BorderBottom, LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(ui_BorderBottom, UNLOCK_POPUP_W, UNLOCK_POPUP_H - UNLOCK_BORDER_T);
  lv_obj_clear_flag(ui_BorderBottom, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_BorderBottom, lv_color_hex(0x4EC7FF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_BorderBottom, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_BorderBottom, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_BorderLeft = lv_obj_create(ui_Panel11);
  lv_obj_remove_style_all(ui_BorderLeft);
  lv_obj_set_size(ui_BorderLeft, UNLOCK_BORDER_T, 0);
  lv_obj_set_align(ui_BorderLeft, LV_ALIGN_TOP_LEFT);
  lv_obj_set_pos(ui_BorderLeft, 0, UNLOCK_POPUP_H);
  lv_obj_clear_flag(ui_BorderLeft, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(ui_BorderLeft, lv_color_hex(0x4EC7FF),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_BorderLeft, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(ui_BorderLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  // --------------------------------------------------------------------------

  // --- PPG Chart (lock screen, bottom-left) ---
  ui_LockPPGChart = lv_chart_create(ui_ScreenLock);
  lv_obj_set_size(ui_LockPPGChart, 550, 105);
  lv_obj_set_align(ui_LockPPGChart, LV_ALIGN_BOTTOM_LEFT);
  lv_obj_set_x(ui_LockPPGChart, 10);
  lv_obj_set_y(ui_LockPPGChart, -5);
  lv_obj_clear_flag(ui_LockPPGChart, LV_OBJ_FLAG_SCROLLABLE);
  lv_chart_set_type(ui_LockPPGChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(ui_LockPPGChart, 256);
  lv_chart_set_update_mode(ui_LockPPGChart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_range(ui_LockPPGChart, LV_CHART_AXIS_PRIMARY_Y, 0, 255);
  lv_chart_set_div_line_count(ui_LockPPGChart, 0, 0);
  lv_chart_set_axis_tick(ui_LockPPGChart, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 0, 0,
                         false, 0);
  lv_chart_set_axis_tick(ui_LockPPGChart, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0,
                         false, 0);
  lv_obj_set_style_bg_color(ui_LockPPGChart, lv_color_hex(0x242323),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_LockPPGChart, 255,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui_LockPPGChart, 0,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(ui_LockPPGChart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_size(ui_LockPPGChart, 0,
                        LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_line_color(ui_LockPPGChart, lv_color_hex(0x00FF7F),
                              LV_PART_ITEMS | LV_STATE_DEFAULT);
  lv_obj_set_style_line_width(ui_LockPPGChart, 2,
                              LV_PART_ITEMS | LV_STATE_DEFAULT);
  lockPPGSeries = lv_chart_add_series(ui_LockPPGChart, lv_color_hex(0x00FF7F),
                                      LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < lv_chart_get_point_count(ui_LockPPGChart); i++) {
    lockPPGSeries->y_points[i] = LV_CHART_POINT_NONE;
  }
  lv_chart_refresh(ui_LockPPGChart);
  lv_obj_add_flag(ui_LockPPGChart, LV_OBJ_FLAG_EVENT_BUBBLE);

  // --- HR Container (lock screen, bottom-right) ---
  ui_LockHRCont = lv_obj_create(ui_ScreenLock);
  lv_obj_remove_style_all(ui_LockHRCont);
  lv_obj_set_size(ui_LockHRCont, 110, 105);
  lv_obj_set_align(ui_LockHRCont, LV_ALIGN_BOTTOM_RIGHT);
  lv_obj_set_x(ui_LockHRCont, -125);
  lv_obj_set_y(ui_LockHRCont, -5);
  lv_obj_add_flag(ui_LockHRCont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_LockHRCont, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_clear_flag(ui_LockHRCont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(ui_LockHRCont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ui_LockHRCont, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(ui_LockHRCont, 2,
                           LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *ui_LockHRImg = lv_img_create(ui_LockHRCont);
  lv_img_set_src(ui_LockHRImg, &ui_img_heart_red_png);
  lv_obj_set_width(ui_LockHRImg, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LockHRImg, LV_SIZE_CONTENT);
  lv_obj_add_flag(ui_LockHRImg, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_clear_flag(ui_LockHRImg, LV_OBJ_FLAG_SCROLLABLE);

  ui_LockHRLabel = lv_label_create(ui_LockHRCont);
  lv_obj_set_width(ui_LockHRLabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LockHRLabel, LV_SIZE_CONTENT);
  lv_label_set_text(ui_LockHRLabel, "--");
  lv_obj_set_style_text_color(ui_LockHRLabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LockHRLabel, &lv_font_montserrat_48,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *ui_LockHRUnit = lv_label_create(ui_LockHRCont);
  lv_obj_set_width(ui_LockHRUnit, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LockHRUnit, LV_SIZE_CONTENT);
  lv_label_set_text(ui_LockHRUnit, "BPM");
  lv_obj_set_style_text_color(ui_LockHRUnit, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LockHRUnit, &lv_font_montserrat_14,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  // --- PI Container (lock screen, bottom-right, to the right of HR) ---
  ui_LockPICont = lv_obj_create(ui_ScreenLock);
  lv_obj_remove_style_all(ui_LockPICont);
  lv_obj_set_size(ui_LockPICont, 110, 105);
  lv_obj_set_align(ui_LockPICont, LV_ALIGN_BOTTOM_RIGHT);
  lv_obj_set_x(ui_LockPICont, -10);
  lv_obj_set_y(ui_LockPICont, -5);
  lv_obj_add_flag(ui_LockPICont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_LockPICont, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_clear_flag(ui_LockPICont,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(ui_LockPICont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ui_LockPICont, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(ui_LockPICont, 2,
                           LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *ui_LockPITitle = lv_label_create(ui_LockPICont);
  lv_obj_set_width(ui_LockPITitle, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LockPITitle, LV_SIZE_CONTENT);
  lv_label_set_text(ui_LockPITitle, "PI");
  lv_obj_set_style_text_color(ui_LockPITitle, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LockPITitle, &lv_font_montserrat_14,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  ui_LockPILabel = lv_label_create(ui_LockPICont);
  lv_obj_set_width(ui_LockPILabel, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LockPILabel, LV_SIZE_CONTENT);
  lv_label_set_text(ui_LockPILabel, "--");
  lv_obj_set_style_text_color(ui_LockPILabel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LockPILabel, &lv_font_montserrat_48,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *ui_LockPIUnit = lv_label_create(ui_LockPICont);
  lv_obj_set_width(ui_LockPIUnit, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LockPIUnit, LV_SIZE_CONTENT);
  lv_label_set_text(ui_LockPIUnit, "%");
  lv_obj_set_style_text_color(ui_LockPIUnit, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ui_LockPIUnit, &lv_font_montserrat_12,
                             LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_add_event_cb(ui_AlarmLockImg, ui_event_AlarmLockImg, LV_EVENT_ALL,
                      NULL);
  lv_obj_add_event_cb(ui_AlarmLockCont, ui_event_AlarmLockCont, LV_EVENT_ALL,
                      NULL);
  lv_obj_add_event_cb(ui_ScreenLock, ui_event_ScreenLock, LV_EVENT_ALL, NULL);

  lv_obj_set_ext_click_area(ui_LockButton, TOUCH_EXT_SMALL);
  lv_obj_set_ext_click_area(ui_AlarmLockImg, TOUCH_EXT_MEDIUM);
  lv_obj_set_ext_click_area(ui_AlarmLockCont, TOUCH_EXT_NARROW);
}

// ============================================================================
// AUTO AIR — widget creation (logic/callbacks stay in UITask.cpp)
// ============================================================================

// Helper: create a glove-friendly +/- button for the popup
static lv_obj_t *aa_make_spinbtn(lv_obj_t *parent, const char *text,
                                 lv_event_cb_t cb) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 64, 56);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x0075EE), LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  return btn;
}

// Build one input row for the left column (fieldName + units sublabel + value +
// dec/inc). Returns the value display label.
static lv_obj_t *aa_make_input_row(lv_obj_t *parent, const char *fieldName,
                                   const char *fieldUnits, int yOffset,
                                   lv_event_cb_t decCb, lv_event_cb_t incCb,
                                   lv_obj_t **unitsLblOut = nullptr,
                                   lv_obj_t **rowOut = nullptr,
                                   lv_obj_t **nameLblOut = nullptr,
                                   int unitsXOffset = 85) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, 295, 76);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, yOffset);
  lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(row, lv_color_hex(0xDDDDDD), LV_PART_MAIN);
  lv_obj_set_style_radius(row, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_all(row, 6, LV_PART_MAIN);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  // Field name (e.g. "Gestational Age")
  lv_obj_t *nameLbl = lv_label_create(row);
  lv_label_set_text(nameLbl, fieldName);
  lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(nameLbl, lv_color_hex(0x555555), 0);
  lv_obj_align(nameLbl, LV_ALIGN_TOP_LEFT, 2, 0);

  // Value label
  lv_obj_t *val = lv_label_create(row);
  lv_label_set_text(val, "--");
  lv_obj_set_style_text_font(val, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(val, lv_color_hex(0x0075EE), 0);
  lv_obj_align(val, LV_ALIGN_LEFT_MID, 2, 6);

  // Units sublabel (e.g. "WEEKS") — anchored to the right, before the buttons
  lv_obj_t *unitsLbl = lv_label_create(row);
  lv_label_set_text(unitsLbl, fieldUnits);
  lv_obj_set_style_text_font(unitsLbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(unitsLbl, lv_color_hex(0x000000), 0);
  lv_obj_align(unitsLbl, LV_ALIGN_LEFT_MID, unitsXOffset, 8);
  if (nameLblOut)
    *nameLblOut = nameLbl;
  if (unitsLblOut)
    *unitsLblOut = unitsLbl;
  if (rowOut)
    *rowOut = row;

  // Dec (▼) button — override size after aa_make_spinbtn
  lv_obj_t *btnDec = aa_make_spinbtn(row, LV_SYMBOL_DOWN, decCb);
  lv_obj_set_size(btnDec, 64, 40);
  lv_obj_align(btnDec, LV_ALIGN_RIGHT_MID, -78, 0);

  // Inc (▲) button
  lv_obj_t *btnInc = aa_make_spinbtn(row, LV_SYMBOL_UP, incCb);
  lv_obj_set_size(btnInc, 64, 40);
  lv_obj_align(btnInc, LV_ALIGN_RIGHT_MID, -2, 0);

  return val;
}

void create_autoair_popup() {
  // Full-screen dim overlay; child of ScreenMain so it persists after intro
  ui_AutoAirOverlay = lv_obj_create(ui_ScreenMain);
  lv_obj_set_size(ui_AutoAirOverlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(ui_AutoAirOverlay, 0, 0);
  lv_obj_set_style_bg_color(ui_AutoAirOverlay, lv_color_hex(0x000000),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_AutoAirOverlay, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_border_width(ui_AutoAirOverlay, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(ui_AutoAirOverlay, 0, LV_PART_MAIN);
  lv_obj_clear_flag(ui_AutoAirOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ui_AutoAirOverlay, LV_OBJ_FLAG_HIDDEN);

  // Modal — wide landscape layout (700×420)
  ui_AutoAirModal = lv_obj_create(ui_AutoAirOverlay);
  lv_obj_t *modal = ui_AutoAirModal;
  lv_obj_set_size(modal, 700, 420);
  lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(modal, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(modal, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(modal, lv_color_hex(0x0075EE), LV_PART_MAIN);
  lv_obj_set_style_pad_all(modal, 0, LV_PART_MAIN);
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

  // ── Title bar ──────────────────────────────────────────────
  ui_AutoAirTitle = lv_label_create(modal);
  lv_obj_t *title = ui_AutoAirTitle;
  lv_label_set_text(title, "COMFORT ZONE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x0075EE), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 6);

  lv_obj_t *citLbl = lv_label_create(modal);
  lv_label_set_text(
      citLbl, "BASED ON: SAUER, DANE & VISSER (1984); DEACON & O'NEILL (2004); "
              "CLOHERTY 3RD ED.; WHO/AAP GUIDELINES");
  lv_obj_set_style_text_font(citLbl, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(citLbl, lv_color_hex(0x000000), 0);
  lv_obj_set_width(citLbl, 620);
  lv_label_set_long_mode(citLbl, LV_LABEL_LONG_WRAP);
  lv_obj_align(citLbl, LV_ALIGN_TOP_LEFT, 16, 33);

  lv_obj_t *btnClose = lv_btn_create(modal);
  lv_obj_set_size(btnClose, 32, 32);
  lv_obj_align(btnClose, LV_ALIGN_TOP_RIGHT, -8, 8);
  lv_obj_set_style_bg_color(btnClose, lv_color_hex(0x888888), LV_PART_MAIN);
  lv_obj_set_style_radius(btnClose, 16, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btnClose, 0, LV_PART_MAIN);
  lv_obj_t *closeLbl = lv_label_create(btnClose);
  lv_label_set_text(closeLbl, LV_SYMBOL_CLOSE);
  lv_obj_set_style_text_font(closeLbl, &lv_font_montserrat_16, 0);
  lv_obj_center(closeLbl);
  lv_obj_add_event_cb(btnClose, aa_cancel_cb, LV_EVENT_CLICKED, nullptr);

  // Horizontal separator
  ui_AutoAirHSep = lv_obj_create(modal);
  lv_obj_t *hSep = ui_AutoAirHSep;
  lv_obj_set_size(hSep, 698, 2);
  lv_obj_set_pos(hSep, 1, 56);
  lv_obj_set_style_bg_color(hSep, lv_color_hex(0xDDDDDD), LV_PART_MAIN);
  lv_obj_set_style_border_width(hSep, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(hSep, 0, LV_PART_MAIN);

  // Vertical separator between columns
  ui_AutoAirVSep = lv_obj_create(modal);
  lv_obj_t *vSep = ui_AutoAirVSep;
  lv_obj_set_size(vSep, 2, 362);
  lv_obj_set_pos(vSep, 329, 58);
  lv_obj_set_style_bg_color(vSep, lv_color_hex(0xDDDDDD), LV_PART_MAIN);
  lv_obj_set_style_border_width(vSep, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(vSep, 0, LV_PART_MAIN);

  // ══════════════════════════════════════════════════════════
  // LEFT COLUMN — Baby Information
  // ══════════════════════════════════════════════════════════
  lv_obj_t *colLeft = lv_obj_create(modal);
  lv_obj_set_size(colLeft, 327, 362);
  lv_obj_set_pos(colLeft, 2, 58);
  lv_obj_set_style_border_width(colLeft, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(colLeft, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_pad_all(colLeft, 10, LV_PART_MAIN);
  lv_obj_clear_flag(colLeft, LV_OBJ_FLAG_SCROLLABLE);

  ui_AutoAirLeftHeader = lv_label_create(colLeft);
  lv_obj_t *leftHeader = ui_AutoAirLeftHeader;
  lv_label_set_text(leftHeader, "BABY INFORMATION:");
  lv_obj_set_style_text_font(leftHeader, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(leftHeader, lv_color_hex(0x555555), 0);
  lv_obj_align(leftHeader, LV_ALIGN_TOP_LEFT, 0, 0);

  // Field rows (y offsets relative to colLeft content area)
  // [0] Gestational Age → ui_AutoAirGestVal
  ui_AutoAirGestVal =
      aa_make_input_row(colLeft,
                        (g_lang == LANG_ES)   ? "EDAD GESTACIONAL"
                        : (g_lang == LANG_FR) ? "AGE GESTATIONNEL"
                                              : "GESTATIONAL AGE",
                        "WEEKS", 28, aa_gest_dec_cb, aa_gest_inc_cb, nullptr,
                        &ui_AutoAirRowGest, &ui_AutoAirGestLabel, 50);

  // [1] Post-Natal Age → ui_AutoAirDaysVal
  ui_AutoAirDaysVal = aa_make_input_row(
      colLeft,
      (g_lang == LANG_ES)   ? "EDAD POSTNATAL"
      : (g_lang == LANG_FR) ? "AGE POST-NATAL"
                            : "POST-NATAL AGE",
      "DAYS", 114, aa_days_dec_cb, aa_days_inc_cb, &ui_AutoAirDaysUnitLbl,
      &ui_AutoAirRowDays, &ui_AutoAirDaysLabel, 50);

  // [2] Weight → ui_AutoAirWeightVal
  ui_AutoAirWeightVal = aa_make_input_row(
      colLeft,
      (g_lang == LANG_ES)   ? "PESO"
      : (g_lang == LANG_FR) ? "POIDS"
                            : "WEIGHT",
      "GRAMS", 202, aa_weight_dec_cb, aa_weight_inc_cb, nullptr,
      &ui_AutoAirRowWeight, &ui_AutoAirWeightLabel, 75);

  // Error label
  ui_AutoAirErrLabel = lv_label_create(colLeft);
  lv_label_set_text(ui_AutoAirErrLabel, "");
  lv_obj_set_style_text_color(ui_AutoAirErrLabel, lv_color_hex(0xFF3300), 0);
  lv_obj_set_style_text_font(ui_AutoAirErrLabel, &lv_font_montserrat_12, 0);
  lv_obj_align(ui_AutoAirErrLabel, LV_ALIGN_BOTTOM_MID, 0, -44);
  lv_obj_add_flag(ui_AutoAirErrLabel, LV_OBJ_FLAG_HIDDEN);

  // CANCEL button
  lv_obj_t *btnCancel = lv_btn_create(colLeft);
  lv_obj_set_size(btnCancel, 180, 36);
  lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(btnCancel, lv_color_hex(0x888888), LV_PART_MAIN);
  lv_obj_set_style_radius(btnCancel, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btnCancel, 0, LV_PART_MAIN);
  ui_AutoAirCancelLabel = lv_label_create(btnCancel);
  lv_obj_t *cancelLbl = ui_AutoAirCancelLabel;
  const char *CANCEL_TXT[] = {"CANCELAR", "CANCEL", "ANNULER"};
  lv_label_set_text(cancelLbl, CANCEL_TXT[g_lang]);
  lv_obj_set_style_text_font(cancelLbl, &lv_font_montserrat_16, 0);
  lv_obj_center(cancelLbl);
  lv_obj_add_event_cb(btnCancel, aa_cancel_cb, LV_EVENT_CLICKED, nullptr);

  // ══════════════════════════════════════════════════════════
  // RIGHT COLUMN — Recommended Range
  // ══════════════════════════════════════════════════════════
  lv_obj_t *colRight = lv_obj_create(modal);
  lv_obj_set_size(colRight, 369, 362);
  lv_obj_set_pos(colRight, 331, 58);
  lv_obj_set_style_border_width(colRight, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(colRight, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_pad_all(colRight, 10, LV_PART_MAIN);
  lv_obj_clear_flag(colRight, LV_OBJ_FLAG_SCROLLABLE);

  ui_AutoAirRightHeader = lv_label_create(colRight);
  lv_obj_t *rightHeader = ui_AutoAirRightHeader;
  lv_label_set_text(rightHeader, "RECOMMENDED RANGE (C)");
  lv_obj_set_style_text_font(rightHeader, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(rightHeader, lv_color_hex(0x555555), 0);
  lv_obj_align(rightHeader, LV_ALIGN_TOP_LEFT, 0, 0);

  // Vertical range bar (height > width → LVGL 8 draws it vertical)
  aa_range_bar = lv_bar_create(colRight);
  lv_obj_set_size(aa_range_bar, 40, 320);
  lv_obj_set_pos(aa_range_bar, 70, 22);
  lv_bar_set_mode(aa_range_bar, LV_BAR_MODE_NORMAL);
  lv_bar_set_range(aa_range_bar, 280, 370); // °C × 10 → 28.0–37.0
  lv_bar_set_value(aa_range_bar, 280, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(aa_range_bar, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(aa_range_bar, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_obj_set_style_radius(aa_range_bar, 4, LV_PART_MAIN);
  lv_obj_set_style_border_width(aa_range_bar, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(aa_range_bar, lv_color_hex(0x000000),
                                LV_PART_MAIN);
  lv_obj_add_event_cb(aa_range_bar, aa_bar_drag_cb, LV_EVENT_PRESSING, nullptr);

  // Fixed-size blue setpoint marker
  aa_setpoint_marker = lv_obj_create(colRight);
  lv_obj_set_size(aa_setpoint_marker, 40, 32);
  lv_obj_set_pos(aa_setpoint_marker, 70, 22);
  lv_obj_set_style_bg_color(aa_setpoint_marker, lv_color_hex(0x0095DA),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(aa_setpoint_marker, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(aa_setpoint_marker, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(aa_setpoint_marker, 3, LV_PART_MAIN);
  lv_obj_add_flag(aa_setpoint_marker, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(aa_setpoint_marker, aa_bar_drag_cb, LV_EVENT_PRESSING,
                      nullptr);

  // hi label — top of bar
  aa_label_hi = lv_label_create(colRight);
  lv_label_set_text(aa_label_hi, "--.-");
  lv_obj_set_style_text_font(aa_label_hi, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(aa_label_hi, lv_color_hex(0x333333), 0);
  lv_obj_set_pos(aa_label_hi, 118, 22);

  // mid label — current setpoint (highlighted); y updated dynamically in
  // aa_update_marker_pos
  aa_label_mid = lv_label_create(colRight);
  lv_label_set_text(aa_label_mid, "--.-");
  lv_obj_set_style_text_font(aa_label_mid, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(aa_label_mid, lv_color_hex(0x0075EE), 0);
  lv_obj_set_pos(aa_label_mid, 5, 173);

  // setpoint sub-label — kept alive for guard checks but hidden (duplicate of
  // aa_label_mid)
  aa_setpoint_label = lv_label_create(colRight);
  lv_label_set_text(aa_setpoint_label, "--.-");
  lv_obj_set_style_text_font(aa_setpoint_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(aa_setpoint_label, lv_color_hex(0x555555), 0);
  lv_obj_set_pos(aa_setpoint_label, 5, 197);
  lv_obj_add_flag(aa_setpoint_label, LV_OBJ_FLAG_HIDDEN);

  // lo label — bottom of bar
  aa_label_lo = lv_label_create(colRight);
  lv_label_set_text(aa_label_lo, "--.-");
  lv_obj_set_style_text_font(aa_label_lo, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(aa_label_lo, lv_color_hex(0x333333), 0);
  lv_obj_set_pos(aa_label_lo, 118, 318);

  // Setpoint ▲ button
  lv_obj_t *btnSpUp = lv_btn_create(colRight);
  lv_obj_set_size(btnSpUp, 80, 46);
  lv_obj_set_pos(btnSpUp, 214, 38);
  lv_obj_set_style_bg_color(btnSpUp, lv_color_hex(0x0075EE), LV_PART_MAIN);
  lv_obj_set_style_radius(btnSpUp, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btnSpUp, 0, LV_PART_MAIN);
  lv_obj_t *spUpLbl = lv_label_create(btnSpUp);
  lv_label_set_text(spUpLbl, LV_SYMBOL_UP);
  lv_obj_set_style_text_font(spUpLbl, &lv_font_montserrat_20, 0);
  lv_obj_center(spUpLbl);
  lv_obj_add_event_cb(btnSpUp, aa_setpoint_up_cb, LV_EVENT_CLICKED, nullptr);

  // APPLY button (center-right)
  lv_obj_t *btnApply = lv_btn_create(colRight);
  lv_obj_set_size(btnApply, 140, 46);
  lv_obj_set_pos(btnApply, 184, 136);
  lv_obj_set_style_bg_color(btnApply, lv_color_hex(0x00AA44), LV_PART_MAIN);
  lv_obj_set_style_radius(btnApply, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btnApply, 0, LV_PART_MAIN);
  ui_AutoAirApplyLabel = lv_label_create(btnApply);
  lv_obj_t *applyLbl = ui_AutoAirApplyLabel;
  const char *APPLY_TXT[] = {"APLICAR", "APPLY", "APPLIQUER"};
  lv_label_set_text(applyLbl, APPLY_TXT[g_lang]);
  lv_obj_set_style_text_font(applyLbl, &lv_font_montserrat_16, 0);
  lv_obj_center(applyLbl);
  lv_obj_add_event_cb(btnApply, aa_confirm_cb, LV_EVENT_CLICKED, nullptr);

  // Setpoint ▼ button
  lv_obj_t *btnSpDown = lv_btn_create(colRight);
  lv_obj_set_size(btnSpDown, 80, 46);
  lv_obj_set_pos(btnSpDown, 214, 222);
  lv_obj_set_style_bg_color(btnSpDown, lv_color_hex(0x0075EE), LV_PART_MAIN);
  lv_obj_set_style_radius(btnSpDown, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btnSpDown, 0, LV_PART_MAIN);
  lv_obj_t *spDownLbl = lv_label_create(btnSpDown);
  lv_label_set_text(spDownLbl, LV_SYMBOL_DOWN);
  lv_obj_set_style_text_font(spDownLbl, &lv_font_montserrat_20, 0);
  lv_obj_center(spDownLbl);
  lv_obj_add_event_cb(btnSpDown, aa_setpoint_down_cb, LV_EVENT_CLICKED,
                      nullptr);
}

// ============================================================================
// PHOTO SAFETY POPUP (ISO 7010 M025 — eye protection required)
// ============================================================================
void create_photo_safety_popup() {
  // Full-screen dim overlay (child of ScreenMain)
  ui_PhotoSafetyOverlay = lv_obj_create(ui_ScreenMain);
  lv_obj_set_size(ui_PhotoSafetyOverlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(ui_PhotoSafetyOverlay, 0, 0);
  lv_obj_set_style_bg_color(ui_PhotoSafetyOverlay, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_PhotoSafetyOverlay, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_border_width(ui_PhotoSafetyOverlay, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(ui_PhotoSafetyOverlay, 0, LV_PART_MAIN);
  lv_obj_clear_flag(ui_PhotoSafetyOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ui_PhotoSafetyOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(ui_PhotoSafetyOverlay, photo_safety_overlay_cb, LV_EVENT_CLICKED, nullptr);

  // Modal card (560x260, centered)
  ui_PhotoSafetyModal = lv_obj_create(ui_PhotoSafetyOverlay);
  lv_obj_t *modal = ui_PhotoSafetyModal;
  lv_obj_set_size(modal, 560, 260);
  lv_obj_align(modal, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(modal, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(modal, 3, LV_PART_MAIN);
  lv_obj_set_style_border_color(modal, lv_color_hex(0x0075EE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(modal, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_pad_all(modal, 0, LV_PART_MAIN);
  lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

  // Header: warning symbol + multilingual phototherapy label
  lv_obj_t *headerLbl = lv_label_create(modal);
  lv_label_set_text(headerLbl,
      LV_SYMBOL_WARNING "  FOTOTERAPIA  |  PHOTOTHERAPY  |  PHOTOTHERAPIE");
  lv_obj_set_style_text_font(headerLbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(headerLbl, lv_color_hex(0x0075EE), 0);
  lv_obj_align(headerLbl, LV_ALIGN_TOP_LEFT, 16, 10);

  // Separator 1 (blue)
  lv_obj_t *sep1 = lv_obj_create(modal);
  lv_obj_set_size(sep1, 556, 2);
  lv_obj_set_pos(sep1, 2, 38);
  lv_obj_set_style_bg_color(sep1, lv_color_hex(0x0075EE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sep1, LV_OPA_40, LV_PART_MAIN);
  lv_obj_set_style_border_width(sep1, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(sep1, 0, LV_PART_MAIN);

  // ISO 7010 M025 image (left column)
  lv_obj_t *isoImg = lv_img_create(modal);
  lv_img_set_src(isoImg, &ui_img_iso7010_m025_png);
  lv_obj_set_size(isoImg, 120, 120);
  lv_obj_set_pos(isoImg, 14, 46);

  // Safety title label (right column, language-dependent)
  ui_PhotoSafetyTitleLabel = lv_label_create(modal);
  lv_label_set_text(ui_PhotoSafetyTitleLabel, "EYE PROTECTION REQUIRED");
  lv_obj_set_style_text_font(ui_PhotoSafetyTitleLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(ui_PhotoSafetyTitleLabel, lv_color_hex(0x0055BB), 0);
  lv_obj_set_pos(ui_PhotoSafetyTitleLabel, 148, 48);

  // Safety body label (language-dependent, wrapping)
  ui_PhotoSafetyBodyLabel = lv_label_create(modal);
  lv_label_set_text(ui_PhotoSafetyBodyLabel,
      "Protect the patient's eyes with eye\n"
      "shields before activating phototherapy.");
  lv_obj_set_style_text_font(ui_PhotoSafetyBodyLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ui_PhotoSafetyBodyLabel, lv_color_hex(0x333333), 0);
  lv_obj_set_pos(ui_PhotoSafetyBodyLabel, 148, 74);
  lv_obj_set_width(ui_PhotoSafetyBodyLabel, 400);
  lv_label_set_long_mode(ui_PhotoSafetyBodyLabel, LV_LABEL_LONG_WRAP);

  // ISO 7010 M025 reference (small, gray)
  lv_obj_t *isoRef = lv_label_create(modal);
  lv_label_set_text(isoRef, "ISO 7010 M025");
  lv_obj_set_style_text_font(isoRef, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(isoRef, lv_color_hex(0x999999), 0);
  lv_obj_set_pos(isoRef, 148, 154);

  // Separator 2 (gray)
  lv_obj_t *sep2 = lv_obj_create(modal);
  lv_obj_set_size(sep2, 556, 1);
  lv_obj_set_pos(sep2, 2, 180);
  lv_obj_set_style_bg_color(sep2, lv_color_hex(0xDDDDDD), LV_PART_MAIN);
  lv_obj_set_style_border_width(sep2, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(sep2, 0, LV_PART_MAIN);

  // TURN ON confirmation button
  lv_obj_t *turnOnBtn = lv_btn_create(modal);
  lv_obj_set_size(turnOnBtn, 536, 54);
  lv_obj_set_pos(turnOnBtn, 12, 192);
  lv_obj_set_style_bg_color(turnOnBtn, lv_color_hex(0x4EC7FF), LV_PART_MAIN);
  lv_obj_set_style_radius(turnOnBtn, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(turnOnBtn, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(turnOnBtn, photo_turnon_cb, LV_EVENT_CLICKED, nullptr);

  ui_PhotoSafetyTurnOnLabel = lv_label_create(turnOnBtn);
  lv_label_set_text(ui_PhotoSafetyTurnOnLabel, "TURN ON");
  lv_obj_set_style_text_font(ui_PhotoSafetyTurnOnLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(ui_PhotoSafetyTurnOnLabel, lv_color_hex(0x07111C), 0);
  lv_obj_center(ui_PhotoSafetyTurnOnLabel);
}

// ============================================================================
// MAIN SCREEN TOGGLE BUTTONS (replace lv_switch + ON/OFF labels)
// ============================================================================
void create_main_toggle_buttons() {
  // Hide original switch sliders and flanking ON/OFF labels (kept hidden as
  // state holders — UI_SyncAll reads lv_obj_has_state on them directly)
  lv_obj_add_flag(ui_Switch1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Label9,  LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Label15, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Switch2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Label13, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Label16, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Switch3, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Label10, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_Label17, LV_OBJ_FLAG_HIDDEN);

  // Temperature toggle button (same row as the switch it replaces)
  ui_TempToggleBtn = lv_btn_create(ui_TempCont);
  lv_obj_set_size(ui_TempToggleBtn, 160, 39);
  lv_obj_set_x(ui_TempToggleBtn, 91);
  lv_obj_set_y(ui_TempToggleBtn, -187);
  lv_obj_set_align(ui_TempToggleBtn, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_color(ui_TempToggleBtn, lv_color_hex(0x4EC7FF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_TempToggleBtn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_TempToggleBtn, lv_color_hex(0x4EC7FF), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(ui_TempToggleBtn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_radius(ui_TempToggleBtn, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(ui_TempToggleBtn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(ui_TempToggleBtn, TempToggleBtn_cb, LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *lbl = lv_label_create(ui_TempToggleBtn);
    lv_label_set_text(lbl, "TURN OFF");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);
  }

  // Humidity toggle button
  ui_HumToggleBtn = lv_btn_create(ui_HumCont);
  lv_obj_set_size(ui_HumToggleBtn, 160, 39);
  lv_obj_set_x(ui_HumToggleBtn, 86);
  lv_obj_set_y(ui_HumToggleBtn, -98);
  lv_obj_set_align(ui_HumToggleBtn, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_color(ui_HumToggleBtn, lv_color_hex(0x4EC7FF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_HumToggleBtn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_HumToggleBtn, lv_color_hex(0x4EC7FF), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(ui_HumToggleBtn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_radius(ui_HumToggleBtn, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(ui_HumToggleBtn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(ui_HumToggleBtn, HumToggleBtn_cb, LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *lbl = lv_label_create(ui_HumToggleBtn);
    lv_label_set_text(lbl, "TURN OFF");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);
  }

  // Phototherapy toggle button
  ui_PhotoToggleBtn = lv_btn_create(ui_PhotoCont);
  lv_obj_set_size(ui_PhotoToggleBtn, 160, 39);
  lv_obj_set_x(ui_PhotoToggleBtn, 83);
  lv_obj_set_y(ui_PhotoToggleBtn, 1);
  lv_obj_set_align(ui_PhotoToggleBtn, LV_ALIGN_CENTER);
  lv_obj_set_style_bg_color(ui_PhotoToggleBtn, lv_color_hex(0x4EC7FF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui_PhotoToggleBtn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui_PhotoToggleBtn, lv_color_hex(0x4EC7FF), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(ui_PhotoToggleBtn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_radius(ui_PhotoToggleBtn, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(ui_PhotoToggleBtn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_event_cb(ui_PhotoToggleBtn, PhotoToggleBtn_cb, LV_EVENT_CLICKED, NULL);
  {
    lv_obj_t *lbl = lv_label_create(ui_PhotoToggleBtn);
    lv_label_set_text(lbl, "TURN OFF");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);
  }
}

void create_autoair_button() {
  // AUTO AIR button inside the air panel (bottom area, last child → front
  // Z-order)
  ui_AutoAirBtn = lv_btn_create(ui_TempCont);
  lv_obj_set_size(ui_AutoAirBtn, 130, 60);
  lv_obj_set_x(ui_AutoAirBtn, -108);
  lv_obj_set_y(ui_AutoAirBtn, 26);
  lv_obj_set_align(ui_AutoAirBtn, LV_ALIGN_CENTER);
  lv_obj_set_style_radius(ui_AutoAirBtn, 6, LV_PART_MAIN);
  lv_obj_set_style_border_width(ui_AutoAirBtn, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(ui_AutoAirBtn, lv_color_hex(0x0075EE),
                                LV_PART_MAIN);
  lv_obj_set_style_shadow_width(ui_AutoAirBtn, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_AutoAirBtn, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_AutoAirBtn, LV_OPA_TRANSP, LV_STATE_PRESSED);
  lv_obj_set_style_opa(ui_AutoAirBtn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(ui_AutoAirBtn, 0, LV_PART_MAIN);

  // Image fills the entire button area
  ui_AutoAirBtnLabel = lv_img_create(ui_AutoAirBtn);
  lv_img_set_src(ui_AutoAirBtnLabel, &ui_img_auto_air_png);
  lv_obj_set_size(ui_AutoAirBtnLabel, 130, 60);
  lv_obj_center(ui_AutoAirBtnLabel);

  lv_obj_add_event_cb(ui_AutoAirBtn, AutoAirBtn_cb, LV_EVENT_ALL, nullptr);

  // Dedicated blue toast for AUTO AIR feedback (child of ScreenMain)
  ui_AutoAirToast = lv_label_create(ui_ScreenMain);
  lv_label_set_text(ui_AutoAirToast, "");
  lv_label_set_long_mode(ui_AutoAirToast, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(ui_AutoAirToast, 400);
  lv_obj_align(ui_AutoAirToast, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_set_style_text_align(ui_AutoAirToast, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(ui_AutoAirToast, lv_color_hex(0x004A9E),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui_AutoAirToast, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui_AutoAirToast, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN);
  lv_obj_set_style_pad_all(ui_AutoAirToast, 10, LV_PART_MAIN);
  lv_obj_set_style_radius(ui_AutoAirToast, 8, LV_PART_MAIN);
  lv_obj_add_flag(ui_AutoAirToast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_AutoAirBtn,   LV_OBJ_FLAG_HIDDEN);
}

// ============================================================================
// UI MAIN FUNCTIONS
// ============================================================================

void ui_init(void) {
  lv_disp_t *dispp = lv_disp_get_default();
  lv_theme_t *theme = lv_theme_default_init(
      dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
      false, LV_FONT_DEFAULT);
  lv_disp_set_theme(dispp, theme);

  ui_ScreenMain_screen_init();
  ui_ScreenSettings_screen_init();
  ui_ScreenAlarms_screen_init();
  ui_ScreenCharts_screen_init();
  ui_ScreenPulseOxi_screen_init();
  ui_ScreenLock_screen_init();
  ui_ScreenIntro_screen_init();

  ui____initial_actions0 = lv_obj_create(NULL);
  if (!g_hmiRestoreState) {
    lv_scr_load(ui_ScreenIntro);
  }
}

void ui_destroy(void) {
  // Destruir pantallas si es necesario
  _ui_screen_delete(&ui_ScreenMain);
  _ui_screen_delete(&ui_ScreenSettings);
  _ui_screen_delete(&ui_ScreenAlarms);
  _ui_screen_delete(&ui_ScreenCharts);
  _ui_screen_delete(&ui_ScreenPulseOxi);
  _ui_screen_delete(&ui_ScreenLock);
  _ui_screen_delete(&ui_ScreenIntro);
}

void ui_comp_Temp_button_create_hook(lv_obj_t *comp) {}
