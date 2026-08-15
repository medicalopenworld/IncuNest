#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Conjunto de condiciones de alarma derivado de IEC 60601-2-19 (201.12.3.101
// a 201.12.3.103, 201.15.4.2.1) y de la Tabla 1 de IEC 60601-1-8. La
// justificacion por condicion vive en docs/alarms_normative_analysis.md §5.
//
// El valor numerico es el indice de bit en el bitmask del protocolo, asi que
// insertar en medio rompe la compatibilidad: las condiciones nuevas van al
// final, antes de ALARM_COUNT.
typedef enum {
  ALARM_NONE = 0,
  // --- Prioridad ALTA ---
  ALARM_AIR_THERMAL_CUTOUT = 1,       // 201.15.4.2.1 aa)
  ALARM_SKIN_THERMAL_CUTOUT = 2,      // 201.15.4.2.1 bb)
  ALARM_AIR_SENSOR_FAULT = 3,
  ALARM_SKIN_SENSOR_FAULT_SKIN_MODE = 4,  // 201.12.3.102
  ALARM_FAN_FAILURE = 5,                  // 201.12.3.101
  ALARM_AIR_OUTLET_BLOCKED = 6,           // 201.12.3.101
  ALARM_MAINS_INTERRUPTION = 7,           // 201.12.3.103
  // --- Prioridad MEDIA ---
  ALARM_AIR_TEMP_DEVIATION_HIGH = 8,   // 201.15.4.2.1 dd), +3 C
  ALARM_AIR_TEMP_DEVIATION_LOW = 9,    // 201.15.4.2.1 dd), -3 C
  ALARM_SKIN_TEMP_DEVIATION_HIGH = 10, // 201.15.4.2.1 ee), +1 C
  ALARM_SKIN_TEMP_DEVIATION_LOW = 11,  // 201.15.4.2.1 ee), -1 C
  ALARM_HEATER_FAULT = 12,
  ALARM_SUPPLY_UNDERVOLTAGE = 13,
  ALARM_HMI_LINK_LOST = 14,
  // --- Prioridad BAJA ---
  ALARM_SKIN_SENSOR_FAULT_AIR_MODE = 15,
  ALARM_HUMIDITY_DEVIATION = 16,
  ALARM_COUNT
} AlarmId;

// Alias de compatibilidad: hay bucles que iteran el rango completo del enum.
#define NO_ALARMS  ALARM_NONE
#define NUM_ALARMS ALARM_COUNT

typedef enum {
  ALARM_PRIORITY_LOW = 0,
  ALARM_PRIORITY_MEDIUM = 1,
  ALARM_PRIORITY_HIGH = 2,
} AlarmPriority;

#define MAX_ALARM_STRING_SIZE 255

#ifdef __cplusplus
}
#endif
