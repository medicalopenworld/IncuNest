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

#define BUZZER_DISABLED false
#define BUZZER_ENABLED true

int buzzerBeeps, buzzerToneTime;
long buzzerTime;
bool buzzerBuzzing;

void buzzerHandler()
{
  if (millis() - buzzerTime > buzzerToneTime && buzzerBeeps)
  {
    buzzerBeeps -= buzzerBuzzing;
    buzzerBuzzing = !buzzerBuzzing;
    ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM * buzzerBuzzing);
    buzzerTime = millis();
  }
}

void shutBuzzer()
{
    // logI("[BUZZER] -> BUZZER was shutted");
    buzzerBeeps = 0;
    buzzerBuzzing = false;
    ledcWrite(BUZZER_PWM_CHANNEL, false);
}

void buzzerTone(int beepTimes, int timevTaskDelay, int freq)
{
  // logI("[BUZZER] -> BUZZER beep mode activated  " + String(beepTimes) + "
  // times");
  if (timevTaskDelay > buzzerToneTime || buzzerBeeps == 0) {
    buzzerToneTime = timevTaskDelay;
  }
  buzzerBeeps += beepTimes;
}
