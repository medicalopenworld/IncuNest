#include "maintenance.h"

#include <Preferences.h>
#include <cstdio>
#include <ctime>

#include "CommTask.h"          // HMI_GetEpochNow(), HMI_HasLocalTime()
#include "EEPROM_defines.h"    // HMI_NS_CFG, HMI_KEY_MNT_*
#include "esp_log.h"
#include "ui/BabyWizard.h"     // BabyWizard_GetActiveSeq()
#include "ui/i18n.h"           // TR() para la linea de "ultimo mantenimiento"

// Los intervalos que ofrece el desplegable de Ajustes, en el mismo orden.
// El 0 (apagado) va primero para que el indice del desplegable y este array
// sean el mismo numero.
const uint16_t MNT_INTERVAL_DAYS[MNT_INTERVAL_COUNT] = {
    MNT_INTERVAL_OFF, 7, 15, 30, 90};

namespace {

constexpr const char *TAG = "MNT";
constexpr uint32_t SECS_PER_DAY = 86400UL;
// Lo que dura el "MAS TARDE". Un dia: el recordatorio no es una alarma, y
// volver a preguntar en el mismo turno solo ensena a despacharlo sin leerlo.
constexpr uint32_t SNOOZE_SECS = 24UL * 3600UL;

uint32_t s_lastDone = 0;    // epoch UTC, 0 = sin registro
uint32_t s_snoozeUntil = 0; // epoch UTC, 0 = sin "mas tarde" vigente
uint32_t s_notifiedSeq = 0; // ultimo perfil de bebe por el que ya se aviso
uint16_t s_intervalDays = MNT_INTERVAL_DEFAULT_DAYS;
bool s_pendingBaby = false; // aviso de bebe nuevo sin atender

void putU32(const char *key, uint32_t v) {
  Preferences p;
  p.begin(HMI_NS_CFG, false);
  p.putUInt(key, v);
  p.end();
}

}  // namespace

void Maintenance_Init(void) {
  Preferences p;
  p.begin(HMI_NS_CFG, true);
  s_lastDone = p.getUInt(HMI_KEY_MNT_LAST, 0);
  s_snoozeUntil = p.getUInt(HMI_KEY_MNT_SNOOZE, 0);
  s_notifiedSeq = p.getUInt(HMI_KEY_MNT_SEQ, 0);
  s_intervalDays = p.getUShort(HMI_KEY_MNT_DAYS, MNT_INTERVAL_DEFAULT_DAYS);
  p.end();

  // Un valor de NVS que ya no esta en la lista (bajada de version, escritura
  // a medias) cae al de fabrica en vez de dejar el desplegable sin ninguna
  // opcion marcada.
  bool known = false;
  for (int i = 0; i < MNT_INTERVAL_COUNT; i++) {
    if (MNT_INTERVAL_DAYS[i] == s_intervalDays) known = true;
  }
  if (!known) s_intervalDays = MNT_INTERVAL_DEFAULT_DAYS;

  s_pendingBaby = false;
  ESP_LOGI(TAG, "init: last=%lu interval=%ud snooze=%lu seq=%lu",
           (unsigned long)s_lastDone, (unsigned)s_intervalDays,
           (unsigned long)s_snoozeUntil, (unsigned long)s_notifiedSeq);
}

void Maintenance_Tick(void) {
  const uint32_t now = HMI_GetEpochNow();

  // Primer arranque con hora valida: el plazo empieza a contar HOY. Sin esto
  // un equipo recien fabricado tendria s_lastDone = 0, o sea "vencido desde
  // 1970", y saldria el pop-up en la primera pantalla que viese el operador.
  if (now != 0 && s_lastDone == 0) {
    s_lastDone = now;
    putU32(HMI_KEY_MNT_LAST, s_lastDone);
    ESP_LOGI(TAG, "plazo sembrado en el primer arranque con hora: %lu",
             (unsigned long)now);
  }

  // Reloj hacia atras (alguien ajusta la fecha a mano y se equivoca de ano,
  // o la placa corrige un epoch disparatado): el registro quedaria en el
  // futuro y el plazo no volveria a cumplirse nunca. Se reancla a hoy.
  if (now != 0 && s_lastDone > now) {
    ESP_LOGW(TAG, "ultimo mantenimiento en el futuro (%lu > %lu): reanclado",
             (unsigned long)s_lastDone, (unsigned long)now);
    s_lastDone = now;
    putU32(HMI_KEY_MNT_LAST, s_lastDone);
  }
  if (now != 0 && s_snoozeUntil > now + SNOOZE_SECS) {
    s_snoozeUntil = 0;
    putU32(HMI_KEY_MNT_SNOOZE, 0);
  }

  // Bebe nuevo o cambio de bebe. `BabyWizard_GetActiveSeq()` es el perfil que
  // este HMI puso al mando (0 = ninguno), asi que un seq distinto de cero y
  // distinto del ultimo avisado es exactamente "hay otro paciente dentro".
  // El seq avisado se persiste para que un reinicio con el MISMO bebe no
  // vuelva a dar la lata.
  const uint32_t seq = BabyWizard_GetActiveSeq();
  if (seq != 0 && seq != s_notifiedSeq) {
    s_notifiedSeq = seq;
    putU32(HMI_KEY_MNT_SEQ, seq);
    s_pendingBaby = true;
    ESP_LOGI(TAG, "bebe nuevo (seq=%lu): recordatorio de limpieza pendiente",
             (unsigned long)seq);
  }
}

