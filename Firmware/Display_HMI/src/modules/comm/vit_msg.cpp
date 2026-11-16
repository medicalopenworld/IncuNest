#include "modules/comm/vit_msg.h"

#include <stdio.h>

bool vit_parse(const char *line, VitMsg *out) {
  int hr = 0, spo2 = 0;
  float pi = 0.0f;
  const int n = sscanf(line, "CTRL,VIT,%d,%d,%f", &hr, &spo2, &pi);
  if (n < 2) return false;
  out->hr   = (uint16_t)hr;
  out->spo2 = (uint8_t)spo2;
  out->pi   = (n >= 3) ? pi : 0.0f;
  return true;
}
