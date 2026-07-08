#include <PID_v1.h>  //

#include "main.h"

// PID VARIABLES
#define skinPID 0
#define airPID 1
#define humidityPID 2
#define numPID 3

#define PID_TEMPERATURE_SAMPLE_TIME 4000
#define PID_HUMIDITY_SAMPLE_TIME 200

#define KP_AIR 150
#define KI_AIR 0.75
#define KD_AIR 250
#define AWO_SKIN 2

#define KP_SKIN 100
#define KI_SKIN 0.5
#define KD_SKIN 250
#define AWO_AIR 2

#define KP_HUMIDITY 200
#define KI_HUMIDITY 2
#define KD_HUMIDITY 20
#define AWO_HUMIDITY 5

// Fan RPM closed-loop control. Not part of the numPID-indexed arrays above
// (air/skin/humidity are mutually-exclusive control *modes*; the fan loop
// simply follows in3.fanCommandedOn independently of which mode is active).
//
// Gain sizing: the plant needs ~137/255 duty for 4000 rpm, i.e. ~29 rpm per
// duty count, and the measured RPM passes a 6th-order Butterworth with a
// few hundred ms of lag — so the loop must stay slow (crossover ~1 rad/s).
// The original 0.5/0.3 gains commanded ~15x the duty the plant needs per
// rpm of error, slamming the output rail-to-rail against the filter lag
// (observed on hardware: fan revs up hard, brakes hard, in a limit cycle).
// Bench-trim from here: raise KI if heater-sag recovery feels too slow,
// lower it if the post-spin-up overshoot lingers too long.
#define KP_FAN 0.03
#define KI_FAN 0.05
#define KD_FAN 0.0
#define PID_FAN_SAMPLE_TIME 200
