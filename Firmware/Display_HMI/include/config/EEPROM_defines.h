#pragma once
#include <Preferences.h>

// --------------- Non-EEPROM constants kept from original header ---------------
#define INACTIVITY_TIMEOUT_MS 20000 // 20s of inactivity before auto-lock
#define EEPROM_COMMIT_DELAY 5000    // 5s debounce — NVS writes happen outside LVGL lock

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
constexpr char HMI_KEY_SKIN_EN[]     = "skin_en";
constexpr char HMI_KEY_VOLUME[]      = "volume";
constexpr char HMI_KEY_DISP_FREQ[]   = "disp_freq";
// Recordatorio de mantenimiento (modules/maintenance). Las fechas son epoch
// UTC, nunca hora local: la zona puede cambiar y los plazos no deben moverse
// con ella.
constexpr char HMI_KEY_MNT_DAILY[]    = "mnt_daily";   // epoch ultima diaria
constexpr char HMI_KEY_MNT_WEEKLY[]   = "mnt_weekly";  // epoch ultima semanal
constexpr char HMI_KEY_MNT_TERMINAL[] = "mnt_term";    // epoch ultima terminal
constexpr char HMI_KEY_MNT_EN[]       = "mnt_en";      // avisos on/off
constexpr char HMI_KEY_MNT_SNOOZE[]   = "mnt_snooze";  // epoch fin del "mas tarde"
constexpr char HMI_KEY_MNT_SEQ[]      = "mnt_seq";     // ultimo bebe visto al mando
constexpr char HMI_KEY_MNT_TPEND[]    = "mnt_tpend";   // alta sin terminal hecha

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
