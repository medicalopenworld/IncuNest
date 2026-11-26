#pragma once
// Aviso "Coge mi mano" para la familia, en la pantalla de bloqueo
// (Firmware/PROTOCOL.md, "CTRL,FAMILY"). Franja inferior de ui_ScreenLock, la
// del pulsioximetro: texto y, si hay URL valida, un QR. No es modal, no se
// toca (el toque llega a ui_ScreenLock y abre el deslizador) y no suena.
//
// Las reglas de cuando se pinta viven en modules/family/family_view.cpp.
#include <lvgl.h>

// Crea la tarjeta (oculta) dentro de `lockScreen`. Una vez, durante la init de
// UI, justo despues de crear ui_LockVitCont.
void FamilyNotice_Init(lv_obj_t *lockScreen);

// Consume CTRL,FAMILY (g_pendingFamily), recalcula el modo y solo toca los
// widgets si algo cambio (modo, URL o idioma). Una vez por vuelta de UI_Task,
// dentro de LVGL_Lock().
void FamilyNotice_Poll(void);

// Ultima URL valida recibida, haya aviso o no; false/"" antes de recibir nada
// y con el enlace con la placa perdido (al perderlo se olvida). Lo usa la
// tarjeta Familia de la ayuda.
bool Family_HasUrl(void);
const char *Family_Url(void);
