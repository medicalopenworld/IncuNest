#pragma once
// Decide si y como se pinta el aviso "Coge mi mano" de la familia en la
// pantalla de bloqueo. Sin LVGL ni Arduino a proposito, para probarlo en host.
//
// El aviso es suave: no modal, no se toca, no suena. Cede el sitio a todo lo
// que importa mas (alarmas, enlace caido, pulsioximetro, deslizador).
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum : uint8_t {
  FAMILY_VIEW_HIDDEN = 0,
  FAMILY_VIEW_TEXT = 1,
  FAMILY_VIEW_TEXT_QR = 2
} FamilyViewMode;

typedef struct {
  bool notice;            // la placa dice que el aviso esta activo
  bool hasUrl;            // hay URL y su QR pequeno se pudo codificar
  bool onLockScreen;      // la pantalla de bloqueo es la visible
  bool anyAlarm;          // UI_IsAnyAlarmActive()
  bool linkLost;          // Display_IsBoardLinkLost()
  bool pulseOxShown;      // ui_LockPulseOxFrame visible
  bool unlockSliderShown; // ui_UnlockCont visible
} FamilyViewInputs;

FamilyViewMode family_view_mode(const FamilyViewInputs *in);

// Entrada de ayuda con el QR: solo con URL guardada y enlace vivo. NO depende
// del aviso: es para que la enfermera ensene el QR a la madre al ingreso.
bool family_help_entry_visible(bool hasUrl, bool linkLost);

#ifdef __cplusplus
}
#endif
