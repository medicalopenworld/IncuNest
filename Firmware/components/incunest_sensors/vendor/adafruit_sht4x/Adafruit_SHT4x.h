/*!
 *  @file Adafruit_SHT4x.h
 *
 *  This is a library for the SHT4x Digital Humidity & Temp Sensor
 *
 *  Designed specifically to work with the SHT4x Humidity & Temp Sensor
 *  -----> https://www.adafruit.com/product/4885
 *
 *  These sensors use I2C to communicate, 2 pins are required to interface
 *
 *  Adafruit invests time and resources providing this open source code,
 *  please support Adafruit andopen-source hardware by purchasing products
 *  from Adafruit!
 *
 *  Limor Fried/Ladyada (Adafruit Industries).
 *
 *  BSD license, all text above must be included in any redistribution
 */

#ifndef ADAFRUIT_SHT4x_H
#define ADAFRUIT_SHT4x_H

// PARCHE INCUNEST: Arduino -> capa de plataforma propia
#include "platform/plat_gpio.h"
#include "platform/plat_num.h"
#include "platform/plat_string.h"
#include "platform/plat_time.h"
#include "platform/plat_types.h"
#include <Adafruit_I2CDevice.h>
#include <Adafruit_Sensor.h>

#define SHT4x_DEFAULT_ADDR 0x44 /**< SHT4x I2C Address */
#define SHT4x_NOHEAT_HIGHPRECISION                                             \
  0xFD /**< High precision measurement, no heater */
#define SHT4x_NOHEAT_MEDPRECISION                                              \
  0xF6 /**< Medium precision measurement, no heater */
#define SHT4x_NOHEAT_LOWPRECISION                                              \
  0xE0 /**< Low precision measurement, no heater */

#define SHT4x_HIGHHEAT_1S                                                      \
  0x39 /**< High precision measurement, high heat for 1 sec */
#define SHT4x_HIGHHEAT_100MS                                                   \
  0x32 /**< High precision measurement, high heat for 0.1 sec */
#define SHT4x_MEDHEAT_1S                                                       \
  0x2F /**< High precision measurement, med heat for 1 sec */
#define SHT4x_MEDHEAT_100MS                                                    \
  0x24 /**< High precision measurement, med heat for 0.1 sec */
#define SHT4x_LOWHEAT_1S                                                       \
  0x1E /**< High precision measurement, low heat for 1 sec */
#define SHT4x_LOWHEAT_100MS                                                    \
  0x15 /**< High precision measurement, low heat for 0.1 sec */

#define SHT4x_READSERIAL 0x89 /**< Read Out of Serial Register */
#define SHT4x_SOFTRESET 0x94  /**< Soft Reset */

/** How precise (repeatable) the measurement will be */
typedef enum {
  SHT4X_HIGH_PRECISION,
  SHT4X_MED_PRECISION,
  SHT4X_LOW_PRECISION,
} sht4x_precision_t;

/** Optional pre-heater configuration setting */
typedef enum {
  SHT4X_NO_HEATER,
  SHT4X_HIGH_HEATER_1S,
  SHT4X_HIGH_HEATER_100MS,
  SHT4X_MED_HEATER_1S,
  SHT4X_MED_HEATER_100MS,
  SHT4X_LOW_HEATER_1S,
  SHT4X_LOW_HEATER_100MS,
} sht4x_heater_t;

class Adafruit_SHT4x;

/**
 * @brief  Adafruit Unified Sensor interface for the humidity sensor component
 * of SHT4x
 *
 */
class Adafruit_SHT4x_Humidity : public Adafruit_Sensor {
public:
  /** @brief Create an Adafruit_Sensor compatible object for the humidity sensor
    @param parent A pointer to the SHT4x class */
  Adafruit_SHT4x_Humidity(Adafruit_SHT4x *parent) { _theSHT4x = parent; }
  bool getEvent(sensors_event_t *);
  void getSensor(sensor_t *);

private:
  int _sensorID = 0x0401;
  Adafruit_SHT4x *_theSHT4x = NULL;
};

/**
 * @brief Adafruit Unified Sensor interface for the temperature sensor component
 * of SHT4x
 *
 */
class Adafruit_SHT4x_Temp : public Adafruit_Sensor {
public:
  /** @brief Create an Adafruit_Sensor compatible object for the temp sensor
      @param parent A pointer to the SHT4x class */
  Adafruit_SHT4x_Temp(Adafruit_SHT4x *parent) { _theSHT4x = parent; }

  bool getEvent(sensors_event_t *);
  void getSensor(sensor_t *);

private:
  int _sensorID = 0x0400;
  Adafruit_SHT4x *_theSHT4x = NULL;
};

/**
 * Driver for the Adafruit SHT4x Temperature and Humidity breakout board.
 */
class Adafruit_SHT4x {
public:
  Adafruit_SHT4x(void);
  ~Adafruit_SHT4x(void);

  bool begin(I2cBus *theWire = &Wire);
  uint32_t readSerial(void);
  bool reset(void);

  void setPrecision(sht4x_precision_t prec);
  sht4x_precision_t getPrecision(void);
  void setHeater(sht4x_heater_t heat);
  sht4x_heater_t getHeater(void);

  bool getEvent(sensors_event_t *humidity, sensors_event_t *temp);
  Adafruit_Sensor *getTemperatureSensor(void);
  Adafruit_Sensor *getHumiditySensor(void);

protected:
  float _temperature, ///< Last reading's temperature (C)
      _humidity;      ///< Last reading's humidity (percent)

  uint16_t _sensorid_humidity; ///< ID number for humidity
  uint16_t _sensorid_temp;     ///< ID number for temperature

  Adafruit_I2CDevice *i2c_dev = NULL;      ///< Pointer to I2C bus interface
  Adafruit_SHT4x_Temp *temp_sensor = NULL; ///< Temp sensor data object
  Adafruit_SHT4x_Humidity *humidity_sensor =
      NULL; ///< Humidity sensor data object

private:
  sht4x_precision_t _precision = SHT4X_HIGH_PRECISION;
  sht4x_heater_t _heater = SHT4X_NO_HEATER;

  bool writeCommand(uint16_t cmd);
  bool readCommand(uint16_t command, uint8_t *buffer, uint8_t num_bytes);

  friend class Adafruit_SHT4x_Temp;     ///< Gives access to private members to
                                        ///< Temp data object
  friend class Adafruit_SHT4x_Humidity; ///< Gives access to private members to
                                        ///< Humidity data object

  void fillTempEvent(sensors_event_t *temp, uint32_t timestamp);
  void fillHumidityEvent(sensors_event_t *humidity, uint32_t timestamp);
};

#endif
