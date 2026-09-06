#include "maintenance.h"

#include <Preferences.h>
#include <cstdio>
#include <ctime>

#include "CommTask.h"          // HMI_GetEpochNow(), HMI_HasLocalTime()
#include "EEPROM_defines.h"    // HMI_NS_CFG, HMI_KEY_MNT_*
#include "esp_log.h"
#include "ui/BabyWizard.h"     // BabyWizard_GetActiveSeq(), HasLiveSession()
#include "ui/i18n.h"           // TR() para la linea de "ultima"

namespace {

constexpr const char *TAG = "MNT";
constexpr uint32_t SECS_PER_DAY = 86400UL;
// Lo que dura el "MAS TARDE". Un dia: el recordatorio no es una alarma, y
// volver a preguntar en el mismo turno solo ensena a despacharlo sin leerlo.
constexpr uint32_t SNOOZE_SECS = 24UL * 3600UL;

// Cadencia de cada nivel, en dias, en el orden de `mnt_level_t`. La diaria
// ademas exige paciente dentro; la terminal ademas vence en el alta. Ese
// "ademas" esta en isDue(), aqui solo esta el plazo.
constexpr uint32_t PERIOD_DAYS[MNT_LEVEL_COUNT] = {1, 7, 7};

const char *const KEY_LAST[MNT_LEVEL_COUNT] = {
    HMI_KEY_MNT_DAILY, HMI_KEY_MNT_WEEKLY, HMI_KEY_MNT_TERMINAL};

uint32_t s_last[MNT_LEVEL_COUNT] = {0, 0, 0};  // epoch UTC, 0 = sin registro
uint32_t s_snoozeUntil = 0;  // epoch UTC, 0 = sin "mas tarde" vigente
uint32_t s_lastSeq = 0;      // ultimo perfil de bebe visto al mando
bool s_enabled = true;
// Alta de paciente sin limpieza terminal registrada todavia. Se persiste: el
// equipo puede apagarse entre el alta y la limpieza.
bool s_terminalPending = false;

void putU32(const char *key, uint32_t v) {
  Preferences p;
  p.begin(HMI_NS_CFG, false);
  p.putUInt(key, v);
  p.end();
}

void putBool(const char *key, bool v) {
  Preferences p;
  p.begin(HMI_NS_CFG, false);
  p.putUChar(key, v ? 1 : 0);
  p.end();
}

bool validLevel(mnt_level_t lvl) {
  return lvl >= MNT_LEVEL_DAILY && lvl < MNT_LEVEL_COUNT;
}

// Plazo cumplido para ese nivel. Sin hora o sin registro previo no se puede
// afirmar nada, asi que no vence.
bool periodElapsed(mnt_level_t lvl) {
  const uint32_t now = HMI_GetEpochNow();
  if (now == 0 || s_last[lvl] == 0) return false;
  return now >= s_last[lvl] + PERIOD_DAYS[lvl] * SECS_PER_DAY;
}

}  // namespace

void Maintenance_Init(void) {
  Preferences p;
  p.begin(HMI_NS_CFG, true);
  for (int i = 0; i < MNT_LEVEL_COUNT; i++) {
    s_last[i] = p.getUInt(KEY_LAST[i], 0);
  }
  s_snoozeUntil = p.getUInt(HMI_KEY_MNT_SNOOZE, 0);
  s_lastSeq = p.getUInt(HMI_KEY_MNT_SEQ, 0);
  s_enabled = p.getUChar(HMI_KEY_MNT_EN, 1) != 0;
  s_terminalPending = p.getUChar(HMI_KEY_MNT_TPEND, 0) != 0;
  p.end();

  ESP_LOGI(TAG, "init: en=%d daily=%lu weekly=%lu term=%lu tpend=%d seq=%lu",
           (int)s_enabled, (unsigned long)s_last[MNT_LEVEL_DAILY],
           (unsigned long)s_last[MNT_LEVEL_WEEKLY],
           (unsigned long)s_last[MNT_LEVEL_TERMINAL], (int)s_terminalPending,
           (unsigned long)s_lastSeq);
}

void Maintenance_Tick(void) {
  const uint32_t now = HMI_GetEpochNow();

  if (now != 0) {
    for (int i = 0; i < MNT_LEVEL_COUNT; i++) {
      // Primer arranque con hora valida: los plazos empiezan a contar HOY. Sin
      // esto un equipo recien fabricado tendria las tres fechas a 0, o sea
      // "vencido desde 1970", y el aviso saldria en la primera pantalla que
      // viese el operador.
      if (s_last[i] == 0) {
        s_last[i] = now;
        putU32(KEY_LAST[i], now);
        ESP_LOGI(TAG, "nivel %d sembrado en el primer arranque con hora", i);
        continue;
      }
      // Reloj hacia atras (alguien ajusta la fecha a mano y se equivoca de
      // ano, o la placa corrige un epoch disparatado): el registro quedaria en
      // el futuro y el plazo no volveria a cumplirse nunca. Se reancla a hoy.
      if (s_last[i] > now) {
        ESP_LOGW(TAG, "nivel %d fechado en el futuro (%lu > %lu): reanclado", i,
                 (unsigned long)s_last[i], (unsigned long)now);
        s_last[i] = now;
        putU32(KEY_LAST[i], now);
      }
    }
    if (s_snoozeUntil > now + SNOOZE_SECS) {
      s_snoozeUntil = 0;
      putU32(HMI_KEY_MNT_SNOOZE, 0);
    }
  }

  // Alta del paciente. `BabyWizard_GetActiveSeq()` es el perfil que este HMI
  // puso al mando (0 = ninguno): pasa a 0 cuando el dialogo de salida da el
  // alta (BabyWizard_ClearActiveProfile()), y cambia de un seq a otro cuando
  // se cambia de bebe sin pasar por el alta. Las dos cosas dejan pendiente una
  // limpieza terminal; empezar con el primer paciente (0 -> seq) no, que ahi
  // no ha salido nadie.
  const uint32_t seq = BabyWizard_GetActiveSeq();
  if (seq != s_lastSeq) {
    if (s_lastSeq != 0) {
      s_terminalPending = true;
      putBool(HMI_KEY_MNT_TPEND, true);
      ESP_LOGI(TAG, "alta del paciente (seq %lu -> %lu): terminal pendiente",
               (unsigned long)s_lastSeq, (unsigned long)seq);
    }
    s_lastSeq = seq;
    putU32(HMI_KEY_MNT_SEQ, seq);
  }
}

