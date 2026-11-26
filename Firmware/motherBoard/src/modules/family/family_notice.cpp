#include "family_notice.h"

#include <math.h>
#include <string.h>

// Unico lector del estado activo: al vencer el plazo BORRA la bandera. Sin eso,
// tras ~24,8 dias de millis() (2^31 ms) la resta con signo volveria a dar
// positivo y un aviso vencido reaparecería sin un nuevo hand_hold_until.
static bool is_active(FamilyNotice *f, uint32_t nowMs) {
  if (f->active && (int32_t)(f->deadlineMs - nowMs) <= 0) f->active = false;
  return f->active;
}

void family_notice_init(FamilyNotice *f) {
  memset(f, 0, sizeof *f);
}

void family_notice_on_hand_hold(FamilyNotice *f, double untilEpochMs, bool fromRequest,
                                uint32_t nowEpochS, bool clockFromNetwork, uint32_t nowMs) {
  if (!isfinite(untilEpochMs) || untilEpochMs <= 0.0) return;
  // ThingsBoard reenvia el ultimo valor en cada conexion: igual o anterior al
  // ultimo aplicado no cuenta nunca.
  is_active(f, nowMs); // normaliza un plazo vencido antes de tocar el estado
  if (untilEpochMs <= f->lastApplied) return;
  // Se apunta ANTES de decidir, para que un duplicado posterior tampoco cuente
  // aunque este se ignore (hora no fiable, o ya pasado).
  f->lastApplied = untilEpochMs;

  const bool trusted = clockFromNetwork && nowEpochS != 0;
  uint32_t holdMs;
  if (trusted) {
    const double remainingMs = untilEpochMs - (double)nowEpochS * 1000.0;
    if (remainingMs < 1.0) return;
    const double capMs = (double)FAMILY_HOLD_MAX_S * 1000.0;
    holdMs = (uint32_t)(remainingMs > capMs ? capMs : floor(remainingMs));
  } else if (fromRequest) {
    return; // sin hora de red, una respuesta de reconexion no vale
  } else {
    holdMs = FAMILY_HOLD_UNTRUSTED_S * 1000u;
  }
  f->active = true;
  f->deadlineMs = nowMs + holdMs;
}

void family_notice_set_url(FamilyNotice *f, const char *url) {
  if (url != NULL && family_url_valid(url)) {
    strncpy(f->url, url, FAMILY_URL_MAX);
    f->url[FAMILY_URL_MAX] = '\0';
  } else {
    f->url[0] = '\0';
  }
}

FamilyUrlAction family_notice_url_action(bool keyPresent, bool isString, bool fromRequest) {
  if (keyPresent) return isString ? FAMILY_URL_SET : FAMILY_URL_CLEAR;
  return fromRequest ? FAMILY_URL_CLEAR : FAMILY_URL_KEEP;
}

void family_notice_on_discharge(FamilyNotice *f) {
  f->active = false;
  f->url[0] = '\0';
}

bool family_notice_take_msg(FamilyNotice *f, uint32_t nowMs, bool force, FamilyMsg *out) {
  FamilyMsg cur;
  memset(&cur, 0, sizeof cur);
  cur.notice = is_active(f, nowMs);
  strncpy(cur.url, f->url, FAMILY_URL_MAX);

  const bool changed = !f->everSent || cur.notice != f->lastSent.notice ||
                       strcmp(cur.url, f->lastSent.url) != 0;
  if (!force && !changed) return false;

  f->lastSent = cur;
  f->everSent = true;
  *out = cur;
  return true;
}
