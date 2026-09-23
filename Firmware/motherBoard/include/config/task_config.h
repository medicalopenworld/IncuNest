#pragma once
// Task priorities, periods, and core assignments

#define CORE_MONITOR_FREERTOS 0
#define CORE_ID_FREERTOS 1

// Tasks priorities
#define POWER_MANAGEMENT_TASK_PRIORITY 1
#define TIME_TRACK_TASK_PRIORITY 2
#define OTA_TASK_PRIORITY 4
#define GPRS_TAST_PRIORITY 5
#define BUZZER_TASK_PRIORITY 6
#define COMMUNICATION_TASK_PRIORITY 7
#define COMMUNICATION_RECEIVER_PRIORITY 7
#define SENSORS_TASK_PRIORITY 8
// Al nivel de SENSORS y no por debajo: en un equipo con SensorBoard, lo que
// llega por este enlace ES el sensor de aire de la incubadora (la variable
// del PID), no telemetria accesoria. Con una prioridad baja, el demonio USB y
// la tarea del driver CDC se quedaban por debajo de siete tareas de periodo
// 1 ms en el mismo core y podian perder datos de la propia variable de
// control.
#define SENSORBOARD_TASK_PRIORITY 8
#define SPO2_TASK_PRIORITY 8
#define SECURITY_TASK_PRIORITY 9
#define GPRS_MONITOR_TASK_PRIORITY 10

#define PWR_HOLD_MS 3000
#define PWR_OFF_UPDATE_INTERVAL_MS 200
#define POWER_MANAGEMENT_TASK_PERIOD_MS 50

#define GPRS_TASK_PERIOD_MS 1
#define OTA_TASK_PERIOD_MS 50
#define SENSORS_TASK_PERIOD_MS 1
#define SPO2_TASK_PERIOD_MS 1
#define SKIN_SENSOR_UPDATE_PERIOD_MS 200 // in millis
#define ROOM_SENSOR_UPDATE_PERIOD_MS 1000
#define ROOM_SENSOR_RECONNECT_MS     500
#define PHOTOTHERAPY_INITIAL_PWM_PCT 40
// Corriente de trabajo del lazo de fototerapia. NO es un duty: sensors_module
// mueve el PWM a 1 cuenta/s hasta igualar este valor, asi que este define —y no
// PHOTOTHERAPY_INITIAL_PWM_PCT, que solo es la semilla— es el unico ajuste de
// intensidad que sobrevive a la regulacion.
//
// Historia del valor:
//   2026-09-21  0.45 -> 0.27 A (-40 %) por peticion de producto. Sin efecto
//               real: el lazo no se ejecutaba nunca (known_issues.md #17b).
//   2026-09-23  0.27 -> 0.45 A, vuelta al valor original por decision de
//               producto, ya con el lazo funcionando.
//
// OJO al leer la flota: hasta 18.41 el lazo estaba muerto, asi que los ~0.45 A
// que se ven en las unidades antiguas son su semilla en lazo abierto
// (extrapolada para 0.45 por el autotest), no un valor regulado. Con el lazo
// vivo el resultado deberia ser el mismo en las unidades lineales y corregido en
// las que no lo son (la de banco).
//
// Va EMPAREJADO con PHOTOTHERAPY_CONSUMPTION_DEFAULT (initHardware.cpp), que
// extrapola el PWM inicial para este mismo setpoint. Y la semilla guardada en
// NVS (KEY_PHOTO_PWM) lleva al lado el objetivo para el que se calibro
// (KEY_PHOTO_PWM_TGT): si este define cambia, esa semilla se descarta en vez de
// arrancar en el punto de trabajo de otro objetivo.
//
// La corriente NO esta calibrada contra irradiancia; el valor real de
// uW/cm2/nm solo lo da un radiometro sobre la unidad.
#define PHOTO_TARGET_CURRENT 0.45f
#define PHOTO_SETTLE_MS      3000
#define PHOTO_CONTROL_PERIOD_MS 1000
#define PHOTO_MAX_STEP       1
#define PHOTO_TOLERANCE_A    0.02f
#define PHOTO_MIN_PWM        10
#define DIGITAL_CURRENT_SENSOR_PERIOD_MS 5
#define BUZZER_TASK_PERIOD_MS 10
#define SECURITY_TASK_PERIOD_MS 1
#define COMMUNICATION_TASK_PERIOD_MS 1
// 1 s basta: el heartbeat del SensorBoard es de 30 s y su margen de 90 s.
#define SENSORBOARD_TASK_PERIOD_MS 1000
#define TIME_TRACK_TASK_PERIOD_MS 100
#define FAN_TASK_PERIOD_MS 10
#define LOOP_TASK_PERIOD_MS 1000
#define GPRS_MONITOR_TASK_PERIOD 5000
#define GPRS_MONITOR_TASK_DELETE 30000
