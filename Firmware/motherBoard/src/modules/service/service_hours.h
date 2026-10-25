#pragma once
// Horas desde servicio de cada actuador: la parte que toca el equipo (NVS e
// `in3`). El calculo esta en service_calc.h y el formato de las lineas en
// shared/include/service_hours_proto.h.
//
// Hilos: ServiceHours_Init() corre en setup, antes de las tareas. MarkDone()
// lo llama Comm_Task. Las bases son floats de 32 bits (lectura atomica en el
// ESP32) que solo escribe MarkDone, y las leen Comm_Task y las tareas de
// telemetria; no hace falta mutex (mismo criterio que los contadores in3.*).
#include <stdbool.h>
#include <stdint.h>

#include "service_hours_proto.h"

// Carga las bases de NVS. Llamar DESPUES de cargar los contadores de vida.
void ServiceHours_Init(void);

// Informe de los cuatro actuadores con los valores de ahora mismo.
void ServiceHours_BuildReport(ServiceReport *out);

// Horas (enteras) desde servicio de un actuador; 0 si el id no existe. Lo
// usa la telemetria.
uint32_t ServiceHours_SinceHours(uint8_t id);

// Registra el servicio: la base pasa a ser el contador de vida actual y se
// guarda el epoch (0 si no hay hora valida). Devuelve false (sin tocar nada)
// si el id no existe o si falla la escritura en NVS.
bool ServiceHours_MarkDone(uint8_t id, uint32_t epochUtc);