mnt_reason_t Maintenance_PendingReason(void) {
  if (s_intervalDays == MNT_INTERVAL_OFF) return MNT_REASON_NONE;

  const uint32_t now = HMI_GetEpochNow();
  if (now != 0 && s_snoozeUntil != 0 && now < s_snoozeUntil) {
    return MNT_REASON_NONE;
  }

  // El bebe manda sobre el plazo: es el motivo mas concreto y el que el
  // operador puede atender ahora mismo, antes de meter al paciente.
  if (s_pendingBaby) return MNT_REASON_NEW_BABY;

  if (now == 0 || s_lastDone == 0) return MNT_REASON_NONE;
  const uint32_t deadline = s_lastDone + (uint32_t)s_intervalDays * SECS_PER_DAY;
  return (now >= deadline) ? MNT_REASON_DUE : MNT_REASON_NONE;
}

void Maintenance_MarkDone(void) {
  const uint32_t now = HMI_GetEpochNow();
  s_pendingBaby = false;
  s_snoozeUntil = 0;
  putU32(HMI_KEY_MNT_SNOOZE, 0);
  if (now == 0) {
    // Sin reloj no hay fecha que registrar. Se calla el aviso (el operador ha
    // limpiado de verdad) pero el registro sigue vacio, y eso es lo honesto:
    // inventar una fecha seria peor que no tener ninguna.
    ESP_LOGW(TAG, "mantenimiento hecho sin hora valida: no se fecha");
    return;
  }
  s_lastDone = now;
  putU32(HMI_KEY_MNT_LAST, s_lastDone);
  ESP_LOGI(TAG, "mantenimiento registrado: %lu", (unsigned long)now);
}

void Maintenance_Snooze(void) {
  s_pendingBaby = false;
  const uint32_t now = HMI_GetEpochNow();
  if (now == 0) {
    // Sin reloj no se puede fechar el final del silencio. No hace falta: sin
    // hora el motivo del plazo no dispara, y el del bebe ya queda atendido
    // con el flag de arriba hasta que entre otro paciente.
    ESP_LOGI(TAG, "mas tarde sin hora valida: solo se calla el aviso del bebe");
    return;
  }
  s_snoozeUntil = now + SNOOZE_SECS;
  putU32(HMI_KEY_MNT_SNOOZE, s_snoozeUntil);
  ESP_LOGI(TAG, "mas tarde hasta %lu", (unsigned long)s_snoozeUntil);
}

uint32_t Maintenance_LastEpoch(void) { return s_lastDone; }

int32_t Maintenance_DaysSince(void) {
  const uint32_t now = HMI_GetEpochNow();
  if (now == 0 || s_lastDone == 0 || now < s_lastDone) return -1;
  return (int32_t)((now - s_lastDone) / SECS_PER_DAY);
}

void Maintenance_FormatLastLine(char *out, size_t cap) {
  char date[32];
  if (s_lastDone == 0) {
    snprintf(date, sizeof(date), "%s", TR(STR_MAINT_NEVER));
  } else {
    // Lo ALMACENADO sigue en UTC; la hora local solo se aplica al formatear
    // para una persona (mismo criterio que BabyHistory.cpp).
    time_t t = (time_t)(HMI_HasLocalTime() ? HMI_ToLocal(s_lastDone)
                                           : s_lastDone);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    snprintf(date, sizeof(date), "%04d-%02d-%02d", tmv.tm_year + 1900,
             tmv.tm_mon + 1, tmv.tm_mday);
  }
  snprintf(out, cap, TR(STR_MAINT_LAST_FMT), date);
}

uint16_t Maintenance_IntervalDays(void) { return s_intervalDays; }

void Maintenance_SetIntervalDays(uint16_t days) {
  if (days == s_intervalDays) return;
  s_intervalDays = days;
  {
    Preferences p;
    p.begin(HMI_NS_CFG, false);
    p.putUShort(HMI_KEY_MNT_DAYS, days);
    p.end();
  }
  s_snoozeUntil = 0;
  putU32(HMI_KEY_MNT_SNOOZE, 0);
  ESP_LOGI(TAG, "intervalo = %u dias", (unsigned)days);
}
