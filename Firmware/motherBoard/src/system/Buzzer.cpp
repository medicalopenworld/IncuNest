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

// Motor de audio de alarma: gobernado por ESTADO, no por eventos ni por un
// contador de agotamiento. IEC 60601-1-8 6.10 es taxativo: el audio de alarma
// solo cesa por accion del operador (inactivacion o reset), nunca por haber
// sonado "suficiente" tiempo. Mientras audioRequired siga en true el patron de
// rafaga se regenera indefinidamente.
//
// No comparte variables con buzzerHandler()/buzzerTone() (feedback de
// encoder, confirmacion de comandos HMI, autotest de arranque): son dos
// consumidores distintos del mismo canal PWM y este es deliberadamente
// independiente para que un cambio de prioridad de alarma pueda tomar el
// canal de inmediato sin depender del contador de la otra maquina.
//
// Gobernarse por estado (prioridad activa) y no por el flanco de subida de
// audioRequired es lo que evita el defecto que motiva esta tarea: si una
// alarma de prioridad mas alta aparece mientras ya suena una de prioridad mas
// baja, audioRequired ya era true y no habria flanco — con el motor viejo esa
// alarma de maxima prioridad quedaba muda. Aqui, en cambio, un cambio de
// prioridad arranca el patron de la prioridad nueva EN EL MISMO CICLO en que
// se detecta el cambio (ver `freshStart` abajo), sin esperar a que termine la
// rafaga vieja ni al periodo de silencio entre rafagas de la nueva: ese
// periodo (2,5-15 s en ALTA, hasta 30 s en BAJA, Tabla 3) separa rafagas
// SUCESIVAS de un patron que ya esta sonando, nunca la primera — exigirlo
// tambien en el arranque dejaba el zumbador mudo hasta 30 s tras la primera
// alarma, o 10 s de silencio justo al escalar a maxima prioridad (hallazgo
// C-1 de la revision de la tarea 11).
void buzzerAlarmUpdate(bool audioRequired, AlarmPriority priority)
{
  static uint32_t phaseStart = 0;
  static uint32_t pulsesLeft = 0;
  static bool on = false;
  static bool wasAudioRequired = false;
  static AlarmPriority lastPriority = ALARM_PRIORITY_LOW;

  if (!audioRequired)
  {
    if (on)
    {
      ledcWrite(BUZZER_PWM_CHANNEL, 0);
      on = false;
    }
    pulsesLeft = 0;
    phaseStart = millis();
    wasAudioRequired = false;
    lastPriority = priority;
    return;
  }

  const uint32_t now = millis();

  const uint32_t burstPulses =
      priority == ALARM_PRIORITY_HIGH   ? ALARM_BURST_PULSES_HIGH
      : priority == ALARM_PRIORITY_MEDIUM ? ALARM_BURST_PULSES_MEDIUM
                                          : ALARM_BURST_PULSES_LOW;
  const uint32_t burstPeriod =
      priority == ALARM_PRIORITY_HIGH   ? ALARM_BURST_PERIOD_MS_HIGH
      : priority == ALARM_PRIORITY_MEDIUM ? ALARM_BURST_PERIOD_MS_MEDIUM
                                          : ALARM_BURST_PERIOD_MS_LOW;

  // Primer ciclo con audio exigido tras un silencio, o cambio de prioridad
  // mientras ya sonaba: ambos arrancan la rafaga YA, en este mismo ciclo, en
  // vez de caer en el chequeo de `burstPeriod` de mas abajo (que solo debe
  // gobernar el silencio ENTRE rafagas de un patron que ya esta sonando).
  const bool freshStart = !wasAudioRequired || (priority != lastPriority);
  wasAudioRequired = true;
  lastPriority = priority;

  if (freshStart)
  {
    pulsesLeft = burstPulses;
    phaseStart = now;
    on = true;
    ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM);
    return; // el primer pulso ya ha salido en este mismo ciclo
  }

  if (pulsesLeft == 0)
  {
    // Silencio ENTRE rafagas sucesivas de un patron que ya esta sonando (ver
    // comentario de cabecera): la rafaga inicial nunca pasa por aqui porque
    // freshStart la intercepta arriba.
    if ((uint32_t)(now - phaseStart) < burstPeriod)
    {
      return;
    }
    pulsesLeft = burstPulses;
    phaseStart = now;
    on = false;
  }

  const uint32_t slot = on ? ALARM_PULSE_MS : ALARM_PULSE_GAP_MS;
  if ((uint32_t)(now - phaseStart) < slot)
  {
    return;
  }
  phaseStart = now;
  on = !on;
  ledcWrite(BUZZER_PWM_CHANNEL, on ? BUZZER_HALF_PWM : 0);
  if (!on && pulsesLeft > 0)
  {
    pulsesLeft--;
  }
}
