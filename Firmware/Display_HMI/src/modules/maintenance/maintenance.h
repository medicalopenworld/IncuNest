#pragma once
// Recordatorio de mantenimiento (limpieza y desinfeccion) de la incubadora.
//
// Aqui vive solo la decision de CUANDO hay que recordar; el pop-up que lo
// cuenta y el QR de los tutoriales estan en `ui/MaintenanceDialog.cpp`. Sin
// LVGL a proposito, igual que `modules/support/support_report.cpp`.
//
// Dos motivos independientes para recordar, los dos que pidio el usuario:
//
//   - PLAZO: han pasado `Maintenance_IntervalDays()` dias desde el ultimo
//     mantenimiento registrado. Es tiempo de CALENDARIO (epoch de la placa,
//     `HMI_GetEpochNow()`), no horas de funcionamiento: una incubadora
//     apagada un mes tambien necesita limpieza antes de volver a usarse.
//   - BEBE NUEVO: el HMI ha puesto al mando un perfil distinto del ultimo
//     por el que se aviso (`BabyWizard_GetActiveSeq()`), o sea alta de un
//     paciente nuevo o cambio de un bebe a otro. Es la limpieza entre
//     pacientes y no depende del plazo.
//
// Todo el estado se persiste en NVS (`HMI_NS_CFG`, claves `mnt_*`), asi que
// un reinicio no pierde el plazo ni repite el aviso del mismo bebe.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Motivo por el que toca avisar. NONE = no toca.
typedef enum {
  MNT_REASON_NONE = 0,
  MNT_REASON_DUE,       // se cumplio el plazo
  MNT_REASON_NEW_BABY,  // bebe nuevo o cambio de bebe
} mnt_reason_t;

// Intervalos ofrecidos en Ajustes, en dias. El 0 apaga el recordatorio por
// completo (ningun motivo dispara, tampoco el del bebe: si alguien lo apaga
// es que no quiere avisos, no que quiera unos y no otros).
#define MNT_INTERVAL_OFF 0
extern const uint16_t MNT_INTERVAL_DAYS[];
#define MNT_INTERVAL_COUNT 5
// Valor de fabrica: un mes. Es el plazo con el que se desplego el equipo y el
// que se puede cambiar por centro desde Ajustes > MANTENIMIENTO.
#define MNT_INTERVAL_DEFAULT_DAYS 30

// Carga el estado de NVS. Llamar una vez en el arranque de UI_Task, antes del
// primer `Maintenance_Tick()`.
void Maintenance_Init(void);

// Una vez por vuelta de UI_Task. Vigila el cambio de bebe, siembra el plazo
// en el primer arranque con hora valida y corrige el reloj hacia atras. No
// pinta nada.
void Maintenance_Tick(void);

// Motivo pendiente ahora mismo, ya con el "mas tarde" aplicado. El pop-up
// pregunta esto en cada vuelta.
mnt_reason_t Maintenance_PendingReason(void);

// El operador ha registrado el mantenimiento (boton MANTENIMIENTO HECHO).
// Guarda la fecha de hoy, borra el "mas tarde" y da por avisado al bebe
// actual. Sin hora valida no puede fechar nada: entonces solo calla el aviso
// (y se vera "sin registrar" hasta que haya reloj y se vuelva a registrar).
void Maintenance_MarkDone(void);

// Boton MAS TARDE: calla los avisos 24 h (o hasta el proximo bebe distinto).
void Maintenance_Snooze(void);

// Epoch UTC del ultimo mantenimiento registrado, 0 si no hay ninguno.
uint32_t Maintenance_LastEpoch(void);

// Dias enteros transcurridos desde el ultimo mantenimiento. -1 cuando no se
// puede saber (sin registro previo o sin hora).
int32_t Maintenance_DaysSince(void);

// "Ultimo mantenimiento: 2026-08-01" en el idioma activo, o "... sin
// registrar" cuando no hay ninguno. La fecha se formatea en hora local (lo
// almacenado sigue siendo UTC). Lo pintan igual el pop-up y la fila de
// Ajustes, asi que vive aqui y no en cada uno.
void Maintenance_FormatLastLine(char *out, size_t cap);

// Intervalo vigente en dias (0 = apagado).
uint16_t Maintenance_IntervalDays(void);

// Cambia el intervalo y lo persiste. Un cambio de intervalo borra el "mas
// tarde": el operador acaba de decidir cada cuanto quiere el aviso, y la
// respuesta a la pregunta anterior ya no vale.
void Maintenance_SetIntervalDays(uint16_t days);
