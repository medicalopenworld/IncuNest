#pragma once
#include <Preferences.h>

// --------------- Non-EEPROM constants kept from original header ---------------
#define INACTIVITY_TIMEOUT_MS 20000 // 20s of inactivity before auto-lock
#define EEPROM_COMMIT_DELAY 5000    // 5s debounce — NVS writes happen outside LVGL lock

// --------------- Namespaces ---------------
constexpr char HMI_NS_CFG[]   = "hmi_cfg";
constexpr char HMI_NS_WIFI[]  = "hmi_wifi";
constexpr char HMI_NS_GPRS[]  = "hmi_gprs";
constexpr char HMI_NS_FTEST[] = "hmi_ftest";

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

// --------------- Keys: hmi_ftest (test de fabrica, shared-factory-test) ---
// "probe" es tambien el propio test HMI_NVS: escribe y relee este mismo par
// namespace/clave para comprobar que la NVS del display funciona.
constexpr char HMI_KEY_FTEST_PROBE[]    = "probe";
constexpr char HMI_KEY_FTEST_EPOCH[]    = "epoch";
constexpr char HMI_KEY_FTEST_PASSMASK[] = "pass_mask";
constexpr char HMI_KEY_FTEST_FAILMASK[] = "fail_mask";
constexpr char HMI_KEY_FTEST_MBPASS[]   = "mb_pass";
constexpr char HMI_KEY_FTEST_MBFAIL[]   = "mb_fail";
constexpr char HMI_KEY_FTEST_MBSKIP[]   = "mb_skip";
constexpr char HMI_KEY_FTEST_FWVER[]    = "fwver";

void initEEPROM();
void loaddefaultValues();
void recapVariables();
void resetFlash();
