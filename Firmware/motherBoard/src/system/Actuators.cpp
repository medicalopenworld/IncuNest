/*
  MIT License

  Copyright (c) 2022 Medical Open World, Pablo Sánchez Bergasa

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/
#include <Arduino.h>

#include "main.h"

extern IncuNest_parameters in3;
extern PID fanControlPID;
extern double fanControlPIDOutput;

// Migrated from legacy/UI_actuatorsProgress.cpp - despite living in the
// on-board UI folder, this is live actuator control called from the active
// HMI-driven path (main.cpp), CommTask.cpp, calibrateSensors.cpp and
// initHardware.cpp, not from any UI code.
void turnFans(bool mode) {
  in3.fanCommandedOn = mode || in3.phototherapy;
  digitalWrite(ACTUATORS_EN, mode || in3.phototherapy);
#if (HW_NUM >= 8)
  ledcWrite(FAN_PWM_CHANNEL,
            (mode && !ongoingFanCriticalAlarm()) * in3.fanPwrSupplyPWM);
#if defined(FAN_SPEED_FEEDBACK)
  if (in3.fanHasSpeedFeedback) {
    fanControlPID.SetMode(in3.fanCommandedOn ? AUTOMATIC : MANUAL);
    if (!in3.fanCommandedOn) {
      fanControlPIDOutput = 0;
      ledcWrite(FAN_CTL_PWM_CHANNEL, 0);
    }
  } else
#endif
  {
    ledcWrite(FAN_CTL_PWM_CHANNEL, mode * in3.fanCtlPWM);
  }
#else
  digitalWrite(FAN, in3.phototherapy || mode && !ongoingFanCriticalAlarm());
#endif
}
