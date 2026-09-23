#pragma once
// De donde salen la temperatura y la humedad de cabina en ESTE equipo.
//
// Los pines 19/20 tienen dos funciones segun la generacion: en los equipos
// antiguos son el bus I2C2 hacia una PCBA con SHTC3 + STS35 principal y
// redundante; en los nuevos son el USB hacia el SensorBoard, que lleva esos
// sensores a bordo (3x SHT40) y los publica por `sensor_data`. No es un
// conflicto de pines: es un multiplexado por generacion.
//
// La fuente se decide UNA vez por arranque sondeando el bus I2C2 (es rapido,
// 10 ms por direccion, y no destructivo). Si responde algun sensor -> equipo
// antiguo y no se toca el USB jamas. Si el bus esta mudo -> equipo nuevo, se
// liberan los pads y se levanta el host USB. Deliberadamente NO se persiste
// en NVS: un solo cambio de modo por arranque, sin estado que se desincronice
// del hardware que hay puesto.
//
// El orden importa y es el barato: sondear I2C primero es reversible con un
// gpio_reset_pin(); probar USB primero exigiria desmontar el stack USB entero
// (cdc_acm_host_uninstall -> usb_host_uninstall -> usb_del_phy) y, si el PHY
// no devolviera los pads, el equipo antiguo se quedaria sin sensor de aire,
// que es el peor fallo posible.
#include <stdbool.h>

typedef enum {
  SENSOR_SOURCE_I2C = 0,      // STS35/SHTC3 en el bus I2C2 (equipo antiguo)
  SENSOR_SOURCE_SENSORBOARD,  // SHT40 del SensorBoard por USB (equipo nuevo)
} SensorSource;

SensorSource sensorSourceGet(void);
void sensorSourceSet(SensorSource src);

// true si el sondeo de initRoomSensor() encontro algun sensor en el bus I2C2.
// Requiere que initRoomSensor() ya haya corrido.
bool roomSensorI2CDetected(void);