bool Maintenance_IsEnabled(void) { return s_enabled; }

void Maintenance_SetEnabled(bool on) {
  if (on == s_enabled) return;
  s_enabled = on;
  putBool(HMI_KEY_MNT_EN, on);
  // Volver a activarlo no debe arrastrar un "mas tarde" de hace tres semanas.
  s_snoozeUntil = 0;
  putU32(HMI_KEY_MNT_SNOOZE, 0);
  ESP_LOGI(TAG, "recordatorio %s", on ? "activado" : "desactivado");
}

bool Maintenance_IsDue(mnt_level_t lvl) {
  if (!validLevel(lvl)) return false;
  switch (lvl) {
    case MNT_LEVEL_DAILY:
      // Solo con paciente dentro: una incubadora vacia no se ensucia a diario.
      // "Dentro" es el criterio de BabyWizard: identidad conocida Y terapia en
      // marcha ahora mismo, no un perfil recordado de la semana pasada.
      return BabyWizard_HasLiveSession() && periodElapsed(lvl);
    case MNT_LEVEL_WEEKLY:
      return periodElapsed(lvl);
    case MNT_LEVEL_TERMINAL:
      // El alta manda sobre el plazo, y no necesita reloj para saberse.
      return s_terminalPending || periodElapsed(lvl);
    default:
      return false;
  }
}

bool Maintenance_ShouldWarn(void) {
  if (!s_enabled) return false;
  const uint32_t now = HMI_GetEpochNow();
  if (now != 0 && s_snoozeUntil != 0 && now < s_snoozeUntil) return false;
  for (int i = 0; i < MNT_LEVEL_COUNT; i++) {
    if (Maintenance_IsDue((mnt_level_t)i)) return true;
  }
  return false;
}

void Maintenance_MarkDone(mnt_level_t lvl) {
  if (!validLevel(lvl)) return;

  s_snoozeUntil = 0;
  putU32(HMI_KEY_MNT_SNOOZE, 0);
  if (lvl == MNT_LEVEL_TERMINAL && s_terminalPending) {
    s_terminalPending = false;
    putBool(HMI_KEY_MNT_TPEND, false);
  }

  const uint32_t now = HMI_GetEpochNow();
  if (now == 0) {
    // Sin reloj no hay fecha que registrar. Se calla el aviso (el operador ha
    // limpiado de verdad) pero el registro sigue vacio, y eso es lo honesto:
    // inventar una fecha seria peor que no tener ninguna.
    ESP_LOGW(TAG, "nivel %d hecho sin hora valida: no se fecha", (int)lvl);
    return;
  }
  // Un nivel pone al dia tambien los menos profundos: la limpieza terminal
  // incluye lo que hace la semanal, y la semanal lo que hace la diaria.
  for (int i = 0; i <= (int)lvl; i++) {
    s_last[i] = now;
    putU32(KEY_LAST[i], now);
  }
  ESP_LOGI(TAG, "nivel %d registrado: %lu", (int)lvl, (unsigned long)now);
}

void Maintenance_Snooze(void) {
  const uint32_t now = HMI_GetEpochNow();
  if (now == 0) {
    ESP_LOGI(TAG, "mas tarde sin hora valida: no se puede fechar el silencio");
    return;
  }
  s_snoozeUntil = now + SNOOZE_SECS;
  putU32(HMI_KEY_MNT_SNOOZE, s_snoozeUntil);
  ESP_LOGI(TAG, "mas tarde hasta %lu", (unsigned long)s_snoozeUntil);
}

uint32_t Maintenance_LastEpoch(mnt_level_t lvl) {
  return validLevel(lvl) ? s_last[lvl] : 0;
}

void Maintenance_FormatLastLine(mnt_level_t lvl, char *out, size_t cap) {
  const uint32_t epoch = Maintenance_LastEpoch(lvl);
  char date[32];
  if (epoch == 0) {
    snprintf(date, sizeof(date), "%s", TR(STR_MAINT_NEVER));
  } else {
    // Lo ALMACENADO sigue en UTC; la hora local solo se aplica al formatear
    // para una persona (mismo criterio que BabyHistory.cpp).
    time_t t = (time_t)(HMI_HasLocalTime() ? HMI_ToLocal(epoch) : epoch);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    snprintf(date, sizeof(date), "%04d-%02d-%02d", tmv.tm_year + 1900,
             tmv.tm_mon + 1, tmv.tm_mday);
  }
  snprintf(out, cap, TR(STR_MAINT_LAST_FMT), date);
}
