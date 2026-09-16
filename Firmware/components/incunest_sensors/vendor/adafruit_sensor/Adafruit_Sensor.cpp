#include "Adafruit_Sensor.h"

// PARCHE INCUNEST: printSensorDetails() volcaba por Serial, que no existe sin
// Arduino. Es traza de depuracion opcional (el firmware no la llama); se
// redirige al log de ESP-IDF en vez de arrastrar un puerto serie entero.
#include "esp_log.h"

// PARCHE INCUNEST: F() metia la cadena en flash en AVR; en ESP32 ya era la
// identidad y aqui no existe.
#ifndef F
#define F(x) (x)
#endif
template <class T> static void plat_sensor_trace(const T &v) { (void)v; }
static void plat_sensor_trace() {}

/**************************************************************************/
/*!
    @brief  Prints sensor information to serial console
*/
/**************************************************************************/
void Adafruit_Sensor::printSensorDetails(void) {
  sensor_t sensor;
  getSensor(&sensor);
  plat_sensor_trace(F("------------------------------------"));
  plat_sensor_trace(F("Sensor:       "));
  plat_sensor_trace(sensor.name);
  plat_sensor_trace(F("Type:         "));
  switch ((sensors_type_t)sensor.type) {
  case SENSOR_TYPE_ACCELEROMETER:
    plat_sensor_trace(F("Acceleration (m/s2)"));
    break;
  case SENSOR_TYPE_MAGNETIC_FIELD:
    plat_sensor_trace(F("Magnetic (uT)"));
    break;
  case SENSOR_TYPE_ORIENTATION:
    plat_sensor_trace(F("Orientation (degrees)"));
    break;
  case SENSOR_TYPE_GYROSCOPE:
    plat_sensor_trace(F("Gyroscopic (rad/s)"));
    break;
  case SENSOR_TYPE_LIGHT:
    plat_sensor_trace(F("Light (lux)"));
    break;
  case SENSOR_TYPE_PRESSURE:
    plat_sensor_trace(F("Pressure (hPa)"));
    break;
  case SENSOR_TYPE_PROXIMITY:
    plat_sensor_trace(F("Distance (cm)"));
    break;
  case SENSOR_TYPE_GRAVITY:
    plat_sensor_trace(F("Gravity (m/s2)"));
    break;
  case SENSOR_TYPE_LINEAR_ACCELERATION:
    plat_sensor_trace(F("Linear Acceleration (m/s2)"));
    break;
  case SENSOR_TYPE_ROTATION_VECTOR:
    plat_sensor_trace(F("Rotation vector"));
    break;
  case SENSOR_TYPE_RELATIVE_HUMIDITY:
    plat_sensor_trace(F("Relative Humidity (%)"));
    break;
  case SENSOR_TYPE_AMBIENT_TEMPERATURE:
    plat_sensor_trace(F("Ambient Temp (C)"));
    break;
  case SENSOR_TYPE_OBJECT_TEMPERATURE:
    plat_sensor_trace(F("Object Temp (C)"));
    break;
  case SENSOR_TYPE_VOLTAGE:
    plat_sensor_trace(F("Voltage (V)"));
    break;
  case SENSOR_TYPE_CURRENT:
    plat_sensor_trace(F("Current (mA)"));
    break;
  case SENSOR_TYPE_COLOR:
    plat_sensor_trace(F("Color (RGBA)"));
    break;
  case SENSOR_TYPE_TVOC:
    plat_sensor_trace(F("Total Volatile Organic Compounds (ppb)"));
    break;
  case SENSOR_TYPE_VOC_INDEX:
    plat_sensor_trace(F("Volatile Organic Compounds (Index)"));
    break;
  case SENSOR_TYPE_NOX_INDEX:
    plat_sensor_trace(F("Nitrogen Oxides (Index)"));
    break;
  case SENSOR_TYPE_CO2:
    plat_sensor_trace(F("Carbon Dioxide (ppm)"));
    break;
  case SENSOR_TYPE_ECO2:
    plat_sensor_trace(F("Equivalent/estimated CO2 (ppm)"));
    break;
  case SENSOR_TYPE_PM10_STD:
    plat_sensor_trace(F("Standard Particulate Matter 1.0 (ppm)"));
    break;
  case SENSOR_TYPE_PM25_STD:
    plat_sensor_trace(F("Standard Particulate Matter 2.5 (ppm)"));
    break;
  case SENSOR_TYPE_PM100_STD:
    plat_sensor_trace(F("Standard Particulate Matter 10.0 (ppm)"));
    break;
  case SENSOR_TYPE_PM10_ENV:
    plat_sensor_trace(F("Environmental Particulate Matter 1.0 (ppm)"));
    break;
  case SENSOR_TYPE_PM25_ENV:
    plat_sensor_trace(F("Environmental Particulate Matter 2.5 (ppm)"));
    break;
  case SENSOR_TYPE_PM100_ENV:
    plat_sensor_trace(F("Environmental Particulate Matter 10.0 (ppm)"));
    break;
  case SENSOR_TYPE_GAS_RESISTANCE:
    plat_sensor_trace(F("Gas Resistance (ohms)"));
    break;
  case SENSOR_TYPE_UNITLESS_PERCENT:
    plat_sensor_trace(F("Unitless Percent (%)"));
    break;
  case SENSOR_TYPE_ALTITUDE:
    plat_sensor_trace(F("Altitude (m)"));
    break;
  }

  plat_sensor_trace();
  plat_sensor_trace(F("Driver Ver:   "));
  plat_sensor_trace(sensor.version);
  plat_sensor_trace(F("Unique ID:    "));
  plat_sensor_trace(sensor.sensor_id);
  plat_sensor_trace(F("Min Value:    "));
  plat_sensor_trace(sensor.min_value);
  plat_sensor_trace(F("Max Value:    "));
  plat_sensor_trace(sensor.max_value);
  plat_sensor_trace(F("Resolution:   "));
  plat_sensor_trace(sensor.resolution);
  plat_sensor_trace(F("------------------------------------\n"));
}
