#include "modules/maintenance/weigh_tracker.h"

#include <Preferences.h>

#include "CommTask.h"        // HMI_GetEpochNow()
#include "EEPROM_defines.h"  // HMI_NS_CFG, HMI_KEY_WGH_*
#include "esp_log.h"
#include "modules/maintenance/weigh_reminder.h"
#include "state/training_mode.h"  // Training_IsActive(), Training_IsPracticeSeq()
#include "ui/BabyWizard.h"        // BabyWizard_GetActiveSeq(), HasLiveSession()

namespace {

constexpr const char *TAG = "WGH";

uint32_t s_weighSeq = 0;
uint32_t s_weighEpoch = 0;
uint32_t s_seenSeq = 0;
uint32_t s_seenEpoch = 0;
uint32_t s_snoozeUntil = 0;

// Ultimo bebe al mando visto por Tick(); solo en RAM. Al arrancar es 0, asi
// que el primer seq tras un reinicio no cuenta como "cambio de bebe" y el sello
// guardado de ese mismo bebe se conserva.
uint32_t s_trackedSeq = 0;
// Pesada enviada sin hora: se sella con la primera hora valida. Mientras
// tanto no se avisa. Solo en RAM: sin hora y con un reinicio en medio, se
// conserva el sello anterior y, como mucho, sale un aviso de mas.
uint32_t s_pendingSeq = 0;
bool s_dirty = false;

void markDirty() { s_dirty = true; }

weigh_inputs_t snapshot() {
  weigh_inputs_t in = {};
  in.nowEpoch = HMI_GetEpochNow();
  in.activeSeq = BabyWizard_GetActiveSeq();
  in.liveSession = BabyWizard_HasLiveSession();
  in.weighSeq = s_weighSeq;
  in.weighEpoch = s_weighEpoch;
  in.seenSeq = s_seenSeq;
  in.seenEpoch = s_seenEpoch;
  in.snoozeUntil = s_snoozeUntil;
  return in;
}

void clearAll() {
  s_weighSeq = s_weighEpoch = 0;
  s_seenSeq = s_seenEpoch = 0;
  s_snoozeUntil = 0;
  s_pendingSeq = 0;
  markDirty();
}

}  // namespace

void WeighTracker_Init(void) {
  Preferences p;
  if (!p.begin(HMI_NS_CFG, true)) {
    ESP_LOGW(TAG, "init: no se pudo abrir la NVS, estado vacio");
    return;
  }
  s_weighSeq = p.getUInt(HMI_KEY_WGH_SEQ, 0);
  s_weighEpoch = p.getUInt(HMI_KEY_WGH_LAST, 0);
  s_seenSeq = p.getUInt(HMI_KEY_WGH_SEEN_SEQ, 0);
  s_seenEpoch = p.getUInt(HMI_KEY_WGH_SEEN_AT, 0);
  s_snoozeUntil = p.getUInt(HMI_KEY_WGH_SNOOZE, 0);
  p.end();
  ESP_LOGI(TAG, "init: weigh seq=%lu at=%lu seen seq=%lu at=%lu snooze=%lu",
           (unsigned long)s_weighSeq, (unsigned long)s_weighEpoch,
           (unsigned long)s_seenSeq, (unsigned long)s_seenEpoch,
           (unsigned long)s_snoozeUntil);
}

void WeighTracker_Tick(void) {
  // El bebe al mando en formacion es de practica (ZOE o el de la leccion): ni
  // es un paciente real ni debe escribirse en NVS. Va ANTES de leer nada.
  if (Training_IsActive()) return;

  const uint32_t now = HMI_GetEpochNow();
  const uint32_t seq = BabyWizard_GetActiveSeq();

  if (seq != s_trackedSeq) {
    if (seq == 0) {
      // Alta del bebe (o perfil olvidado): no queda nada que recordar.
      ESP_LOGI(TAG, "alta del bebe %lu: sello borrado",
               (unsigned long)s_trackedSeq);
      clearAll();
    } else {
      // Bebe nuevo al mando: lo que no sea suyo se descarta. El "mas tarde"
      // solo se conserva si ya conociamos a este bebe (mismo bebe tras un
      // reinicio del HMI).
      const bool known = (seq == s_weighSeq) || (seq == s_seenSeq);
      if (s_weighSeq != seq) s_weighSeq = s_weighEpoch = 0;
      if (s_seenSeq != seq) s_seenSeq = s_seenEpoch = 0;
      if (!known) s_snoozeUntil = 0;
      if (s_pendingSeq != seq) s_pendingSeq = 0;
      markDirty();
      ESP_LOGI(TAG, "bebe al mando: %lu (conocido=%d)", (unsigned long)seq,
               (int)known);
    }
    s_trackedSeq = seq;
  }

  if (now == 0) return;

  // Pesada enviada sin hora: ya hay reloj, se sella ahora.
  if (s_pendingSeq != 0) {
    if (s_pendingSeq == seq) {
      s_weighSeq = seq;
      s_weighEpoch = now;
      s_snoozeUntil = 0;
      markDirty();
      ESP_LOGI(TAG, "pesada pendiente sellada: %lu", (unsigned long)now);
    }
    s_pendingSeq = 0;
  }

  // Primera vez que el HMI ve a este bebe al mando (el ACK de seleccion o
  // alta es lo que mueve BabyWizard_GetActiveSeq()). Es la referencia de un
  // bebe sin ninguna pesada sellada.
  if (seq != 0 && s_seenSeq != seq) {
    s_seenSeq = seq;
    s_seenEpoch = now;
    markDirty();
    ESP_LOGI(TAG, "bebe %lu visto por primera vez: %lu", (unsigned long)seq,
             (unsigned long)now);
  }

  // Reloj hacia atras: lo guardado quedaria en el futuro y el plazo no se
  // cumpliria hasta que el reloj lo alcanzase. Se reancla a hoy.
  if (s_weighEpoch > now) {
    s_weighEpoch = now;
    markDirty();
  }
  if (s_seenEpoch > now) {
    s_seenEpoch = now;
    markDirty();
  }
  if (s_snoozeUntil > now + WEIGH_REMINDER_SNOOZE_SECS) {
    s_snoozeUntil = 0;
    markDirty();
  }
}

