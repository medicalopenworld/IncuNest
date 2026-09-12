#pragma once

// MODO DEPURACION DEL DISPLAY
//
// Hermano del de la motherBoard (motherBoard/src/modules/debug/debug_mode.h) y
// con sus mismas reglas: arranca apagado, no se persiste, y apagarlo retira
// todo lo simulado.
//
// Lo que aporta aqui y no alla: el display no mide nada, lo pinta. Todo lo que
// enseña le llega por el cable en lineas CTRL,*. Asi que simular un escenario
// en el display es INYECTAR ESAS LINEAS — con eso se reproduce cualquier
// estado de la placa (alarmas, telemetria, modo, fototerapia, PPG) sin tener la
// placa delante, y ademas se prueba el parseador de verdad, que es donde han
// estado los fallos.
//
// POR QUE LA INYECCION VA POR COLA Y NO POR LLAMADA DIRECTA: los manejadores
// del servidor web corren en la tarea de OTA, y parse_message() escribe
// ctrl_state_msg, alarmList y las banderas que consume la UI. Llamarlo desde
// otra tarea meteria una carrera con la tarea Comm justo en las estructuras que
// gobiernan lo que ve el operador. La linea se encola y la drena Comm_Task en
// el mismo punto en el que trata una linea real, asi que el contexto, el orden
// y el sello de latido son EXACTAMENTE los del camino de produccion.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool debug_mode_enabled(void);
void debug_mode_set(bool on);

// Encola una linea para que Comm_Task la trate como si hubiera llegado por el
// cable. Exige el modo encendido. Devuelve false si esta apagado, si la linea
// no empieza por el prefijo esperado o si la cola esta llena.
bool debug_inject_line(const char *line);

// Simula la perdida del enlace con la placa sin tocar el cable: mientras esta
// puesto, Display_IsBoardLinkLost() devuelve true pase lo que pase. Es la unica
// forma de probar el banner y el aviso audible de LINK LOST con las dos placas
// conectadas y funcionando.
void debug_link_mute_set(bool muted);
bool debug_link_mute_get(void);

// --- red: apagar por partes, en caliente ---------------------------------
//
// Existe para aislar de que parte de la red viene el temblor del panel RGB.
// El framebuffer vive en PSRAM y WiFi le roba ancho de banda y desactiva
// interrupciones; el foro del fabricante describe el sintoma. Con estos dos
// interruptores se prueba en el banco en segundos en vez de a golpe de
// recompilar:
//
//   tb    = publicacion periodica a ThingsBoard (5 s). Es el trafico de fondo.
//   radio = la radio entera. Apagarla deja el equipo SIN webserver ni OTA,
//           asi que es solo para el diagnostico.
//
// NO SE PERSISTEN: un reinicio devuelve las dos a encendido. Y a diferencia del
// resto del modo depuracion, estas NO exigen el modo encendido — apagar red
// nunca puede falsear una medida ni mover un actuador.
void debug_net_set_tb(bool enabled);
bool debug_net_tb_enabled(void);
void debug_net_set_radio(bool enabled);
bool debug_net_radio_enabled(void);

// Volcado de estado del display: enlace, pantalla, alarmas conocidas, ultima
// telemetria recibida, memoria y tareas. SOLO LECTURA, no depende del modo.
size_t debug_state_json(char *out, size_t out_len);
// Igual, pero con la tabla de tareas (pila minima por tarea). Va aparte porque
// construirla cuesta ancho de banda de PSRAM y recorre todas las TCB: en el
// display, pedirla en bucle produce flicker en el panel. Se pide solo cuando se
// esta diagnosticando.
size_t debug_state_json_ex(char *out, size_t out_len, bool with_tasks);

typedef enum {
  DEBUG_CRASH_NONE = 0,
  DEBUG_CRASH_ABORT,
  DEBUG_CRASH_NULL_DEREF,
  DEBUG_CRASH_STACK,
  DEBUG_CRASH_TASK_WDT,
  DEBUG_CRASH_ASSERT,
} debug_crash_kind_t;

debug_crash_kind_t debug_crash_from_name(const char *name);
bool debug_crash_request(debug_crash_kind_t kind, uint32_t delay_ms);

#ifdef __cplusplus
} // extern "C"
#endif
