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
// Para los static_assert que atan el patron de rafaga (main.h) con la rafaga
// minima que exige 6.10 (ALARM_MIN_BURST_MS_*).
#include "modules/control/alarm_machine.h"

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
// --- Comprobacion EN COMPILACION de las ventanas de la Tabla 3 y la Tabla 4 ---
//
// Este es el unico fichero que ve a la vez las constantes del patron (main.h)
// y las de la rafaga minima de 6.10 (alarm_machine.h). Verificarlas aqui es lo
// que impide que vuelvan a divergir en silencio, como ya paso una vez.
static_assert(ALARM_PULSE_SPACING_X_MS >= 50u && ALARM_PULSE_SPACING_X_MS <= 125u,
              "Tabla 3: x debe estar entre 50 y 125 ms");
static_assert(ALARM_PULSE_SPACING_Y_MS >= 125u && ALARM_PULSE_SPACING_Y_MS <= 250u,
              "Tabla 3: y debe estar entre 125 y 250 ms");
static_assert(ALARM_PULSE_MS >= 75u && ALARM_PULSE_MS <= 200u,
              "Tabla 4: duracion de pulso de ALTA entre 75 y 200 ms");
static_assert(ALARM_PULSE_MS >= 125u && ALARM_PULSE_MS <= 250u,
              "Tabla 4: duracion de pulso de MEDIA/BAJA entre 125 y 250 ms");
static_assert(ALARM_PULSE_RISE_MS * 10u >= ALARM_PULSE_MS &&
                  ALARM_PULSE_RISE_MS * 10u <= ALARM_PULSE_MS * 4u,
              "Tabla 4: RISE TIME entre el 10 % y el 40 % de la duracion del pulso");
static_assert(ALARM_PULSE_RISE_MS + ALARM_PULSE_FALL_MS < ALARM_PULSE_MS,
              "las rampas no pueden comerse el pulso entero");
// Intervalo ENTRE rafagas = periodo - duracion de la rafaga.
static_assert(ALARM_BURST_PERIOD_MS_HIGH >= ALARM_BURST_LEN_MS_HIGH + 2500u &&
                  ALARM_BURST_PERIOD_MS_HIGH <= ALARM_BURST_LEN_MS_HIGH + 15000u,
              "Tabla 3: intervalo entre rafagas de ALTA entre 2,5 s y 15 s");
static_assert(ALARM_BURST_PERIOD_MS_MEDIUM >= ALARM_BURST_LEN_MS_MEDIUM + 2500u &&
                  ALARM_BURST_PERIOD_MS_MEDIUM <= ALARM_BURST_LEN_MS_MEDIUM + 30000u,
              "Tabla 3: intervalo entre rafagas de MEDIA entre 2,5 s y 30 s");
static_assert(ALARM_BURST_PERIOD_MS_LOW > ALARM_PULSE_MS + 15000u,
              "Tabla 3: intervalo entre rafagas de BAJA mayor que 15 s");
// Orden entre prioridades que exige la Tabla 3.
static_assert(ALARM_BURST_PERIOD_MS_MEDIUM - ALARM_BURST_LEN_MS_MEDIUM >=
                  ALARM_BURST_PERIOD_MS_HIGH - ALARM_BURST_LEN_MS_HIGH,
              "Tabla 3: el intervalo de MEDIA no puede ser menor que el de ALTA");
static_assert(ALARM_BURST_PERIOD_MS_LOW - ALARM_PULSE_MS >=
                  ALARM_BURST_PERIOD_MS_MEDIUM - ALARM_BURST_LEN_MS_MEDIUM,
              "Tabla 3: el intervalo de BAJA no puede ser menor que el de MEDIA");
static_assert(ALARM_PULSE_SPACING_Y_MS >= ALARM_PULSE_SPACING_X_MS,
              "Tabla 3: MEDIA t+y debe ser mayor o igual que ALTA t+x");
