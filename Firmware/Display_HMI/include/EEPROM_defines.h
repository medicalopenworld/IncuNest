#pragma once
#include <Preferences.h>

// --------------- Non-EEPROM constants kept from original header ---------------
#define INACTIVITY_TIMEOUT_MS 20000 // 20s of inactivity before auto-lock
#define EEPROM_COMMIT_DELAY 0       // write immediately after change

// --------------- Namespaces ---------------
constexpr char HMI_NS_CFG[]  = "hmi_cfg";
constexpr char HMI_NS_WIFI[] = "hmi_wifi";
constexpr char HMI_NS_GPRS[] = "hmi_gprs";

// --------------- Keys: hmi_cfg ---------------
constexpr char HMI_KEY_LANG[]        = "lang";
constexpr char HMI_KEY_SERIAL[]      = "serial";
constexpr char HMI_KEY_AIR_TEMP[]    = "air_temp";
constexpr char HMI_KEY_SKIN_TEMP[]   = "skin_temp";
constexpr char HMI_KEY_HUMIDITY[]    = "humidity";
constexpr char HMI_KEY_PHOTO_MIN[]   = "photo_min";
constexpr char HMI_KEY_DARK_MODE[]   = "dark_mode";
constexpr char HMI_KEY_HUM_EN[]      = "hum_en";
constexpr char HMI_KEY_VOLUME[]      = "volume";
constexpr char HMI_KEY_DISP_FREQ[]   = "disp_freq";
constexpr char HMI_KEY_AA_WEIGHT[]   = "aa_weight";
constexpr char HMI_KEY_AA_GEST[]     = "aa_gest";
constexpr char HMI_KEY_AA_AGE_H[]    = "aa_age_h";

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
