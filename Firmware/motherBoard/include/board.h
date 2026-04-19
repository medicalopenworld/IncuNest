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
#define HW_NUM 16
#define HW_REVISION 'A'
#define HWversion String(HW_NUM) + "." + String(HW_REVISION)
#define FWversion "16.4"
#define WIFI_NAME "IncuNest"
#define CURRENT_FIRMWARE_TITLE "IncuNest"
// Set to true only on the HMI board
#define IS_HMI false

#if (HW_NUM <= 8)
#define DISPLAY_SPI_CLK SPI_CLOCK_DIV16
#elif (HW_NUM == 9)
#define DISPLAY_SPI_CLK SPI_CLOCK_DIV16
#elif (HW_NUM >= 10)
#define DISPLAY_SPI_CLK SPI_CLOCK_DIV16
#endif

#if (HW_NUM <= 8)
#define ANALOG_TO_AMP_FACTOR 0.2
#define CURRENT_MEASURES_AMOUNT 20
#endif
#if (HW_NUM <= 6)
#define HUMIDIFIER_INTERFACE HUMIDIFIER_BINARY
#elif (HW_NUM <= 8)
#define HUMIDIFIER_INTERFACE HUMIDIFIER_PWM
#elif (HW_NUM == 16)
#define HUMIDIFIER_INTERFACE HUMIDIFIER_BINARY // USB_EN GPIO ON/OFF
#else
#define HUMIDIFIER_INTERFACE HUMIDIFIER_I2C
#endif

#define GPIO_EXP_BASE 100 // To differentiate with ESP32 GPIO
#if (HW_NUM == 16)
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
#define AFE_LED_ALM 7

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
#elif (HW_NUM == 15)
// PINOUT
#define BUZZER 5 // Set FAKE_PIN to mute buzzer
#define LED 7
#define I2C_SDA 8
#define I2C_SCL 9
#define BABY_NTC_PIN 10
#define FAN 12
#define PHOTOTHERAPY 13
#define ACTUATORS_EN 14
#define HEATER 16
#define AFE44XX_CS 21
#define FAN_SPEED_FEEDBACK 38
#define AFE_ADC_READY 45
#define GSM_UART_TX_PIN 48
#define GSM_UART_RX_PIN 47
// AFE SPI bus (shared with TFT SPI, pins from build flags TFT_SCLK/MISO/MOSI)
#define AFE_SCK 36
#define AFE_MISO 37
#define AFE_MOSI 35

#define FAKE_PIN 46

#define SCREENBACKLIGHT FAKE_PIN
#define ON_OFF_SWITCH FAKE_PIN
#define AFE44XX_PWDN_PIN FAKE_PIN
#define GPRS_PWRKEY FAKE_PIN
#undef TFT_DC
#define TFT_DC FAKE_PIN
#define ENC_SWITCH FAKE_PIN
#define ENC_A FAKE_PIN
#define ENC_B FAKE_PIN
#undef TFT_CS
#define TFT_CS FAKE_PIN

#elif (HW_NUM == 14)
// PINOUT
#define TFT_DC 0
#define AFE44XX_CS 2
#define ENC_SWITCH 4
#define BUZZER 5
#define FAN 12
#define PHOTOTHERAPY 13
#define ACTUATORS_EN 14
#define TFT_CS 15
#define I2C_SDA 21
#define I2C_SCL 22
#define ENC_A 25
#define HEATER 27
#define SCREENBACKLIGHT 33
#define ENC_B 34
#define FAN_SPEED_FEEDBACK 35
#define BABY_NTC_PIN 39
#define GSM_UART_TX_PIN 16
#define GSM_UART_RX_PIN 17
#define AFE4490_ADC_READY NULL
#define ON_OFF_SWITCH NULL
#define AFE44XX_PWDN_PIN NULL
#define GPRS_PWRKEY NULL

#elif (HW_NUM >= 13)
// PINOUT
// #define TFT_DC 0
#define AFE44XX_CS 2
#define ENC_SWITCH 4
#define BUZZER 5
#define FAN 12
#define PHOTOTHERAPY 13
#define ACTUATORS_EN 14
#define GSM_UART_TX_PIN 16
#define GSM_UART_RX_PIN 17
// #define TFT_CS 15
#define I2C_SDA 21
#define I2C_SCL 22
#define ENC_A 25
#define GPRS_PWRKEY 26
#define HEATER 27
#define SCREENBACKLIGHT 33
#define ENC_B 32
#define FAN_SPEED_FEEDBACK 35
#define BABY_NTC_PIN 39
#define AFE4490_ADC_READY NULL
#define ON_OFF_SWITCH NULL
#define AFE44XX_PWDN_PIN NULL