// 6.10: media rafaga en ALTA, rafaga entera en MEDIA.
static_assert(ALARM_MIN_BURST_MS_HIGH >=
                  (ALARM_BURST_PULSES_HIGH / 2u) * ALARM_PULSE_MS +
                      (ALARM_BURST_PULSES_HIGH / 2u - 1u) * ALARM_PULSE_SPACING_X_MS,
              "6.10: ALARM_MIN_BURST_MS_HIGH no cubre media rafaga");
static_assert(ALARM_MIN_BURST_MS_MEDIUM >= ALARM_BURST_LEN_MS_MEDIUM,
              "6.10: ALARM_MIN_BURST_MS_MEDIUM no cubre la rafaga entera");

// Amplitud del pulso con sus rampas de subida y bajada (Tabla 4).
//
// La unica palanca de amplitud disponible es el ciclo de trabajo del PWM: el
// zumbador es pasivo y se excita con una onda cuadrada de
// BUZZER_PWM_FREQUENCY. Subir el duty de 0 al 50 % sube la energia entregada
// y con ella el nivel acustico, que es lo que la norma llama RISE TIME.
//
// LIMITE HONESTO: variar el duty tambien cambia el contenido armonico, y el
// tiempo de subida ACUSTICO real lo domina la respuesta mecanica del
// transductor, no esta rampa. Esto acerca el pulso a la forma que pide la
// Tabla 4, pero afirmar cumplimiento exige medirlo con microfono.
static void writePulseAmplitude(uint32_t elapsedInPulse)
{
  const uint32_t peak = (uint32_t)BUZZER_HALF_PWM;
  uint32_t duty = peak;
  if (elapsedInPulse < ALARM_PULSE_RISE_MS)
  {
    duty = (peak * elapsedInPulse) / ALARM_PULSE_RISE_MS;
  }
  else if (elapsedInPulse + ALARM_PULSE_FALL_MS >= ALARM_PULSE_MS)
  {
    const uint32_t remain = ALARM_PULSE_MS - elapsedInPulse;
    duty = (peak * remain) / ALARM_PULSE_FALL_MS;
  }
  ledcWrite(BUZZER_PWM_CHANNEL, duty);
}

// Hueco que sigue al pulso numero `pulsesDone` (1 = ya sono el primero).
//
// En ALTA, tras el pulso 5 el hueco vale 2x + y en vez de x: eso parte la
// rafaga de diez en DOS GRUPOS DE CINCO, que es lo que hace reconocible el
// patron de maxima prioridad (Tabla 3). En MEDIA el espaciado es y.
static uint32_t gapAfterPulse(AlarmPriority priority, uint32_t pulsesDone,
                              uint32_t burstPulses)
{
  if (priority == ALARM_PRIORITY_HIGH)
  {
    return (pulsesDone == burstPulses / 2u) ? ALARM_GROUP_GAP_MS
                                            : ALARM_PULSE_SPACING_X_MS;
  }
  return ALARM_PULSE_SPACING_Y_MS;
}

void buzzerAlarmUpdate(bool audioRequired, AlarmPriority priority)
{
  static uint32_t phaseStart = 0;
  static uint32_t pulsesLeft = 0;
  static uint32_t pulsesDone = 0;
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
    pulsesDone = 0;
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
    pulsesDone = 0;
    phaseStart = now;
    on = true;
    writePulseAmplitude(0);
    return; // el primer pulso ya ha arrancado en este mismo ciclo
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
    pulsesDone = 0;
    phaseStart = now;
    on = false;
  }

  const uint32_t elapsed = (uint32_t)(now - phaseStart);

  if (on)
  {
    if (elapsed < ALARM_PULSE_MS)
    {
      // Dentro del pulso la amplitud la gobierna la rampa, asi que el duty se
      // reescribe en cada ciclo, no solo en los bordes.
      writePulseAmplitude(elapsed);
      return;
    }
    ledcWrite(BUZZER_PWM_CHANNEL, 0);
    phaseStart = now;
    on = false;
    pulsesDone++;
    pulsesLeft--;
    return;
  }

  if (elapsed < gapAfterPulse(priority, pulsesDone, burstPulses))
  {
    return;
  }
  phaseStart = now;
  on = true;
  writePulseAmplitude(0);
}
