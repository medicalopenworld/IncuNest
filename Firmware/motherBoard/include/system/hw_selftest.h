#pragma once
// Expone al modulo factory_test (design.md D4, shared-factory-test) el
// autotest de arranque de src/system/initHardware.cpp, que hasta ahora se
// enlazaba por declaracion implicita (ni siquiera main.h lo declaraba). No se
// toca initHardware.cpp: son las mismas funciones y la misma variable, con
// prototipo explicito en vez de implicito.

// Bitmask de errores de hardware (HW_ERROR_ID, include/main.h:172). El test
// de fabrica compara su valor antes/despues de llamar a actuatorsTest() /
// testStandByCurrent() para saber si esa llamada concreta anadio algun bit.
extern long HW_error;

// Autotest de arranque, reutilizado tal cual: true = fallo critico
// (calefactor fuera de rango), false en cualquier otro caso (incluye "algun
// otro actuador fallo" -- el detalle esta en los bits nuevos de HW_error).
bool actuatorsTest();
void testStandByCurrent();

// true mientras la tarea FTEST tiene el control de los actuadores. Los dos
// escritores de PWM que sobreviven con el control apagado -- PIDHandler()
// (src/system/PID.cpp) y turnFans() (src/system/Actuators.cpp) -- retornan
// sin escribir nada mientras este flag este activo, para que ninguno pise el
// estado seguro de la bateria de fabrica. Definida en factory_test_task.cpp.
extern volatile bool g_factoryTestActive;

// true mientras una prueba de carga de banco (LOAD, solo en builds con
// FTEST_BENCH_DEBUG) tiene los actuadores. Esa prueba levanta tambien
// g_factoryTestActive para que PIDHandler()/turnFans() no pisen sus PWM, pero
// NO es la bateria de fabrica: las vigilancias que la bateria apaga (corte de
// red, bateria baja, ruido) tienen que seguir vivas, porque son justo lo que se
// mide en una prueba de carga/descarga. Ver ftestSuppressesAlarms().
// Definida en factory_test_task.cpp; siempre false en el firmware normal.
extern volatile bool g_benchLoadActive;

// true si la bateria de fabrica (y no una prueba de carga de banco) tiene los
// actuadores y por tanto hay que callar las alarmas que ella provoca a
// proposito.
static inline bool ftestSuppressesAlarms(void) {
  return g_factoryTestActive && !g_benchLoadActive;
}