// #define DISPLAY_CONTROLLER_IC ST7789V_CONTROLLER

#elif (HW_NUM >= 9)
// PINOUT
#define TFT_DC 0
#define AFE44XX_CS 2
#define ENC_SWITCH 4
#define BUZZER 5
#define FAN 12
#define PHOTOTHERAPY 13
#define ACTUATORS_EN 14
#define TFT_CS 15
#define GSM_UART_TX_PIN 16
#define GSM_UART_RX_PIN 17
#define I2C_SDA 21
#define I2C_SCL 22
#define ENC_A 25
#define GPRS_PWRKEY 26
#define HEATER 27
#define ENC_B 32
#define SCREENBACKLIGHT 33
#define ON_OFF_SWITCH 34
#define FAN_SPEED_FEEDBACK 35
#define AFE4490_ADC_READY 36
#define BABY_NTC_PIN 39
#define AFE44XX_PWDN_PIN NULL

// #define DISPLAY_CONTROLLER_IC ST7789V_CONTROLLER
#elif (HW_NUM == 8)
#define TFT_DC 0
#define ENC_SWITCH 4
#define BUZZER 5
#define FAN 12
#define PHOTOTHERAPY 13
#define HUMIDIFIER_CTL 14
#define TFT_CS 15
#define GSM_UART_TX_PIN 16
#define GSM_UART_RX_PIN 17
#define I2C_SDA 21
#define I2C_SCL 22
#define ENC_A 25
#define GPRS_PWRKEY 26
#define HEATER 27
#define ENC_B 32
#define SCREENBACKLIGHT 33
#define ACTUATORS_EN 34 // fake pin
#define SYSTEM_CURRENT_SENSOR 36
#define BABY_NTC_PIN 39

#define DISPLAY_CONTROLLER_IC ST7789V_CONTROLLER

#else
// Hardware
// PINOUT

#define AFE44XX_CS 2
#define TFT_DC 4
#define BUZZER 5
#define TFT_CS 15 // fake GPIO
#define GSM_UART_TX_PIN 16
#define GSM_UART_RX_PIN 17
#define I2C_SDA 21
#define I2C_SCL 22
#define ENC_A 25
#define ENC_SWITCH 26
#define HEATER 27
#define ENC_B 32
#define SCREENBACKLIGHT 33
#define BABY_NTC_PIN 34
#define AIR_NTC_PIN 35
#define SYSTEM_CURRENT_SENSOR 36
// #define SYSTEM_SHUNT 36

#define GPIO_EXP_0 0 + GPIO_EXP_BASE
#define GPIO_EXP_1 1 + GPIO_EXP_BASE
#define GPIO_EXP_2 2 + GPIO_EXP_BASE
#define GPIO_EXP_3 3 + GPIO_EXP_BASE
#define GPIO_EXP_4 4 + GPIO_EXP_BASE
#define GPIO_EXP_5 5 + GPIO_EXP_BASE
#define GPIO_EXP_6 6 + GPIO_EXP_BASE
#define GPIO_EXP_7 7 + GPIO_EXP_BASE
#define GPIO_EXP_8 8 + GPIO_EXP_BASE
#define GPIO_EXP_9 9 + GPIO_EXP_BASE
#define GPIO_EXP_10 10 + GPIO_EXP_BASE
#define GPIO_EXP_11 11 + GPIO_EXP_BASE
#define GPIO_EXP_12 12 + GPIO_EXP_BASE
#define GPIO_EXP_13 13 + GPIO_EXP_BASE
#define GPIO_EXP_14 14 + GPIO_EXP_BASE
#define GPIO_EXP_15 15 + GPIO_EXP_BASE

#define UNUSED_GPIO_EXP0 GPIO_EXP_5
#define UNUSED_GPIO_EXP1 GPIO_EXP_12
#define UNUSED_GPIO_EXP2 GPIO_EXP_14
#define UNUSED_GPIO_EXP3 GPIO_EXP_15

