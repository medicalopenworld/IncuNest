#pragma once
// Pop-up del recordatorio de limpieza y mantenimiento.
//
// Cuando toca avisar lo decide `modules/maintenance/maintenance.h`; aqui solo
// se cuenta. Mismo patron que HelpDialog: overlay reutilizado sobre
// ui_ScreenMain, tarjeta de 780x460 y contenido reconstruido en cada apertura.
//
// A la izquierda, el QR de SUPPORT_TUTORIAL_URL, el mismo que la vista "Video
// tutorial" del menu de ayuda: los tutoriales de limpieza viven en esa misma
// pagina, y el operador ya conoce ese codigo.
//
// Dos salidas y ninguna X: MANTENIMIENTO HECHO (registra la fecha y reinicia
// el plazo) y MAS TARDE (calla el aviso 24 h). Un recordatorio que se puede
// cerrar sin contestar no deja constancia de nada.
#include <lvgl.h>

// Igual que la ayuda y la formacion, el pop-up esta exento del auto-bloqueo de
// 20 s mientras esta abierto, con el mismo tope de 3 min sin ningun toque:
// leer el QR con el movil lleva mas de 20 s, pero un aviso olvidado no puede
// quedarse horas tapando la pantalla (el banner de alarma solo se pinta en
// ui_ScreenLock).
//
// El tope se mide con `lv_disp_get_inactive_time()`, que es fiable porque la
// exencion de `inactivity_timer_cb()` NO llama a `lv_disp_trig_activity()`
// para estos overlays — si lo hiciera cada 200 ms, el tope no llegaria nunca.
#define MNT_IDLE_TIMEOUT_MS (3UL * 60UL * 1000UL)

// Crea overlay/tarjeta (ocultos). Llamar una vez durante la init de UI, con
// ui_ScreenMain como parent explicito (nunca lv_scr_act(): en ese momento la
// pantalla activa sigue siendo el splash).
void MaintenanceDialog_Init(lv_obj_t *parent);

// Arma el aviso: en la siguiente vuelta de `_Poll()`, si hay motivo
// pendiente, se abre el pop-up. Lo llama el desbloqueo de la pantalla (y una
// vez el arranque de UI): el aviso sale cuando alguien acaba de coger el
// equipo, no en medio de una maniobra.
void MaintenanceDialog_NoteUnlocked(void);

// Verdadero mientras el pop-up este visible. Lo consulta el temporizador de
// inactividad para no mandar la pantalla al bloqueo con el aviso abierto.
bool MaintenanceDialog_IsOpen(void);

// Abre el pop-up sin esperar a que toque nada. Lo llama el boton VER
// RECORDATORIO de Ajustes > MANTENIMIENTO: es la via para registrar una
// limpieza hecha por iniciativa propia, sin duplicar aqui los tres botones.
// Abierto asi, el boton de abajo es CERRAR y no MAS TARDE (no hay nada que
// aplazar).
void MaintenanceDialog_Open(void);

// Una vez por vuelta de UI_Task, dentro de LVGL_Lock(): vigila el cambio de
// bebe, abre el aviso cuando esta armado y toca, y lo cierra por alarma,
// enlace perdido o tope de inactividad.
void MaintenanceDialog_Poll(void);
