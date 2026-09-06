#pragma once
#include <Preferences.h>

// --------------- Non-EEPROM constants kept from original header ---------------
#define INACTIVITY_TIMEOUT_MS 20000 // 20s of inactivity before auto-lock
#define EEPROM_COMMIT_DELAY 5000    // 5s debounce — NVS writes happen outside LVGL lock

// --------------- Namespaces ---------------
constexpr char HMI_NS_CFG[]  = "hmi_cfg";
constexpr char HMI_NS_WIFI[] = "hmi_wifi";
constexpr char HMI_NS_GPRS[] = "hmi_gprs";
// Cursos de formacion (hmi-training-courses): progreso por curso y anillo de
// certificados. Claves por curso "c<N>_name" / "c<N>_done" / "c<N>_att" y
// "cert_<slot>" se componen en training_progress.cpp.
constexpr char HMI_NS_TRAIN[] = "hmi_train";

// --------------- Keys: hmi_train ---------------
constexpr char HMI_KEY_TRAIN_CERT_CNT[]  = "cert_cnt";
constexpr char HMI_KEY_TRAIN_CERT_NEXT[] = "cert_next";

// --------------- Keys: hmi_cfg ---------------
constexpr char HMI_KEY_LANG[]        = "lang";
constexpr char HMI_KEY_SERIAL[]      = "serial";
constexpr char HMI_KEY_AIR_TEMP[]    = "air_temp";
constexpr char HMI_KEY_SKIN_TEMP[]   = "skin_temp";
constexpr char HMI_KEY_HUMIDITY[]    = "humidity";
constexpr char HMI_KEY_PHOTO_MIN[]   = "photo_min";
constexpr char HMI_KEY_DARK_MODE[]   = "dark_mode";
constexpr char HMI_KEY_HUM_EN[]      = "hum_en";
constexpr char HMI_KEY_SKIN_EN[]     = "skin_en";
constexpr char HMI_KEY_VOLUME[]      = "volume";
constexpr char HMI_KEY_DISP_FREQ[]   = "disp_freq";

// --------------- Keys: hmi_wifi ---------------
constexpr char HMI_KEY_SSID[]     = "ssid";
constexpr char HMI_KEY_PASSWORD[] = "password";

// --------------- Keys: hmi_gprs ---------------
constexpr char HMI_KEY_PROVISIONED[] = "provisioned";
constexpr char HMI_KEY_TOKEN[]       = "token";

void initEEPROM();
void loaddefaultValues();
void recapVariables();
void resetFlash();
