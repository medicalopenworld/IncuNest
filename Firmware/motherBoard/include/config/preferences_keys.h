#pragma once
// NVS (Preferences) namespace and key names

// --------------- Preferences namespaces ---------------
constexpr char NS_CFG[]   = "mb_cfg";
constexpr char NS_CAL[]   = "mb_cal";
constexpr char NS_WIFI[]  = "mb_wifi";
constexpr char NS_GPRS[]  = "mb_gprs";
constexpr char NS_RT[]    = "mb_rt";
constexpr char NS_STATE[] = "mb_state";

// --------------- Key names: mb_cfg ---------------
constexpr char KEY_LANG[]        = "lang";
constexpr char KEY_SERIAL[]      = "serial";
constexpr char KEY_CTRL_MODE[]   = "ctrl_mode";
constexpr char KEY_CTRL_TEMP[]   = "ctrl_temp";
constexpr char KEY_CTRL_HUM[]    = "ctrl_hum";
constexpr char KEY_FAN_PWR_SUPPLY_PWM[] = "fan_pwr_sup_pwm";
constexpr char KEY_HEAT_MAX_A[]  = "heat_max_A";
constexpr char KEY_SKIN_T_MAX[]  = "skin_t_max";
constexpr char KEY_AIR_T_MAX[]   = "air_t_max";
constexpr char KEY_HEATER_TEST[] = "heater_test";
constexpr char KEY_FAN_CTL_PWM[] = "fan_ctl_pwm";
constexpr char KEY_FAN_RPM_FEEDBACK[] = "fan_rpm_fb";

// --------------- Key names: mb_cal ---------------
constexpr char KEY_CAL_SK_LOW[]  = "cal_sk_low";
constexpr char KEY_CAL_SK_RNG[]  = "cal_sk_rng";
constexpr char KEY_CAL_REF_RNG[] = "cal_ref_rng";
constexpr char KEY_CAL_REF_LOW[] = "cal_ref_low";
constexpr char KEY_FT_SKIN[]     = "ft_skin";
constexpr char KEY_FT_AIR[]      = "ft_air";

// --------------- Key names: mb_wifi ---------------
constexpr char KEY_SSID[]     = "ssid";
constexpr char KEY_PASSWORD[] = "password";

// --------------- Key names: mb_gprs ---------------
constexpr char KEY_PROVISIONED[]  = "provisioned";
constexpr char KEY_TOKEN[]        = "token";
constexpr char KEY_ACT_PERIOD[]   = "act_period";
constexpr char KEY_PHOTO_PERIOD[] = "photo_period";
constexpr char KEY_STBY_PERIOD[]  = "stby_period";

// --------------- Key names: mb_rt ---------------
constexpr char KEY_RT_STANDBY[]  = "standby";
constexpr char KEY_RT_CTRL[]     = "ctrl_time";
constexpr char KEY_RT_HEATER[]   = "heater_t";
constexpr char KEY_RT_FAN[]      = "fan_t";
constexpr char KEY_RT_PHOTO[]    = "photo_t";
constexpr char KEY_RT_HUM[]      = "hum_t";

// --------------- Key names: mb_state ---------------
constexpr char KEY_PHOTO_ACTIVE[] = "photo_active";
constexpr char KEY_ACTUATION[]    = "actuation";