#define ACTUATORS_EN GPIO_EXP_0
#define GPRS_EN GPIO_EXP_1
#define SD_CS GPIO_EXP_2
#define FAN GPIO_EXP_3
#define HUMIDIFIER_CTL GPIO_EXP_6
#define PHOTOTHERAPY GPIO_EXP_7
#define GPRS_PWRKEY GPIO_EXP_8
#define TFT_CS_EXP GPIO_EXP_9
#define TOUCH_IRQ GPIO_EXP_10
#define TOUCH_CS GPIO_EXP_11
#define TFT_RST GPIO_EXP_13

#define DISPLAY_CONTROLLER_IC ILI9341_CONTROLLER

#endif

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
#define NTC_QTY 1 // number of NTC
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

#define HUMIDIFIER_SHUNT 1

// Cuando es true, el GPIO BABY_TEMP_EN se pone LOW tras cada lectura para
// reducir el autocalentamiento de la NTC (excitación pulsada).
// Cuando es false, BABY_TEMP_EN permanece HIGH entre medidas.
#define SKIN_NTC_PULSED_EXCITATION false

#define SDCard false
#define SYSTEM_SHUNT 3        // miliohms
#define FAN_SHUNT 3           // miliohms
#define PHOTOTHERAPY_SHUNT 15 // miliohms
#define BATTERY_SHUNT 27000   // miliohms
#define USB_SHUNT 3           // miliohms
#define HEATER_SHUNT 3        // miliohms

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
#define HEATER_PWM_FREQUENCY DEFAULT_PWM_FREQUENCY
#define FAN_PWM_FREQUENCY DEFAULT_PWM_FREQUENCY
#define HUMIDIFIER_PWM_FREQUENCY 109000

#define maxADCvalue 4095
#define maxDACvalue 4095
// #define PWM_MAX_VALUE maxADCvalue
#define PWM_MAX_VALUE (pow(2, DEFAULT_PWM_RESOLUTION) - 1)
#define FAN_PWM PWM_MAX_VALUE
#define FAN_CTL_PWM_DEFAULT 120

#if (ADC_READ_FUNCTION == MILLIVOTSREAD_ADC)
#define ADC_TO_DISCARD_MIN 500  // in mV
#define ADC_TO_DISCARD_MAX 2500 // in mV
#else
#define ADC_TO_DISCARD_MIN maxADCvalue / 5     // in ADC points
#define ADC_TO_DISCARD_MAX maxADCvalue * 4 / 5 // in ADC points
#endif

#define DIG_TEMP_TO_DISCARD_MAX 60
#define DIG_TEMP_TO_DISCARD_MIN 5

#define BL_NORMAL 0
#define BL_POWERSAVE 1

#define HEATER_MAX_PWM PWM_MAX_VALUE
#define HEATER_HALF_PWR PWM_MAX_VALUE / 2
#define HEATER_START_PWM 5

#define BUZZER_MAX_PWM PWM_MAX_VALUE
#define BUZZER_HALF_PWM PWM_MAX_VALUE / 2

#define DIRECT_BACKLIGHT_CONTROL true
#define INVERTED_BACKLIGHT_CONTROL false

#define MIN_SYSTEM_VOLTAGE_TRIGGER 0
#define MAX_SYSTEM_VOLTAGE_TRIGGER 8

#if (HW_NUM <= 8 || (HW_NUM == 9 && HW_REVISION == 'A'))
#define SCREEN_BRIGHTNESS_FACTOR                                               \
  0.1 // Max brightness will be multiplied by this constant
#define BACKLIGHT_POWER_SAFE_PERCENTAGE 0.6
#define BACKLIGHT_CONTROL INVERTED_BACKLIGHT_CONTROL
#else
#define SCREEN_BRIGHTNESS_FACTOR                                               \
  0.7 // Max brightness will be multiplied by this constant
#define BACKLIGHT_POWER_SAFE_PERCENTAGE 0.3
#define BACKLIGHT_CONTROL DIRECT_BACKLIGHT_CONTROL
#endif

#define BACKLIGHT_POWER_SAFE PWM_MAX_VALUE *BACKLIGHT_POWER_SAFE_PERCENTAGE
#define BACKLIGHT_POWER_DEFAULT PWM_MAX_VALUE *SCREEN_BRIGHTNESS_FACTOR
