#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  ACTUATION_OFF = 0,
  ACTUATION_TEMPERATURE = 1,
  ACTUATION_HUMIDITY = 2,
  ACTUATION_TEMP_AND_HUMIDITY = 3,
} ActuationMode;

typedef enum {
  CONTROL_SKIN_MODE = 0,
  CONTROL_AIR_MODE  = 1,
} ControlMode;

typedef enum {
  SPANISH = 0,
  ENGLISH,
  FRENCH,
  PORTUGUESE,
  NUM_LANGUAGES,
} Language;

typedef enum {
  COMM_STATUS_NONE        = 0,
  COMM_STATUS_GPRS_ONLY   = 1,
  COMM_STATUS_GPRS_SERVER = 2,
  COMM_STATUS_WIFI_ONLY   = 3,
  COMM_STATUS_WIFI_SERVER = 4,
} CommStatus;

#ifdef __cplusplus
}
#endif
