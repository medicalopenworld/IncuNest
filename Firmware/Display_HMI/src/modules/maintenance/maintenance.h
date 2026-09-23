#pragma once
// Recordatorio de limpieza y mantenimiento de la incubadora.
//
// Aqui vive solo la decision de CUANDO toca cada nivel; el pop-up que lo
// cuenta y el QR de los tutoriales estan en `ui/MaintenanceDialog.cpp`. Sin
// LVGL a proposito, igual que `modules/support/support_report.cpp`.
//
// El protocolo tiene TRES niveles, cada uno con su cadencia propia. No son
// configurables: son el protocolo de limpieza del equipo, no una preferencia.
// Lo unico que se elige en Ajustes es si el equipo avisa o no.
//
//   DIARIA    con paciente dentro
//   SEMANAL   cada 7 dias, o antes si hay suciedad visible
//   TERMINAL  al alta del paciente, o cada 7 dias si la estancia es prolongada
//
// Los niveles se contienen: una limpieza terminal es mas profunda que una
// semanal, y esta que una diaria. Por eso registrar un nivel pone tambien al
// dia los de debajo — si acabas de hacer la terminal, que el equipo siga
// pidiendo la diaria seria ruido.
//
// Todo el estado se persiste en NVS (`HMI_NS_CFG`, claves `mnt_*`), asi que un
// reinicio no pierde ninguna fecha ni repite el aviso del mismo paciente.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Niveles, de menos a mas profundo. El orden importa: registrar uno pone al
// dia todos los de indice menor.
typedef enum {
  MNT_LEVEL_DAILY = 0,
  MNT_LEVEL_WEEKLY,
  MNT_LEVEL_TERMINAL,
  MNT_LEVEL_COUNT,
} mnt_level_t;

// Carga el estado de NVS. Llamar una vez en el arranque de UI_Task, antes del
// primer `Maintenance_Tick()` y antes de `UI_ApplyLanguage()` (que pinta el
// estado del interruptor de Ajustes).
void Maintenance_Init(void);

// Una vez por vuelta de UI_Task. Vigila el alta del paciente, siembra las
// fechas en el primer arranque con hora valida y corrige el reloj hacia
// atras. No pinta nada.
void Maintenance_Tick(void);

// Interruptor unico de Ajustes. Desactivado, NO avisa de ningun nivel: quien
// lo apaga no quiere unos avisos y no otros. Las fechas se siguen guardando.
bool Maintenance_IsEnabled(void);
void Maintenance_SetEnabled(bool on);

// True si a ese nivel le toca ya, mire quien mire (sin aplicar el interruptor
// ni el "mas tarde"): es lo que el pop-up marca como TOCA AHORA.
bool Maintenance_IsDue(mnt_level_t lvl);

// True si hay que sacar el aviso ahora mismo: algun nivel vencido, con el
// recordatorio activado y sin un "mas tarde" vigente.
bool Maintenance_ShouldWarn(void);

// El operador ha registrado ese nivel. Guarda la fecha de hoy en el y en los
// menos profundos, borra el "mas tarde" y, si es el terminal, da por atendida
// el alta pendiente. Sin hora valida no puede fechar nada: entonces solo
// calla el aviso (y se vera "sin registrar" hasta que haya reloj).
void Maintenance_MarkDone(mnt_level_t lvl);

// Boton MAS TARDE: calla el aviso 24 h, sea del nivel que sea.
void Maintenance_Snooze(void);

// Epoch UTC del ultimo registro de ese nivel, 0 si no hay ninguno.
uint32_t Maintenance_LastEpoch(mnt_level_t lvl);

// "Ultima: 2026-08-01" en el idioma activo, o "Ultima: sin registrar". La
// fecha se formatea en hora local (lo almacenado sigue siendo UTC). Lo pintan
// igual el pop-up y el panel de Ajustes, asi que vive aqui.
void Maintenance_FormatLastLine(mnt_level_t lvl, char *out, size_t cap);
