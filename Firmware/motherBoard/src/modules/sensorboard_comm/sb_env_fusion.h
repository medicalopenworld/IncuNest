#pragma once
// Que valor de las tres posiciones del SensorBoard gobierna el lazo de aire.
//
// NO hace cribado. Las tres lecturas viajan crudas a la nube (sb_temp0/1/2,
// sb_hum0/1/2) para poder disenar el cribado mas adelante con datos reales; lo
// unico que se decide aqui es CUAL de las tres se usa para controlar, porque
// in3.temperature[ROOM_DIGITAL_TEMP_SENSOR] tiene que salir de algun numero.
//
// Se usa la MEDIANA, y no la media, por una razon concreta: de esa misma
// variable comen el PID de aire, el corte termico (security.cpp
// checkThermalCutOuts) y la alarma de desviacion. La media es la unica opcion
// que puede devolver un valor que NINGUN sensor esta midiendo, y con eso un
// solo sensor sesgado engana a las tres barreras a la vez y en la direccion
// peligrosa: con la posicion 2 pegada a 25 C y consigna 36 C, (T+T+25)/3 = 36
// da T real = 41.5 C, el corte termico compara la misma media y lee 36, y la
// desviacion respecto a consigna es cero. La mediana de tres siempre es una
// lectura real y es inmune a que UNO de los tres mienta, por arriba o por
// abajo. Cuesta lo mismo.
#include <stdbool.h>
#include <stdint.h>

// Rango plausible para aire de CABINA, no de datasheet: una incubadora fuera
// de 15-50 C no esta midiendo su cabina. Descartar un valor imposible no es
// cribado, es lo mismo que ya hacia el camino I2C con DIG_TEMP_TO_DISCARD.
#define SB_ENV_TEMP_MIN_C 15.0f
#define SB_ENV_TEMP_MAX_C 50.0f

typedef struct {
  bool valid;   // hay al menos una lectura plausible
  float value;  // mediana de las plausibles
  uint8_t used; // cuantas posiciones habia (1-3); sale por telemetria
} SbFusion;

// Con 3 plausibles: la mediana.
// Con 2: la media de las dos -- no hay tercero que arbitre, y elegir una
//        seria una decision de cribado que todavia no toca tomar.
// Con 1: esa. Hay medida y no hay redundancia, que es mejor que quedarse sin
//        control termico.
// Con 0: valid=false, y entonces el llamante NO debe refrescar el sello de
//        frescura: que salte ALARM_AIR_SENSOR_FAULT es lo correcto, igual que
//        con un STS35 averiado.
SbFusion sb_fuse(const bool valid[3], const float value[3], float lo, float hi);
