#include "platform/plat_num.h"

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  // Copia literal de la implementacion de Arduino, division entera incluida.
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
