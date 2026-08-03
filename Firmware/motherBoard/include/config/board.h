/*
  MIT License

  Copyright (c) 2022 Medical Open World, Pablo Sánchez Bergasa

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/
#ifndef HW_NUM
#error "HW_NUM must be defined via build_flags in platformio.ini (-DHW_NUM=16, -DHW_NUM=17 or -DHW_NUM=18)"
#endif
// Set to true only on the HMI board
#define IS_HMI false

#define GPIO_EXP_BASE 100 // To differentiate with ESP32 GPIO
// Power / control
#define PWR_EN 2
#define ON_OFF_SWITCH 4
#define BUZZER 1

// GSM
#define GSM_UART_TX_PIN 9
#define GSM_UART_RX_PIN 10
// #define GSM_PWRKEY         // No se ve conectado a ningún IO del uC

// Display / Modbus UART
#define UART_MB_TX_PIN 15
#define UART_MB_RX_PIN 16

// Actuators
#define ACTUATORS_EN 14
#define HEATER 45
#define FAN 12
#define PHOTOTHERAPY 13
#define FAN_CTL 11
#define FAN_SPEED_FEEDBACK 38
#define USB_EN 5
#define USB_FAULT 6

// Sensors
#define BABY_NTC_PIN 8
#define BABY_TEMP_EN 18
#define ADS1110_I2C_ADDRESS 0x48 // I2C ADC for baby NTC

// USB / Second I2C bus (SHTC3 + STS35 for HW16)
#define USB_D_MINUS 19
#define USB_D_PLUS 20
#define I2C2_SCL 19 // repurposed from USB_D_PLUS
#define I2C2_SDA 20 // repurposed from USB_D_MINUS

// I2C (primary: SHT4x + INA3221)
#define I2C_SDA 47
#define I2C_SCL 48

// AFE
#define AFE_MISO 37
#define AFE_MOSI 35
#define AFE_SCK 36
#define AFE_ADC_READY 17
#define AFE44XX_CS 21

#define FAKE_PIN 46

#define SCREENBACKLIGHT FAKE_PIN
#define AFE44XX_PWDN_PIN FAKE_PIN
#define GPRS_PWRKEY FAKE_PIN
#undef TFT_DC
#define TFT_DC FAKE_PIN
#define ENC_SWITCH FAKE_PIN
#define ENC_A FAKE_PIN
#define ENC_B FAKE_PIN
#undef TFT_CS
#define TFT_CS FAKE_PIN

// Selección del puerto de depuración según modo USB
#if ARDUINO_USB_MODE == 1
// Cuando el USB CDC está activo, Serial ya se enruta por USB
#define debugSerial Serial
#else
// Si no se usa USB CDC, utiliza UART0 físico
#define debugSerial Serial
#endif

// number assignment of each environmental sensor for later call in variable
#define SKIN_SENSOR 0
#define ROOM_DIGITAL_TEMP_SENSOR 1
#define AMBIENT_DIGITAL_TEMP_SENSOR 2
#define SENSOR_TEMP_QTY 3 // number of total temperature sensors in system
#define ROOM_DIGITAL_HUM_SENSOR 0
#define AMBIENT_DIGITAL_HUM_SENSOR 1
#define SENSOR_HUM_QTY 2 // number of total humidity sensors in system

#define SYSTEM_SHUNT_CHANNEL INA3221_CH1
#define PHOTOTHERAPY_SHUNT_CHANNEL INA3221_CH2
#define FAN_SHUNT_CHANNEL INA3221_CH3

#define HEATER_SHUNT_CHANNEL INA3221_CH1
#define DISPLAY_SHUNT_CHANNEL INA3221_CH2
#define USB_SHUNT_CHANNEL INA3221_CH2
#define BATTERY_SHUNT_CHANNEL INA3221_CH3

#if (HW_NUM >= 17)
#define HUMIDIFIER_SHUNT 100 // miliohms
#else
#define HUMIDIFIER_SHUNT 1   // flag (boolean legacy, not used as resistance)
#endif

// Cuando es true, el GPIO BABY_TEMP_EN se pone LOW tras cada lectura para
// reducir el autocalentamiento de la NTC (excitación pulsada).
// Cuando es false, BABY_TEMP_EN permanece HIGH entre medidas.
#define SKIN_NTC_PULSED_EXCITATION false

#if (HW_NUM == 17)
// PCB layout bug: INA3221 IN+ taps the MOSFET switching node instead of the
// shunt pad. During PWM switching the reading flips negative with amplified
// magnitude. Empirical correction: I_real ≈ |I_measured| / factor.
// Tune HEATER_CURRENT_CORRECTION_FACTOR based on measured vs. expected current.
#define HEATER_CURRENT_CORRECTION_FACTOR 5.80f
#define SYSTEM_SHUNT 1000        // miliohms (VSYS_SHUNT+ is not connected properly, is connected before O-ring)
#define HEATER_SHUNT 5        // miliohms
#define FAN_SHUNT 100         // miliohms
#define PHOTOTHERAPY_SHUNT 5   // miliohms
#define USB_SHUNT 100         // miliohms (humidifier via USB_EN channel)
#define BATTERY_SHUNT 27000   // miliohms
#elif (HW_NUM == 18)
// HW18: VSYS_SHUNT+/heater shunt wiring fixed, no more MOSFET-switching-node
// tap - HEATER_CURRENT_CORRECTION_FACTOR is a HW17-only workaround, not
// needed here (see HW_NUM == 17 above and sensors_module.cpp's
// #if (HW_NUM == 17) around heater_current).
#define SYSTEM_SHUNT 5        // miliohms
#define HEATER_SHUNT 2        // miliohms
#define FAN_SHUNT 100         // miliohms
#define PHOTOTHERAPY_SHUNT 5   // miliohms
#define USB_SHUNT 100         // miliohms (humidifier via USB_EN channel)
#define BATTERY_SHUNT 27000   // miliohms
#elif (HW_NUM >= 16)
#define SYSTEM_SHUNT 3        // miliohms
#define FAN_SHUNT 3           // miliohms
#define PHOTOTHERAPY_SHUNT 15 // miliohms
#define BATTERY_SHUNT 27000   // miliohms
#define USB_SHUNT 3           // miliohms
#define HEATER_SHUNT 3        // miliohms
#endif

#define DISPLAY_DEFAULT_ROTATION 3

#define SCREENBACKLIGHT_PWM_CHANNEL 0
#define BUZZER_PWM_CHANNEL 1
#define HEATER_PWM_CHANNEL 2
// FAN y FAN_CTL comparten timer 3 (ch 6,7) a 25 kHz; HEATER (ch 2, timer 1)
// queda aislado para que HEATER_PWM_FREQUENCY (400 Hz) no sea sobrescrito.
#define FAN_PWM_CHANNEL 7
#define PHOTOTHERAPY_PWM_CHANNEL 4
#define HUMIDIFIER_PWM_CHANNEL 5
#define FAN_CTL_PWM_CHANNEL 6
#define DEFAULT_PWM_RESOLUTION 8
#define DEFAULT_PWM_FREQUENCY 400
#define BUZZER_PWM_FREQUENCY DEFAULT_PWM_FREQUENCY
#define PHOTOTHERAPY_PWM_FREQUENCY 10000
#define HEATER_PWM_FREQUENCY DEFAULT_PWM_FREQUENCY
#define FAN_PWM_FREQUENCY DEFAULT_PWM_FREQUENCY
#define HUMIDIFIER_PWM_FREQUENCY 109000

#define maxADCvalue 4095
// #define PWM_MAX_VALUE maxADCvalue
#define PWM_MAX_VALUE (pow(2, DEFAULT_PWM_RESOLUTION) - 1)
#define FAN_PWR_SUPPLY_PWM PWM_MAX_VALUE
#define FAN_CTL_PWM_DEFAULT 130
#define FAN_MIN_RPM 3000            // minimum acceptable fan RPM when speed feedback is present
#define FAN_MIN_RPM_HYSTERESIS 300  // rpm above FAN_MIN_RPM required to clear FAN_ISSUE_ALARM

// Closed-loop fan speed control (HW>=16, feedback-capable units only).
#define FAN_TARGET_RPM 4000
// Default for in3.fanPidEnabled (runtime-toggleable via /config and USB). When
// false, the fan runs at the fixed in3.fanCtlPWM duty with the PID bypassed —
// same as a unit without RPM feedback.
#define FAN_PID_ENABLED_DEFAULT true
// The fan is held open-loop at its baseline duty for this long after being
// commanded on; only then does the PID close the loop (bumpless). Running
// the loop during the ~3s mechanical spin-up made it chase the lagged,
// still-ramping RPM measurement and wind the duty far past baseline — a
// ~6000rpm overshoot on a 4000 target. Also used by the RPM/air-blockage
// monitors in security.cpp as their spin-up grace.
#define FAN_SPINUP_GRACE_MS 6000
// Factory baseline duty (0-255) to hold FAN_TARGET_RPM with a clean air
// outlet is 137. This threshold (margin above baseline) must be confirmed
// on the bench against real unit-to-unit variance AND worst-case legitimate
// load: with the heater at max power the supply voltage sags and the PID
// legitimately raises the duty to keep FAN_TARGET_RPM — the threshold must
// sit above that sagged-supply duty, not above the idle-supply baseline.
#define FAN_DUTY_BLOCKED_THRESHOLD 160
#define FAN_DUTY_BLOCKED_HYSTERESIS 15 // duty below (threshold - this) required to clear AIR_BLOCKED_ALARM
// Master enable for air-outlet-blockage detection (boot check + runtime
// monitor). Disabled until FAN_DUTY_BLOCKED_THRESHOLD is bench-validated as
// described above — with an unvalidated threshold the alarm false-fires
// during normal heater operation. The duty needed to hold FAN_TARGET_RPM is
// still logged at boot regardless, to collect the calibration data.
#define AIR_BLOCKED_DETECTION_ENABLED false
// Duty must stay above the threshold continuously this long before alarming
// (rejects transients: spin-up saturation, heater kick-in sag compensation).
#define AIR_BLOCKED_SUSTAIN_MS 5000

#if (ADC_READ_FUNCTION == MILLIVOTSREAD_ADC)
#define ADC_TO_DISCARD_MIN 500  // in mV
#define ADC_TO_DISCARD_MAX 2500 // in mV
#else
#define ADC_TO_DISCARD_MIN maxADCvalue / 5     // in ADC points
#define ADC_TO_DISCARD_MAX maxADCvalue * 4 / 5 // in ADC points
#endif

#define DIG_TEMP_TO_DISCARD_MAX 60
#define DIG_TEMP_TO_DISCARD_MIN 5

// Master enable for heaterPowerConsumptionCheck() (PID.cpp): the current-based
// ramp that throttles heaterSafeMAXPWM down when in3.heater_current/system_current
// exceed in3.heaterMaxPowerAmps. When false, that check is skipped and
// heaterSafeMAXPWM is pinned at HEATER_MAX_PWM instead - i.e. the heater is no
// longer power-limited by the current sensor.
#define HEATER_CURRENT_LIMIT_ENABLED true

#define HEATER_MAX_PWM PWM_MAX_VALUE
#define HEATER_HALF_PWR PWM_MAX_VALUE / 2
#define HEATER_START_PWM 1

#define BUZZER_MAX_PWM PWM_MAX_VALUE
#define BUZZER_HALF_PWM PWM_MAX_VALUE / 2

#define MIN_SYSTEM_VOLTAGE_TRIGGER 0
#define MAX_SYSTEM_VOLTAGE_TRIGGER 8

#define SCREEN_BRIGHTNESS_FACTOR                                               \
  0.7 // Max brightness will be multiplied by this constant

#define BACKLIGHT_POWER_DEFAULT PWM_MAX_VALUE *SCREEN_BRIGHTNESS_FACTOR