void WeighTracker_NoteWeighed(uint32_t seq, uint16_t grams) {
  // 0 g es SKIP ("no hay peso"), no una pesada.
  if (seq == 0 || grams == 0) return;
  // En formacion no se escribe nada (ADR-0002). Este es el unico punto de
  // sello, asi que la guarda vive aqui y no en cada llamador.
  if (Training_IsActive() || Training_IsPracticeSeq(seq)) return;
  // Un unico hueco de sello: pesar a un bebe que no esta al mando (alta desde
  // la pantalla Bebes) no debe pisar el del que si lo esta.
  if (seq != BabyWizard_GetActiveSeq()) {
    ESP_LOGI(TAG, "pesada del bebe %lu, que no esta al mando: no se sella",
             (unsigned long)seq);
    return;
  }

  s_snoozeUntil = 0;  // registrar un peso cancela el "mas tarde"
  const uint32_t now = HMI_GetEpochNow();
  if (now == 0) {
    // Sin hora no se puede fechar; se sella en cuanto la haya.
    s_pendingSeq = seq;
    ESP_LOGW(TAG, "pesada sin hora valida: se sella cuando la haya");
  } else {
    s_weighSeq = seq;
    s_weighEpoch = now;
    s_pendingSeq = 0;
    ESP_LOGI(TAG, "pesada sellada: bebe %lu a las %lu", (unsigned long)seq,
             (unsigned long)now);
  }
  markDirty();
}

bool WeighTracker_ShouldWarn(void) {
  if (s_pendingSeq != 0) return false;  // acaba de pesarse, falta fecharlo
  const weigh_inputs_t in = snapshot();
  return weigh_reminder_should_warn(&in);
}

uint32_t WeighTracker_ElapsedHours(void) {
  const weigh_inputs_t in = snapshot();
  return weigh_elapsed_hours(&in);
}

void WeighTracker_Snooze(void) {
  const uint32_t until = weigh_snooze_until(HMI_GetEpochNow());
  if (until == 0) {
    ESP_LOGI(TAG, "mas tarde sin hora valida: no se puede fechar el silencio");
    return;
  }
  s_snoozeUntil = until;
  markDirty();
  ESP_LOGI(TAG, "mas tarde hasta %lu", (unsigned long)until);
}

void WeighTracker_Flush(void) {
  if (!s_dirty) return;
  Preferences p;
  if (!p.begin(HMI_NS_CFG, false)) {
    // Se queda sucio y se reintenta en la siguiente vuelta.
    ESP_LOGW(TAG, "flush: no se pudo abrir la NVS");
    return;
  }
  // Un cuarteto de escrituras de 4 bytes; si alguna falla se deja sucio.
  bool ok = true;
  ok &= p.putUInt(HMI_KEY_WGH_SEQ, s_weighSeq) == sizeof(uint32_t);
  ok &= p.putUInt(HMI_KEY_WGH_LAST, s_weighEpoch) == sizeof(uint32_t);
  ok &= p.putUInt(HMI_KEY_WGH_SEEN_SEQ, s_seenSeq) == sizeof(uint32_t);
  ok &= p.putUInt(HMI_KEY_WGH_SEEN_AT, s_seenEpoch) == sizeof(uint32_t);
  ok &= p.putUInt(HMI_KEY_WGH_SNOOZE, s_snoozeUntil) == sizeof(uint32_t);
  p.end();
  if (ok) {
    s_dirty = false;
  } else {
    ESP_LOGW(TAG, "flush: escritura NVS fallida, se reintenta");
  }
}
