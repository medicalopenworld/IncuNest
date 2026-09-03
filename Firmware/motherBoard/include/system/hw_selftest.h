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
