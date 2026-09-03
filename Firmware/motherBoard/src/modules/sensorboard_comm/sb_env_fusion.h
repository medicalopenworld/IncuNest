#pragma once
// Fusion y VOTACION de las tres posiciones de sensor del SensorBoard.
//
// El ADR-0002 del SensorBoard es explicito: el reporta las tres posiciones tal
// cual, con null en la que este caida, y "la fusion/votacion es
// responsabilidad de la motherboard". Esto es esa votacion.
//
// POR QUE NO BASTA LA MEDIA. En un equipo con SensorBoard, el valor que sale
// de aqui va a in3.temperature[ROOM_DIGITAL_TEMP_SENSOR], y de esa MISMA
// variable comen las tres barreras: el PID de aire, el corte termico
// (security.cpp checkThermalCutOuts) y la alarma de desviacion. Con una media
// simple, un solo sensor sesgado las engana a las tres a la vez y en la
// direccion peligrosa: con la posicion 2 pegada a 25 C y consigna 36 C,
// (T+T+25)/3 = 36 da T real = 41.5 C. El corte termico compara la misma media,
// lee 36 y nunca cruza el umbral; la desviacion respecto a consigna es cero.
// Hipertermia silenciosa. El caso simetrico (un sensor alto) es benigno:
// dispara el corte, que ademas es latching.
//
// Por eso aqui se vota con la mediana y se descarta lo que se aparte: la
// mediana de tres es inmune a UN sensor mentiroso, sea por arriba o por abajo.
#include <stdbool.h>
#include <stdint.h>

// Dispersion maxima admisible entre posiciones, en grados. Tres sensores
// dentro de la misma cabina con el ventilador en marcha no deberian
// discrepar de esto; si lo hacen, uno miente y no sabemos cual.
#define SB_ENV_MAX_SPREAD_C 2.0f
#define SB_ENV_MAX_SPREAD_RH 10.0f

// Rango plausible para aire de CABINA. Mas estrecho que el
// DIG_TEMP_TO_DISCARD de board.h (5-60 C), que es un rango de datasheet: una
// incubadora fuera de 15-50 C no esta midiendo su cabina.
#define SB_ENV_TEMP_MIN_C 15.0f
#define SB_ENV_TEMP_MAX_C 50.0f

typedef struct {
  bool valid;          // hay una medida utilizable
  float value;         // valor fusionado
  bool has_redundant;  // hay una segunda posicion independiente
  float redundant;
  uint8_t used;        // posiciones que entraron en el resultado (0-3)
  uint8_t discarded;   // posiciones plausibles pero descartadas por dispersion
} SbFusion;

// Devuelve valid=false cuando no hay medida de fiar. El llamante NO debe
// refrescar entonces el sello de frescura: sin arbitro no hay medida, y que
// salte ALARM_AIR_SENSOR_FAULT (ALTA, corta calefactor) es exactamente la
// respuesta correcta -- es lo mismo que pasaria con un STS35 averiado.
//
// Con 3 posiciones: mediana, se descarta lo que se aparte mas de max_spread y
// se promedia lo que sobrevive.
// Con 2: solo vale si concuerdan; si discrepan no hay forma de saber cual
// miente y se devuelve invalido.
// Con 1: se acepta, pero has_redundant=false -- hay medida y no hay
// redundancia, que es mejor que quedarse sin control termico.
SbFusion sb_fuse(const bool valid[3], const float value[3], float lo, float hi,
                 float max_spread);
