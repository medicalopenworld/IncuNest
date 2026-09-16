#pragma once

// MODO DEPURACION DE LA MOTHERBOARD
//
// Para que existe: probar en banco escenarios que de otra forma exigen romper
// hardware. Desenchufar la sonda de aire para ver la alarma de sensor es
// factible; provocar una subtension de alimentacion, un fallo de calefactor o
// una obstruccion de la salida de aire, no. Sin una forma de simularlos, esas
// ramas de securityCheck() solo se ejercitan en produccion.
//
// ================== LO QUE ESTE MODULO NO PUEDE SER ==================
// Esto va en un equipo que calienta a un recien nacido. Un modo de pruebas que
// se pueda quedar puesto sin que nadie lo note es peor que no tenerlo. Por eso:
//
//  1. ARRANCA SIEMPRE APAGADO. No se persiste en NVS a proposito. Un reinicio
//     —incluido el del watchdog— devuelve la placa a la realidad.
//  2. APAGARLO RETIRA TODAS LAS SIMULACIONES de golpe (debug_mode_set(false)).
//     No hay forma de quedarse con "el modo apagado pero el aire falseado".
//  3. MIENTRAS ESTA ENCENDIDO LO DICE. La placa emite el aviso por el enlace
//     para que el display lo pinte, y lo repite en el log.
//  4. Las simulaciones NO tocan la maquina de alarmas por la puerta de atras:
//     inyectan la MEDIDA, y securityCheck() decide. Asi lo que se prueba es la
//     logica de verdad y no una maqueta de ella. La unica excepcion explicita
//     es debug_alarm_force(), que existe para ejercitar el display y la
//     telemetria con condiciones que ninguna medida puede reproducir.
//
// Transporte: los endpoints /debug/* del servidor web (Wifi_OTA.cpp), con la
// misma autenticacion que /config. Se eligio ese y no la consola porque la
// UART0 de esta placa la comparte el cable del display.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- interruptor general -------------------------------------------------
bool debug_mode_enabled(void);
// Apagarlo limpia TODAS las sobreescrituras y alarmas forzadas.
void debug_mode_set(bool on);

// --- sobreescritura de medidas -------------------------------------------
// Cada canal se falsea por separado; el resto sigue viniendo del sensor real.
typedef enum {
  DEBUG_CH_AIR_TEMP = 0,   // in3.temperature[ROOM_DIGITAL_TEMP_SENSOR]
  DEBUG_CH_AIR_TEMP_RED,   // in3.airTemperatureRedundantSensor
  DEBUG_CH_SKIN_TEMP,      // in3.temperature[SKIN_SENSOR]
  DEBUG_CH_AMBIENT_TEMP,   // in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR]
  DEBUG_CH_HUMIDITY,       // in3.humidity[ROOM_DIGITAL_HUM_SENSOR]
  DEBUG_CH_FAN_RPM,        // in3.fan_rpm
  DEBUG_CH_SYSTEM_VOLTAGE, // in3.system_voltage
  DEBUG_CH_HEATER_CURRENT, // in3.heater_current
  DEBUG_CH_FAN_CURRENT,    // in3.fan_current
  DEBUG_CH_COUNT
} debug_channel_t;

// Nombre corto del canal, el que aceptan los endpoints y sale en el JSON.
const char *debug_channel_name(debug_channel_t ch);
// Devuelve DEBUG_CH_COUNT si el nombre no existe.
debug_channel_t debug_channel_from_name(const char *name);

bool debug_override_set(debug_channel_t ch, double value);
void debug_override_clear(debug_channel_t ch);
void debug_override_clear_all(void);
bool debug_override_active(debug_channel_t ch);

// Pisa las medidas falseadas sobre in3. La llama sensors_Task en cada pasada,
// DESPUES de que los sensores reales hayan escrito: asi el valor simulado es
// el que ven el control y securityCheck(), y al retirar la simulacion la
// medida real vuelve sola en la pasada siguiente. No hace nada con el modo
// apagado.
void debug_sensors_apply(void);

// --- alarmas forzadas ----------------------------------------------------
// Fuerza la CONDICION (el `present` de la maquina), no la senal: el retardo de
// anuncio, la prioridad, el enclavamiento y el corte de calefactor siguen
// siendo los de produccion. present=false deja de forzarla y devuelve la
// alarma a lo que diga la medida.
bool debug_alarm_force(int alarm_id, bool present);
void debug_alarm_clear_all(void);
// true si esta alarma esta forzada ahora mismo.
bool debug_alarm_forced(int alarm_id);
// La llama securityCheck() al final, para que lo forzado gane a lo medido.
void debug_alarms_apply(uint32_t now_ms);

// --- inyeccion de tramas del display -------------------------------------
// Encola una linea "HMI,..." para que la trate CommTask como si hubiera
// llegado por el cable. Es el espejo de lo que hace el display, y por el mismo
// motivo: con ella se reproduce cualquier ORDEN del operador —encender la
// actuacion, cambiar consignas, silenciar, reconocer una alarma— sin tener el
// display delante, y se ejercita el parseador de verdad en vez de una maqueta.
//
// Exige el modo encendido. Devuelve false si esta apagado, si la linea no
// lleva el prefijo esperado o si la cola esta llena.
//
// VA POR COLA, y la drena CommTask: los manejadores del servidor web corren en
// la tarea de OTA, y parse_line() toca g_last_cmd, la maquina de alarmas y el
// arranque de los PID. Llamarlo desde otra tarea seria meter una carrera justo
// en lo que gobierna los actuadores.
bool debug_inject_line(const char *line);

// --- volcado de estado ---------------------------------------------------
// JSON con medidas, actuadores, alarmas, PID, salud de tareas y memoria. Es de
// SOLO LECTURA y no depende del modo: observar nunca cambia nada, y poder
// mirar por dentro una unidad que se esta portando mal es justo lo que hacia
// falta. Devuelve los bytes escritos (sin el '\0').
size_t debug_state_json(char *out, size_t out_len);
// Igual, pero con la tabla de tareas (pila minima por tarea). Va aparte porque
// construirla cuesta ancho de banda de PSRAM y recorre todas las TCB: en el
// display, pedirla en bucle produce flicker en el panel. Se pide solo cuando se
// esta diagnosticando.
size_t debug_state_json_ex(char *out, size_t out_len, bool with_tasks);

// --- provocacion de fallo (prueba de coredump) ---------------------------
typedef enum {
  DEBUG_CRASH_NONE = 0,
  DEBUG_CRASH_ABORT,      // abort() -> panic
  DEBUG_CRASH_NULL_DEREF, // LoadProhibited
  DEBUG_CRASH_STACK,      // desbordamiento de pila
  DEBUG_CRASH_TASK_WDT,   // bucle sin alimentar el watchdog
  DEBUG_CRASH_ASSERT,     // assert() fallido
} debug_crash_kind_t;

debug_crash_kind_t debug_crash_from_name(const char *name);
// Lanza el fallo en una tarea aparte tras `delay_ms`, para que la peticion HTTP
// pueda contestar antes. EXIGE el modo encendido. Devuelve false si no.
bool debug_crash_request(debug_crash_kind_t kind, uint32_t delay_ms);

#ifdef __cplusplus
} // extern "C"
#endif
