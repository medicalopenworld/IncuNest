#pragma once
// Menu del heading de ui_ScreenMain (boton azul de tres rayas, ui_MenuButton).
// Agrupa lo que antes ocupaba la cabecera con un boton cada uno: ajuste de
// fecha y hora, bebes registrados, ajustes y ayuda. Es solo un selector: cada
// tarjeta cierra el menu y abre el dialogo o la pantalla que ya existia
// (TimeDialog, BabyHistory, ui_ScreenSettings, HelpDialog).
#include <lvgl.h>

// Inactividad tras la que el menu se cierra solo. El auto-bloqueo (20 s) suele
// llegar antes y lo cierra igualmente: el menu solo vive sobre ui_ScreenMain.
#define MENU_IDLE_TIMEOUT_MS (60UL * 1000UL)

// Tarjetas del menu. Persistentes (no se recrean al abrir) para que las
// lecciones de formacion puedan apuntarlas con un puntero estable.
extern lv_obj_t *ui_MenuTimeBtn;
extern lv_obj_t *ui_MenuBabiesBtn;
extern lv_obj_t *ui_MenuSettingsBtn;
extern lv_obj_t *ui_MenuHelpBtn;

void MenuDialog_Init(lv_obj_t *parent);
// Lo llama el boton del heading (ui_MenuButton).
void MenuDialog_Open(void);
bool MenuDialog_IsOpen(void);
void MenuDialog_Close(void);
// Cada pasada del bucle de UI: cierre por inactividad o si se cambia de
// pantalla (auto-bloqueo).
void MenuDialog_Poll(void);
