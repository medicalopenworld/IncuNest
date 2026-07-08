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
// These are starting values — must be bench-tuned on real hardware, exactly
// like the gains above were.
#define KP_FAN 0.5
#define KI_FAN 0.3
#define KD_FAN 0.0
#define PID_FAN_SAMPLE_TIME 200
