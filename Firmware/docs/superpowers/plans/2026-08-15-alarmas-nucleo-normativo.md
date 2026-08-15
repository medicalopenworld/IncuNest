# Núcleo normativo de alarmas (motherBoard) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sustituir el sistema de alarmas de la motherBoard por uno cuyo conjunto de condiciones, prioridades, latching y efectos sobre actuadores derivan exclusivamente de IEC 60601-1-8 y 60601-2-19.

**Architecture:** Tres capas. `shared/` contiene datos y funciones puras (identificadores, prioridad, latching, corte de calefactor) que ambas placas compilan y que se testean en host. `motherBoard/src/modules/control/alarm_machine` contiene la máquina de estados con el tiempo inyectado como parámetro, sin `millis()` dentro, para que sea testeable en host. `security.cpp`, `PID.cpp` y `main.cpp` pasan a ser meros consumidores: detectan condiciones y aplican lo que la máquina dicta.

**Tech Stack:** C++17, PlatformIO, Arduino-ESP32, Unity (`pio test -e native`).

**Spec:** `Firmware/docs/superpowers/specs/2026-08-14-sistema-alarmas-design.md`
**Análisis normativo:** `Firmware/docs/alarms_normative_analysis.md`

## Global Constraints

- **El estado actual del sistema de alarmas no es una referencia.** Donde el código existente contradiga la norma, gana la norma. No se conserva comportamiento por compatibilidad.
- **Ninguna función de `shared/` ni de `alarm_machine` puede llamar a `millis()`, `Serial`, `Preferences` ni a ninguna API de Arduino.** El tiempo entra como parámetro `uint32_t now_ms`. Es la condición para que `pio test -e native` pueda cubrirlas.
- **`CONTROL_SKIN` es `false` y `CONTROL_AIR` es `true`** (`motherBoard/include/main.h:208-209`). El modo piel se comprueba con `in3.controlMode == CONTROL_SKIN`.
- Alcance: solo motherBoard y `shared/`. El protocolo extendido, la presentación en el HMI y el historial NVS son planes separados.
- Toda tarea termina con `pio test -e native` en verde. Las tareas que tocan `motherBoard/src` terminan además con `pio run -e IncuNest_V18`.
- Commits en Conventional Commits, sin `Co-Authored-By`, scope `shared` o `motherboard` según la carpeta tocada.
- Rama de trabajo: `feat/sistema-alarmas`, worktree en `Firmware/.worktrees/sistema-alarmas`.

---

### Task 1: Conjunto de condiciones de alarma y prioridades

Reemplaza el enum de 10 identificadores por las 16 condiciones del análisis normativo, y añade la prioridad como dato consultable. Las prioridades salen de la Tabla 1 de IEC 60601-1-8; la justificación por condición está en `alarms_normative_analysis.md` §5 y no se repite en el código.

**Files:**
- Modify: `Firmware/shared/include/alarm_ids.h` (reescritura completa)
- Create: `Firmware/shared/include/alarm_policy.h`
- Create: `Firmware/shared/src/alarm_policy.cpp`
- Test: `Firmware/motherBoard/test/test_alarm_policy/test_alarm_policy.cpp`

**Interfaces:**
- Consumes: nada.
- Produces: `AlarmId` (enum, `ALARM_NONE=0` … `ALARM_COUNT`), `AlarmPriority` (`ALARM_PRIORITY_LOW=0`, `ALARM_PRIORITY_MEDIUM=1`, `ALARM_PRIORITY_HIGH=2`), y `AlarmPriority alarm_priority(AlarmId id)`.

- [ ] **Step 1: Escribir el test que falla**

Crear `Firmware/motherBoard/test/test_alarm_policy/test_alarm_policy.cpp`:

```cpp
#include <unity.h>
#include "alarm_ids.h"
#include "alarm_policy.h"

void setUp(void) {}
void tearDown(void) {}

// IEC 60601-1-8 Tabla 1: muerte o lesion irreversible con onset "prompt" -> ALTA.
void test_high_priority_set(void) {
  const AlarmId high[] = {
      ALARM_AIR_THERMAL_CUTOUT,  ALARM_SKIN_THERMAL_CUTOUT,
      ALARM_AIR_SENSOR_FAULT,    ALARM_SKIN_SENSOR_FAULT_SKIN_MODE,
      ALARM_FAN_FAILURE,         ALARM_AIR_OUTLET_BLOCKED,
      ALARM_MAINS_INTERRUPTION};
  for (unsigned i = 0; i < sizeof(high) / sizeof(high[0]); ++i) {
    TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_priority(high[i]));
  }
}

// Lesion reversible con onset "prompt" -> MEDIA.
void test_medium_priority_set(void) {
  const AlarmId medium[] = {
      ALARM_AIR_TEMP_DEVIATION_HIGH,  ALARM_AIR_TEMP_DEVIATION_LOW,
      ALARM_SKIN_TEMP_DEVIATION_HIGH, ALARM_SKIN_TEMP_DEVIATION_LOW,
      ALARM_HEATER_FAULT,             ALARM_SUPPLY_UNDERVOLTAGE,
      ALARM_HMI_LINK_LOST};
  for (unsigned i = 0; i < sizeof(medium) / sizeof(medium[0]); ++i) {
    TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_MEDIUM, alarm_priority(medium[i]));
  }
}

// Lesion menor o molestia con onset "delayed" -> BAJA.
void test_low_priority_set(void) {
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW,
                        alarm_priority(ALARM_SKIN_SENSOR_FAULT_AIR_MODE));
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW,
                        alarm_priority(ALARM_HUMIDITY_DEVIATION));
}

// El reparto declarado en la spec: 7 ALTA / 7 MEDIA / 2 BAJA.
void test_priority_distribution(void) {
  int counts[3] = {0, 0, 0};
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    counts[alarm_priority((AlarmId)id)]++;
  }
  TEST_ASSERT_EQUAL_INT(2, counts[ALARM_PRIORITY_LOW]);
  TEST_ASSERT_EQUAL_INT(7, counts[ALARM_PRIORITY_MEDIUM]);
  TEST_ASSERT_EQUAL_INT(7, counts[ALARM_PRIORITY_HIGH]);
}

// Un id fuera de rango no debe devolver basura: se degrada a la mas urgente,
// porque equivocarse hacia arriba es seguro y hacia abajo no.
void test_out_of_range_defaults_to_high(void) {
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_priority(ALARM_COUNT));
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_priority((AlarmId)999));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_high_priority_set);
  RUN_TEST(test_medium_priority_set);
  RUN_TEST(test_low_priority_set);
  RUN_TEST(test_priority_distribution);
  RUN_TEST(test_out_of_range_defaults_to_high);
  return UNITY_END();
}
```

- [ ] **Step 2: Ejecutar y comprobar que falla**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarm_policy`
Expected: FAIL en compilación — `alarm_policy.h: No such file or directory`.

- [ ] **Step 3: Reescribir `alarm_ids.h`**

```c
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Conjunto de condiciones de alarma derivado de IEC 60601-2-19 (201.12.3.101
// a 201.12.3.103, 201.15.4.2.1) y de la Tabla 1 de IEC 60601-1-8. La
// justificacion por condicion vive en docs/alarms_normative_analysis.md §5.
//
// El valor numerico es el indice de bit en el bitmask del protocolo, asi que
// insertar en medio rompe la compatibilidad: las condiciones nuevas van al
// final, antes de ALARM_COUNT.
typedef enum {
  ALARM_NONE = 0,
  // --- Prioridad ALTA ---
  ALARM_AIR_THERMAL_CUTOUT = 1,       // 201.15.4.2.1 aa)
  ALARM_SKIN_THERMAL_CUTOUT = 2,      // 201.15.4.2.1 bb)
  ALARM_AIR_SENSOR_FAULT = 3,
  ALARM_SKIN_SENSOR_FAULT_SKIN_MODE = 4,  // 201.12.3.102
  ALARM_FAN_FAILURE = 5,                  // 201.12.3.101
  ALARM_AIR_OUTLET_BLOCKED = 6,           // 201.12.3.101
  ALARM_MAINS_INTERRUPTION = 7,           // 201.12.3.103
  // --- Prioridad MEDIA ---
  ALARM_AIR_TEMP_DEVIATION_HIGH = 8,   // 201.15.4.2.1 dd), +3 C
  ALARM_AIR_TEMP_DEVIATION_LOW = 9,    // 201.15.4.2.1 dd), -3 C
  ALARM_SKIN_TEMP_DEVIATION_HIGH = 10, // 201.15.4.2.1 ee), +1 C
  ALARM_SKIN_TEMP_DEVIATION_LOW = 11,  // 201.15.4.2.1 ee), -1 C
  ALARM_HEATER_FAULT = 12,
  ALARM_SUPPLY_UNDERVOLTAGE = 13,
  ALARM_HMI_LINK_LOST = 14,
  // --- Prioridad BAJA ---
  ALARM_SKIN_SENSOR_FAULT_AIR_MODE = 15,
  ALARM_HUMIDITY_DEVIATION = 16,
  ALARM_COUNT
} AlarmId;

typedef enum {
  ALARM_PRIORITY_LOW = 0,
  ALARM_PRIORITY_MEDIUM = 1,
  ALARM_PRIORITY_HIGH = 2,
} AlarmPriority;

#define MAX_ALARM_STRING_SIZE 255

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 4: Crear `alarm_policy.h`**

```c
#pragma once
#include "alarm_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

// Prioridad asignada segun la Tabla 1 de IEC 60601-1-8. Un id desconocido
// devuelve ALTA a proposito: sobreestimar la urgencia es seguro.
AlarmPriority alarm_priority(AlarmId id);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 5: Crear `alarm_policy.cpp`**

```cpp
#include "alarm_policy.h"

AlarmPriority alarm_priority(AlarmId id) {
  switch (id) {
    case ALARM_AIR_THERMAL_CUTOUT:
    case ALARM_SKIN_THERMAL_CUTOUT:
    case ALARM_AIR_SENSOR_FAULT:
    case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
    case ALARM_FAN_FAILURE:
    case ALARM_AIR_OUTLET_BLOCKED:
    case ALARM_MAINS_INTERRUPTION:
      return ALARM_PRIORITY_HIGH;

    case ALARM_AIR_TEMP_DEVIATION_HIGH:
    case ALARM_AIR_TEMP_DEVIATION_LOW:
    case ALARM_SKIN_TEMP_DEVIATION_HIGH:
    case ALARM_SKIN_TEMP_DEVIATION_LOW:
    case ALARM_HEATER_FAULT:
    case ALARM_SUPPLY_UNDERVOLTAGE:
    case ALARM_HMI_LINK_LOST:
      return ALARM_PRIORITY_MEDIUM;

    case ALARM_SKIN_SENSOR_FAULT_AIR_MODE:
    case ALARM_HUMIDITY_DEVIATION:
      return ALARM_PRIORITY_LOW;

    default:
      return ALARM_PRIORITY_HIGH;
  }
}
```

- [ ] **Step 6: No tocar `platformio.ini`**

`pre_native.py` ya compila `Firmware/shared/src` entero para `[env:native]` (lo añade con `env.BuildSources`), así que `alarm_policy.cpp` enlaza sin cambiar el `build_src_filter`. Si el enlazado fallase con `undefined reference to alarm_priority`, revisar que el fichero esté en `shared/src/` y no en otra carpeta.

- [ ] **Step 7: Ejecutar y comprobar que pasa**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarm_policy`
Expected: PASS, 5 casos.

- [ ] **Step 8: Commit**

```bash
git add Firmware/shared/include/alarm_ids.h Firmware/shared/include/alarm_policy.h \
        Firmware/shared/src/alarm_policy.cpp \
        Firmware/motherBoard/test/test_alarm_policy/
git commit -m "feat(shared): conjunto de alarmas y prioridades segun IEC 60601"
```

---

### Task 2: Política de latching y de corte de calefactor

Dos preguntas que la norma responde por condición y que hoy están dispersas en tres funciones `ongoing*()` de `security.cpp`. Se convierten en datos consultables.

**Files:**
- Modify: `Firmware/shared/include/alarm_policy.h`
- Modify: `Firmware/shared/src/alarm_policy.cpp`
- Modify: `Firmware/motherBoard/test/test_alarm_policy/test_alarm_policy.cpp`

**Interfaces:**
- Consumes: `AlarmId` de Task 1.
- Produces: `bool alarm_is_latching(AlarmId id)` y `bool alarm_cuts_heater(AlarmId id)`.

- [ ] **Step 1: Añadir los tests que fallan**

Insertar en `test_alarm_policy.cpp`, antes de `main()`:

```cpp
// 201.15.4.2.1 aa) y bb): el corte termico auto-rearmable exige que "la alarma
// opere continuamente hasta reset manual". Es la unica familia latching.
void test_only_thermal_cutouts_latch(void) {
  TEST_ASSERT_TRUE(alarm_is_latching(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_is_latching(ALARM_SKIN_THERMAL_CUTOUT));
  for (int id = ALARM_NONE + 1; id < ALARM_COUNT; ++id) {
    if (id == ALARM_AIR_THERMAL_CUTOUT || id == ALARM_SKIN_THERMAL_CUTOUT) {
      continue;
    }
    TEST_ASSERT_FALSE(alarm_is_latching((AlarmId)id));
  }
}

// 201.12.3.101 (ventilador y salida obstruida), 201.12.3.102 (sonda de piel),
// 201.15.4.2.1 dd)/ee) (desviacion por el lado caliente).
void test_conditions_that_must_cut_the_heater(void) {
  const AlarmId cuts[] = {
      ALARM_AIR_THERMAL_CUTOUT,       ALARM_SKIN_THERMAL_CUTOUT,
      ALARM_AIR_SENSOR_FAULT,         ALARM_SKIN_SENSOR_FAULT_SKIN_MODE,
      ALARM_FAN_FAILURE,              ALARM_AIR_OUTLET_BLOCKED,
      ALARM_AIR_TEMP_DEVIATION_HIGH,  ALARM_SKIN_TEMP_DEVIATION_HIGH,
      ALARM_HEATER_FAULT};
  for (unsigned i = 0; i < sizeof(cuts) / sizeof(cuts[0]); ++i) {
    TEST_ASSERT_TRUE(alarm_cuts_heater(cuts[i]));
  }
}

// dd) y ee) son explicitas: por el lado frio el calefactor DEBE seguir
// encendido. Cortarlo ahi enfriaria a un bebe que ya esta por debajo.
void test_cold_side_deviation_never_cuts_the_heater(void) {
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_AIR_TEMP_DEVIATION_LOW));
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_SKIN_TEMP_DEVIATION_LOW));
}

void test_notify_only_conditions_do_not_cut_the_heater(void) {
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_SUPPLY_UNDERVOLTAGE));
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_HMI_LINK_LOST));
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_FALSE(alarm_cuts_heater(ALARM_SKIN_SENSOR_FAULT_AIR_MODE));
}
```

Y registrarlos en `main()`:

```cpp
  RUN_TEST(test_only_thermal_cutouts_latch);
  RUN_TEST(test_conditions_that_must_cut_the_heater);
  RUN_TEST(test_cold_side_deviation_never_cuts_the_heater);
  RUN_TEST(test_notify_only_conditions_do_not_cut_the_heater);
```

- [ ] **Step 2: Ejecutar y comprobar que falla**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarm_policy`
Expected: FAIL — `alarm_is_latching was not declared in this scope`.

- [ ] **Step 3: Declarar en `alarm_policy.h`**

```c
// 201.15.4.2.1 aa)/bb): un corte termico auto-rearmable debe mantener la
// alarma activa hasta que una persona la resetee, aunque la temperatura ya
// haya bajado. El resto de condiciones son non-latching.
bool alarm_is_latching(AlarmId id);

// true si la norma exige desconectar la alimentacion del calefactor mientras
// la condicion este presente.
bool alarm_cuts_heater(AlarmId id);
```

Añadir `#include <stdbool.h>` al principio del header, tras `#include "alarm_ids.h"`.

- [ ] **Step 4: Implementar en `alarm_policy.cpp`**

```cpp
bool alarm_is_latching(AlarmId id) {
  return id == ALARM_AIR_THERMAL_CUTOUT || id == ALARM_SKIN_THERMAL_CUTOUT;
}

bool alarm_cuts_heater(AlarmId id) {
  switch (id) {
    case ALARM_AIR_THERMAL_CUTOUT:
    case ALARM_SKIN_THERMAL_CUTOUT:
    case ALARM_AIR_SENSOR_FAULT:
    case ALARM_SKIN_SENSOR_FAULT_SKIN_MODE:
    case ALARM_FAN_FAILURE:
    case ALARM_AIR_OUTLET_BLOCKED:
    case ALARM_AIR_TEMP_DEVIATION_HIGH:
    case ALARM_SKIN_TEMP_DEVIATION_HIGH:
    case ALARM_HEATER_FAULT:
      return true;
    default:
      return false;
  }
}
```

- [ ] **Step 5: Ejecutar y comprobar que pasa**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarm_policy`
Expected: PASS, 9 casos.

- [ ] **Step 6: Commit**

```bash
git add Firmware/shared/ Firmware/motherBoard/test/test_alarm_policy/
git commit -m "feat(shared): politica de latching y de corte de calefactor por alarma"
```

---

### Task 3: Máquina de estados — ciclo de vida básico

La máquina sustituye a `alarmOnGoing[]`. El tiempo entra como parámetro; no hay `millis()` dentro.

**Files:**
- Modify: `Firmware/motherBoard/src/modules/control/alarm_machine.h` (reescritura)
- Modify: `Firmware/motherBoard/src/modules/control/alarm_machine.cpp` (reescritura)
- Modify: `Firmware/motherBoard/test/test_alarms/test_alarm_machine.cpp` (reescritura)

**Interfaces:**
- Consumes: `alarm_priority`, `alarm_is_latching`, `alarm_cuts_heater` de Tasks 1-2.
- Produces:
  - `AlarmState` (`ALARM_STATE_INACTIVE`, `ALARM_STATE_PENDING`, `ALARM_STATE_ACTIVE`, `ALARM_STATE_SILENCED`, `ALARM_STATE_ACKED`)
  - `void alarm_machine_init(void)`
  - `void alarm_machine_condition(AlarmId id, bool present, uint32_t now_ms)`
  - `void alarm_machine_tick(uint32_t now_ms)`
  - `AlarmState alarm_machine_state(AlarmId id)`
  - `uint32_t alarm_machine_bitmask(void)`
  - `bool alarm_machine_heater_must_cut(void)`

- [ ] **Step 1: Escribir el test que falla**

Reemplazar el contenido de `test_alarm_machine.cpp`:

```cpp
#include <unity.h>
#include "modules/control/alarm_machine.h"

void setUp(void) { alarm_machine_init(); }
void tearDown(void) {}

void test_starts_inactive(void) {
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_EQUAL_UINT32(0u, alarm_machine_bitmask());
}

// Sin retardo de anuncio configurado, una condicion presente se anuncia ya.
void test_condition_becomes_active(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_FAN_FAILURE));
}

// Non-latching: al irse la condicion, la alarma se va sola.
void test_non_latching_clears_itself(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, false, 2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_EQUAL_UINT32(0u, alarm_machine_bitmask());
}

// Repetir la misma condicion no debe reiniciar nada ni duplicar estado.
void test_repeated_condition_is_idempotent(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1500);
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
}

// El corte de calefactor lo dicta la maquina, no el llamante.
void test_heater_cut_follows_policy(void) {
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 1000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// 201.12.3.101: una salida de aire obstruida debe cortar el calefactor.
void test_blocked_outlet_cuts_the_heater(void) {
  alarm_machine_condition(ALARM_AIR_OUTLET_BLOCKED, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// 201.15.4.2.1 dd)/ee): por el lado frio el calefactor sigue encendido.
void test_cold_deviation_does_not_cut_the_heater(void) {
  alarm_machine_condition(ALARM_AIR_TEMP_DEVIATION_LOW, true, 1000);
  alarm_machine_condition(ALARM_SKIN_TEMP_DEVIATION_LOW, true, 1000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_inactive);
  RUN_TEST(test_condition_becomes_active);
  RUN_TEST(test_non_latching_clears_itself);
  RUN_TEST(test_repeated_condition_is_idempotent);
  RUN_TEST(test_heater_cut_follows_policy);
  RUN_TEST(test_blocked_outlet_cuts_the_heater);
  RUN_TEST(test_cold_deviation_does_not_cut_the_heater);
  return UNITY_END();
}
```

- [ ] **Step 2: Ejecutar y comprobar que falla**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: FAIL en compilación — `ALARM_STATE_INACTIVE was not declared`.

- [ ] **Step 3: Reescribir `alarm_machine.h`**

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "alarm_ids.h"
#include "alarm_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  ALARM_STATE_INACTIVE = 0,
  ALARM_STATE_PENDING,   // condicion presente, dentro del retardo de anuncio
  ALARM_STATE_ACTIVE,    // anunciandose: visual + audio
  ALARM_STATE_SILENCED,  // audio inactivo por accion del operador, visual sigue
  ALARM_STATE_ACKED,     // audio inactivo indefinidamente, visual sigue
} AlarmState;

void alarm_machine_init(void);

// Informa de si la condicion fisica esta presente. Idempotente.
void alarm_machine_condition(AlarmId id, bool present, uint32_t now_ms);

// Hace avanzar los temporizadores. Debe llamarse periodicamente.
void alarm_machine_tick(uint32_t now_ms);

AlarmState alarm_machine_state(AlarmId id);

// Bit por AlarmId de las condiciones que estan generando senal visual, en
// cualquiera de los estados anunciables (ACTIVE, SILENCED, ACKED, PENDING).
uint32_t alarm_machine_bitmask(void);

// true si alguna condicion presente exige desconectar el calefactor.
bool alarm_machine_heater_must_cut(void);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 4: Reescribir `alarm_machine.cpp`**

```cpp
#include "alarm_machine.h"

namespace {

struct Entry {
  bool present;       // la condicion fisica esta ocurriendo ahora
  AlarmState state;
};

Entry g_entries[ALARM_COUNT];

bool is_signalling(AlarmState s) { return s != ALARM_STATE_INACTIVE; }

bool valid(AlarmId id) { return id > ALARM_NONE && id < ALARM_COUNT; }

}  // namespace

void alarm_machine_init(void) {
  for (int i = 0; i < ALARM_COUNT; ++i) {
    g_entries[i].present = false;
    g_entries[i].state = ALARM_STATE_INACTIVE;
  }
}

void alarm_machine_condition(AlarmId id, bool present, uint32_t now_ms) {
  (void)now_ms;
  if (!valid(id)) {
    return;
  }
  Entry &e = g_entries[id];
  e.present = present;

  if (present) {
    if (e.state == ALARM_STATE_INACTIVE) {
      e.state = ALARM_STATE_ACTIVE;
    }
  } else {
    // El latching se aplica en la Task 6; de momento toda condicion que
    // desaparece limpia su alarma.
    e.state = ALARM_STATE_INACTIVE;
  }
}

void alarm_machine_tick(uint32_t now_ms) { (void)now_ms; }

AlarmState alarm_machine_state(AlarmId id) {
  return valid(id) ? g_entries[id].state : ALARM_STATE_INACTIVE;
}

uint32_t alarm_machine_bitmask(void) {
  uint32_t mask = 0;
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    if (is_signalling(g_entries[i].state)) {
      mask |= (1u << i);
    }
  }
  return mask;
}

bool alarm_machine_heater_must_cut(void) {
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    if (g_entries[i].present && alarm_cuts_heater((AlarmId)i)) {
      return true;
    }
  }
  return false;
}
```

- [ ] **Step 5: Ejecutar y comprobar que pasa**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: PASS, 7 casos.

- [ ] **Step 6: Commit**

```bash
git add Firmware/motherBoard/src/modules/control/ Firmware/motherBoard/test/test_alarms/
git commit -m "feat(motherboard): maquina de estados de alarmas con tiempo inyectado"
```

---

### Task 4: Retardo de anuncio (estado PENDING)

201.12.3.104 permite hasta 30 min de AUDIO PAUSED calentando desde frío. El retardo aplaza el anuncio; **no lo cancela**, así que al expirar con la condición presente la alarma pasa a `ACTIVE`. Los cortes térmicos no admiten retardo.

**Files:**
- Modify: `Firmware/motherBoard/src/modules/control/alarm_machine.{h,cpp}`
- Modify: `Firmware/motherBoard/test/test_alarms/test_alarm_machine.cpp`

**Interfaces:**
- Produces: `void alarm_machine_set_announce_delay(AlarmId id, uint32_t delay_ms)` y `bool alarm_machine_audio_required(void)`.

- [ ] **Step 1: Añadir los tests que fallan**

```cpp
// Con retardo configurado, la condicion se registra pero no se anuncia.
void test_delay_holds_in_pending(void) {
  alarm_machine_set_announce_delay(ALARM_HUMIDITY_DEVIATION, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_PENDING,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  // Pero la condicion ya cuenta como senalizada: el operador debe poder verla.
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_HUMIDITY_DEVIATION));
}

// Al expirar el retardo con la condicion viva, se anuncia. Que el retardo
// cancelase el aviso para siempre seria el fallo que tenia el codigo anterior.
void test_delay_expiry_announces(void) {
  alarm_machine_set_announce_delay(ALARM_HUMIDITY_DEVIATION, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_tick(999);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_PENDING,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  alarm_machine_tick(1000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
}

// Si la condicion se va durante el retardo, no llega a anunciarse nunca.
void test_condition_gone_during_delay_never_announces(void) {
  alarm_machine_set_announce_delay(ALARM_HUMIDITY_DEVIATION, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, false, 500);
  alarm_machine_tick(2000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
}

// Un corte termico no admite retardo aunque se le configure uno: 201.15.4.2.1
// exige aviso en el instante en que se dispara.
void test_thermal_cutout_ignores_any_delay(void) {
  alarm_machine_set_announce_delay(ALARM_AIR_THERMAL_CUTOUT, 60000);
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
}
```

Registrar los cuatro en `main()`.

- [ ] **Step 2: Ejecutar y comprobar que falla**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: FAIL — `alarm_machine_set_announce_delay was not declared`.

- [ ] **Step 3: Declarar en `alarm_machine.h`**

```c
// Retardo de anuncio por condicion. 201.12.3.104 lo permite hasta 30 min
// mientras la incubadora calienta desde frio. Los cortes termicos lo ignoran.
void alarm_machine_set_announce_delay(AlarmId id, uint32_t delay_ms);

// true si hay alguna condicion en ACTIVE. SILENCED y ACKED no cuentan.
bool alarm_machine_audio_required(void);
```

- [ ] **Step 4: Implementar**

Ampliar `Entry` y ajustar la lógica:

```cpp
struct Entry {
  bool present;
  AlarmState state;
  uint32_t announce_delay_ms;
  uint32_t present_since_ms;
};
```

En `alarm_machine_init()` inicializar también `announce_delay_ms = 0` y `present_since_ms = 0`.

```cpp
void alarm_machine_set_announce_delay(AlarmId id, uint32_t delay_ms) {
  if (valid(id)) {
    g_entries[id].announce_delay_ms = delay_ms;
  }
}
```

En `alarm_machine_condition()`, sustituir la rama `if (present)`:

```cpp
  if (present) {
    if (e.state == ALARM_STATE_INACTIVE) {
      e.present_since_ms = now_ms;
      // Un corte termico nunca espera: la norma exige aviso inmediato.
      const bool may_wait =
          e.announce_delay_ms > 0 && !alarm_is_latching(id);
      e.state = may_wait ? ALARM_STATE_PENDING : ALARM_STATE_ACTIVE;
    }
  } else {
    e.state = ALARM_STATE_INACTIVE;
  }
```

Y `alarm_machine_tick()`:

```cpp
void alarm_machine_tick(uint32_t now_ms) {
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    Entry &e = g_entries[i];
    if (e.state == ALARM_STATE_PENDING && e.present &&
        (uint32_t)(now_ms - e.present_since_ms) >= e.announce_delay_ms) {
      e.state = ALARM_STATE_ACTIVE;
    }
  }
}

bool alarm_machine_audio_required(void) {
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    if (g_entries[i].state == ALARM_STATE_ACTIVE) {
      return true;
    }
  }
  return false;
}
```

La resta `now_ms - present_since_ms` en `uint32_t` es correcta cuando `millis()` desborda a los ~49 días: el resultado sigue siendo el intervalo real.

- [ ] **Step 5: Ejecutar y comprobar que pasa**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: PASS, 11 casos.

- [ ] **Step 6: Commit**

```bash
git add Firmware/motherBoard/src/modules/control/ Firmware/motherBoard/test/test_alarms/
git commit -m "feat(motherboard): retardo de anuncio de alarma con re-anuncio al expirar"
```

---

### Task 5: Silenciado por condición y aceptación

6.8.1 exige que silenciar una condición **no afecte** a las señales de las demás, lo que descarta el bit `mute` global actual. Y que el silenciado no inactive la señal visual de 1 m.

**Files:**
- Modify: `Firmware/motherBoard/src/modules/control/alarm_machine.{h,cpp}`
- Modify: `Firmware/motherBoard/test/test_alarms/test_alarm_machine.cpp`

**Interfaces:**
- Produces: `void alarm_machine_silence(AlarmId id, uint32_t duration_ms, uint32_t now_ms)` y `void alarm_machine_ack(AlarmId id, uint32_t now_ms)`.

- [ ] **Step 1: Añadir los tests que fallan**

```cpp
// Silenciar calla el audio pero la senal visual sigue (6.8.1: AUDIO PAUSED no
// puede inactivar la senal visual de 1 m).
void test_silence_stops_audio_but_not_the_visual_signal(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_silence(ALARM_FAN_FAILURE, 120000, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_SILENCED,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_FAN_FAILURE));
}

// Al expirar el silencio con la condicion viva, el audio vuelve
// (201.12.3.104: "deben reanudar automaticamente su funcion normal").
void test_silence_expiry_resumes_audio(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_silence(ALARM_FAN_FAILURE, 120000, 0);
  alarm_machine_tick(119999);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  alarm_machine_tick(120000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
}

// El requisito nuclear de 6.8.1: silenciar una NO silencia a las otras.
void test_silencing_one_leaves_the_others_audible(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_silence(ALARM_HUMIDITY_DEVIATION, 120000, 0);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  // Llega un corte termico mientras la otra sigue silenciada.
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 1000);
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_SILENCED,
                        alarm_machine_state(ALARM_HUMIDITY_DEVIATION));
}

// ACK inactiva el audio de forma indefinida, pero no la senal visual.
void test_ack_is_indefinite_and_keeps_the_visual_signal(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_ack(ALARM_FAN_FAILURE, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACKED,
                        alarm_machine_state(ALARM_FAN_FAILURE));
  alarm_machine_tick(3600000);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_FAN_FAILURE));
}

// Silenciar o aceptar no altera la proteccion del actuador.
void test_silencing_never_restores_the_heater(void) {
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  alarm_machine_silence(ALARM_FAN_FAILURE, 120000, 0);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
  alarm_machine_ack(ALARM_FAN_FAILURE, 0);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// Silenciar una condicion que no se esta anunciando no debe hacer nada.
void test_silencing_an_inactive_condition_is_a_no_op(void) {
  alarm_machine_silence(ALARM_FAN_FAILURE, 120000, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_FAN_FAILURE));
}
```

Registrar los seis en `main()`.

- [ ] **Step 2: Ejecutar y comprobar que falla**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: FAIL — `alarm_machine_silence was not declared`.

- [ ] **Step 3: Declarar en `alarm_machine.h`**

```c
// Inactiva el audio de UNA condicion durante duration_ms. 6.8.1 exige que no
// afecte a las senales de las demas, por eso no existe un silencio global.
void alarm_machine_silence(AlarmId id, uint32_t duration_ms, uint32_t now_ms);

// Inactiva el audio de UNA condicion por tiempo indefinido. La senal visual
// se mantiene mientras la condicion persista.
void alarm_machine_ack(AlarmId id, uint32_t now_ms);
```

- [ ] **Step 4: Implementar**

Añadir a `Entry`: `uint32_t silenced_until_ms;` (inicializar a 0 en `init`).

```cpp
void alarm_machine_silence(AlarmId id, uint32_t duration_ms, uint32_t now_ms) {
  if (!valid(id)) {
    return;
  }
  Entry &e = g_entries[id];
  if (e.state != ALARM_STATE_ACTIVE) {
    return;  // solo se silencia lo que se esta anunciando
  }
  e.state = ALARM_STATE_SILENCED;
  e.silenced_until_ms = now_ms + duration_ms;
}

void alarm_machine_ack(AlarmId id, uint32_t now_ms) {
  (void)now_ms;
  if (!valid(id)) {
    return;
  }
  Entry &e = g_entries[id];
  if (e.state == ALARM_STATE_ACTIVE || e.state == ALARM_STATE_SILENCED) {
    e.state = ALARM_STATE_ACKED;
  }
}
```

En `alarm_machine_tick()`, añadir dentro del bucle, tras la rama de `PENDING`:

```cpp
    if (e.state == ALARM_STATE_SILENCED &&
        (int32_t)(now_ms - e.silenced_until_ms) >= 0) {
      e.state = ALARM_STATE_ACTIVE;
    }
```

La comparación con `int32_t` es la forma correcta de comparar instantes de `millis()`: sigue siendo válida cuando el contador desborda.

- [ ] **Step 5: Ejecutar y comprobar que pasa**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: PASS, 17 casos.

- [ ] **Step 6: Commit**

```bash
git add Firmware/motherBoard/src/modules/control/ Firmware/motherBoard/test/test_alarms/
git commit -m "feat(motherboard): silenciado por condicion y aceptacion segun 60601-1-8 6.8.1"
```

---

### Task 6: Alarmas latching y reset manual

201.15.4.2.1 aa)/bb): un corte térmico auto-rearmable debe mantener la alarma activa hasta reset manual, aunque la temperatura ya haya bajado.

**Files:**
- Modify: `Firmware/motherBoard/src/modules/control/alarm_machine.{h,cpp}`
- Modify: `Firmware/motherBoard/test/test_alarms/test_alarm_machine.cpp`

**Interfaces:**
- Produces: `bool alarm_machine_reset(AlarmId id, uint32_t now_ms)` y `bool alarm_machine_is_latched(AlarmId id)`.

- [ ] **Step 1: Añadir los tests que fallan**

```cpp
// El corte termico sigue senalizando aunque la temperatura vuelva a rango.
void test_thermal_cutout_survives_the_condition_clearing(void) {
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, false, 5000);
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_machine_is_latched(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_machine_bitmask() & (1u << ALARM_AIR_THERMAL_CUTOUT));
}

// Solo el reset manual la limpia, y solo si la condicion ya no esta presente.
void test_manual_reset_clears_a_latched_alarm(void) {
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, false, 5000);
  TEST_ASSERT_TRUE(alarm_machine_reset(ALARM_AIR_THERMAL_CUTOUT, 6000));
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_INACTIVE,
                        alarm_machine_state(ALARM_AIR_THERMAL_CUTOUT));
}

// Resetear con la camara todavia caliente no puede apagar la alarma: seria
// devolver el calefactor a un estado peligroso por pulsar un boton.
void test_reset_is_refused_while_the_condition_persists(void) {
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  TEST_ASSERT_FALSE(alarm_machine_reset(ALARM_AIR_THERMAL_CUTOUT, 1000));
  TEST_ASSERT_EQUAL_INT(ALARM_STATE_ACTIVE,
                        alarm_machine_state(ALARM_AIR_THERMAL_CUTOUT));
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
}

// Mientras esta latcheada sin condicion, el calefactor puede volver: la norma
// solo exige mantener la ALARMA, no el corte, una vez bajada la temperatura.
void test_latched_without_condition_releases_the_heater(void) {
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, true, 0);
  TEST_ASSERT_TRUE(alarm_machine_heater_must_cut());
  alarm_machine_condition(ALARM_AIR_THERMAL_CUTOUT, false, 5000);
  TEST_ASSERT_FALSE(alarm_machine_heater_must_cut());
}

// Una non-latching no necesita reset y lo rechaza.
void test_reset_on_a_non_latching_alarm_is_refused(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  TEST_ASSERT_FALSE(alarm_machine_reset(ALARM_HUMIDITY_DEVIATION, 1000));
}
```

Registrar los cinco en `main()`.

- [ ] **Step 2: Ejecutar y comprobar que falla**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: FAIL — `alarm_machine_is_latched was not declared`.

- [ ] **Step 3: Declarar en `alarm_machine.h`**

```c
// true si la alarma sigue senalizando solo porque es latching y su condicion
// ya desaparecio: esta esperando reset manual.
bool alarm_machine_is_latched(AlarmId id);

// Reset manual. Devuelve false si la alarma no es latching o si su condicion
// sigue presente — resetear con la causa viva no puede apagar el aviso.
bool alarm_machine_reset(AlarmId id, uint32_t now_ms);
```

- [ ] **Step 4: Implementar**

En `alarm_machine_condition()`, sustituir la rama `else`:

```cpp
  } else {
    // 201.15.4.2.1 aa)/bb): un corte termico mantiene la alarma hasta reset
    // manual aunque la temperatura ya haya vuelto a rango. El resto se limpia
    // solo (senal non-latching, 6.10).
    if (!alarm_is_latching(id)) {
      e.state = ALARM_STATE_INACTIVE;
    }
  }
```

Y añadir:

```cpp
bool alarm_machine_is_latched(AlarmId id) {
  if (!valid(id)) {
    return false;
  }
  const Entry &e = g_entries[id];
  return alarm_is_latching(id) && !e.present &&
         e.state != ALARM_STATE_INACTIVE;
}

bool alarm_machine_reset(AlarmId id, uint32_t now_ms) {
  (void)now_ms;
  if (!alarm_machine_is_latched(id)) {
    return false;
  }
  g_entries[id].state = ALARM_STATE_INACTIVE;
  return true;
}
```

`alarm_machine_heater_must_cut()` ya consulta `present`, no `state`, así que una alarma latcheada sin condición deja de forzar el corte sin tocar nada.

- [ ] **Step 5: Ejecutar y comprobar que pasa**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: PASS, 22 casos.

- [ ] **Step 6: Commit**

```bash
git add Firmware/motherBoard/src/modules/control/ Firmware/motherBoard/test/test_alarms/
git commit -m "feat(motherboard): cortes termicos latching con reset manual"
```

---

### Task 7: Prioridad activa y ráfaga mínima de audio

6.10 exige que una condición de corta duración complete al menos una ráfaga (MEDIA) o media ráfaga (ALTA). Sin esto, una condición que aparece y se va entre dos evaluaciones no se oye.

**Files:**
- Modify: `Firmware/motherBoard/src/modules/control/alarm_machine.{h,cpp}`
- Modify: `Firmware/motherBoard/test/test_alarms/test_alarm_machine.cpp`

**Interfaces:**
- Produces: `AlarmPriority alarm_machine_top_priority(void)` y `bool alarm_machine_any_signalling(void)`.

- [ ] **Step 1: Añadir los tests que fallan**

```cpp
// La prioridad activa es la mas alta de las que se estan anunciando.
void test_top_priority_is_the_highest_signalling(void) {
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW, alarm_machine_top_priority());
  alarm_machine_condition(ALARM_HEATER_FAULT, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_MEDIUM, alarm_machine_top_priority());
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_HIGH, alarm_machine_top_priority());
}

// Una condicion PENDING no eleva la prioridad activa: todavia no se anuncia.
void test_pending_does_not_raise_top_priority(void) {
  alarm_machine_set_announce_delay(ALARM_HEATER_FAULT, 1000);
  alarm_machine_condition(ALARM_HUMIDITY_DEVIATION, true, 0);
  alarm_machine_condition(ALARM_HEATER_FAULT, true, 0);
  TEST_ASSERT_EQUAL_INT(ALARM_PRIORITY_LOW, alarm_machine_top_priority());
}

void test_no_alarms_means_nothing_signalling(void) {
  TEST_ASSERT_FALSE(alarm_machine_any_signalling());
  alarm_machine_condition(ALARM_FAN_FAILURE, true, 0);
  TEST_ASSERT_TRUE(alarm_machine_any_signalling());
  alarm_machine_condition(ALARM_FAN_FAILURE, false, 1000);
  TEST_ASSERT_FALSE(alarm_machine_any_signalling());
}

// 6.10: una condicion que dura menos que su rafaga minima sigue exigiendo
// audio hasta completarla.
void test_short_condition_still_completes_its_burst(void) {
  alarm_machine_condition(ALARM_HEATER_FAULT, true, 0);
  alarm_machine_condition(ALARM_HEATER_FAULT, false, 10);
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
  alarm_machine_tick(ALARM_MIN_BURST_MS_MEDIUM - 1);
  TEST_ASSERT_TRUE(alarm_machine_audio_required());
  alarm_machine_tick(ALARM_MIN_BURST_MS_MEDIUM);
  TEST_ASSERT_FALSE(alarm_machine_audio_required());
}
```

Registrar los cuatro en `main()`.

- [ ] **Step 2: Ejecutar y comprobar que falla**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: FAIL — `alarm_machine_top_priority was not declared`.

- [ ] **Step 3: Declarar en `alarm_machine.h`**

```c
// Duracion minima de audio que 6.10 exige completar aunque la condicion se
// haya ido: una rafaga entera en MEDIA, media rafaga en ALTA. Los valores
// salen de la Tabla 3 con el patron elegido en la spec §8.
#define ALARM_MIN_BURST_MS_HIGH   1200u
#define ALARM_MIN_BURST_MS_MEDIUM 1600u

// Prioridad mas alta entre las condiciones que se estan anunciando
// (ACTIVE, SILENCED o ACKED). Si no hay ninguna, devuelve ALARM_PRIORITY_LOW.
AlarmPriority alarm_machine_top_priority(void);

// true si alguna condicion esta generando senal visual.
bool alarm_machine_any_signalling(void);
```

- [ ] **Step 4: Implementar**

Añadir a `Entry`: `uint32_t audio_hold_until_ms;` (inicializar a 0).

En `alarm_machine_condition()`, en la transición a `ACTIVE`, sellar el mínimo de ráfaga:

```cpp
      e.state = may_wait ? ALARM_STATE_PENDING : ALARM_STATE_ACTIVE;
      if (e.state == ALARM_STATE_ACTIVE) {
        e.audio_hold_until_ms =
            now_ms + (alarm_priority(id) == ALARM_PRIORITY_HIGH
                          ? ALARM_MIN_BURST_MS_HIGH
                          : ALARM_MIN_BURST_MS_MEDIUM);
      }
```

Y en `alarm_machine_tick()`, en la transición de `PENDING` a `ACTIVE`, hacer lo mismo.

Sustituir `alarm_machine_audio_required()`:

```cpp
bool alarm_machine_audio_required(void) {
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    const Entry &e = g_entries[i];
    if (e.state == ALARM_STATE_ACTIVE) {
      return true;
    }
    // 6.10: la rafaga minima se completa aunque la condicion ya se haya ido,
    // salvo que el operador la haya inactivado explicitamente.
    if (e.state == ALARM_STATE_INACTIVE &&
        (int32_t)(g_last_tick_ms - e.audio_hold_until_ms) < 0) {
      return true;
    }
  }
  return false;
}
```

Esto necesita conocer el instante actual sin recibirlo: añadir un `uint32_t g_last_tick_ms = 0;` en el `namespace`, actualizarlo al principio de `alarm_machine_tick()` y también en `alarm_machine_condition()` (`g_last_tick_ms = now_ms;`), e inicializarlo a 0 en `alarm_machine_init()`.

```cpp
AlarmPriority alarm_machine_top_priority(void) {
  AlarmPriority top = ALARM_PRIORITY_LOW;
  for (int i = ALARM_NONE + 1; i < ALARM_COUNT; ++i) {
    const AlarmState s = g_entries[i].state;
    if (s == ALARM_STATE_ACTIVE || s == ALARM_STATE_SILENCED ||
        s == ALARM_STATE_ACKED) {
      const AlarmPriority p = alarm_priority((AlarmId)i);
      if (p > top) {
        top = p;
      }
    }
  }
  return top;
}

bool alarm_machine_any_signalling(void) { return alarm_machine_bitmask() != 0; }
```

- [ ] **Step 5: Ejecutar y comprobar que pasa**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarms`
Expected: PASS, 26 casos.

- [ ] **Step 6: Commit**

```bash
git add Firmware/motherBoard/src/modules/control/ Firmware/motherBoard/test/test_alarms/
git commit -m "feat(motherboard): prioridad activa y rafaga minima de audio (6.10)"
```

---

### Task 8: Acotar los umbrales de corte térmico

201.15.4.2.1 aa) exige que el corte de aire no exceda 38 °C y bb) que el de piel no exceda 40 °C. Hoy `/config` y USB los escriben sin límite.

**Files:**
- Modify: `Firmware/shared/include/alarm_policy.h`
- Modify: `Firmware/shared/src/alarm_policy.cpp`
- Modify: `Firmware/motherBoard/test/test_alarm_policy/test_alarm_policy.cpp`
- Modify: `Firmware/motherBoard/src/tasks/CommTask.cpp:560-564`
- Modify: `Firmware/motherBoard/src/tasks/Wifi_OTA.cpp:453-458`

**Interfaces:**
- Produces: `float alarm_clamp_air_cutout(float celsius)` y `float alarm_clamp_skin_cutout(float celsius)`.

- [ ] **Step 1: Añadir los tests que fallan**

```cpp
void test_air_cutout_is_capped_at_38(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.0f, alarm_clamp_air_cutout(45.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 38.0f, alarm_clamp_air_cutout(38.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 36.5f, alarm_clamp_air_cutout(36.5f));
}

void test_skin_cutout_is_capped_at_40(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, alarm_clamp_skin_cutout(50.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 37.5f, alarm_clamp_skin_cutout(37.5f));
}

// Un valor absurdo por abajo dejaria el equipo alarmando siempre; se acota a
// un minimo clinicamente util.
void test_cutouts_have_a_floor(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, alarm_clamp_air_cutout(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, alarm_clamp_skin_cutout(-5.0f));
}
```

Registrar los tres en `main()`.

- [ ] **Step 2: Ejecutar y comprobar que falla**

Run: `cd Firmware/motherBoard && pio test -e native -f test_alarm_policy`
Expected: FAIL — `alarm_clamp_air_cutout was not declared`.

- [ ] **Step 3: Declarar en `alarm_policy.h`**

```c
// Limites de corte termico. 201.15.4.2.1 aa): el corte por aire no puede
// exceder 38 C. bb): el de piel no puede exceder 40 C. El suelo de 34 C sale
// del rango auto-rearmable que la misma clausula admite (34-39 C).
#define ALARM_AIR_CUTOUT_MAX_C  38.0f
#define ALARM_SKIN_CUTOUT_MAX_C 40.0f
#define ALARM_CUTOUT_MIN_C      34.0f

float alarm_clamp_air_cutout(float celsius);
float alarm_clamp_skin_cutout(float celsius);
```

- [ ] **Step 4: Implementar en `alarm_policy.cpp`**

```cpp
static float clamp_range(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float alarm_clamp_air_cutout(float celsius) {
  return clamp_range(celsius, ALARM_CUTOUT_MIN_C, ALARM_AIR_CUTOUT_MAX_C);
}

float alarm_clamp_skin_cutout(float celsius) {
  return clamp_range(celsius, ALARM_CUTOUT_MIN_C, ALARM_SKIN_CUTOUT_MAX_C);
}
```

- [ ] **Step 5: Aplicar el clamp en las dos rutas de escritura**

En `CommTask.cpp`, sustituir las dos asignaciones:

```cpp
        in3.skinTemperatureSetMax = alarm_clamp_skin_cutout(value);
        ...
        in3.airTemperatureSetMax = alarm_clamp_air_cutout(value);
```

En `Wifi_OTA.cpp`:

```cpp
      in3.airTemperatureSetMax =
          alarm_clamp_air_cutout(wifiServer.arg("air_tmax").toFloat());
      ...
      in3.skinTemperatureSetMax =
          alarm_clamp_skin_cutout(wifiServer.arg("skin_tmax").toFloat());
```

Añadir `#include "alarm_policy.h"` en ambos ficheros si no llega ya por `main.h`.

- [ ] **Step 6: Ejecutar tests y compilar**

Run: `cd Firmware/motherBoard && pio test -e native && pio run -e IncuNest_V18`
Expected: todos los tests PASS y build SUCCESS.

- [ ] **Step 7: Commit**

```bash
git add Firmware/shared/ Firmware/motherBoard/test/test_alarm_policy/ \
        Firmware/motherBoard/src/tasks/CommTask.cpp Firmware/motherBoard/src/tasks/Wifi_OTA.cpp
git commit -m "fix(motherboard): acotar los umbrales de corte termico a 38/40 C"
```

---

### Task 9: Conectar la detección a la máquina

`security.cpp` deja de mantener estado propio: detecta condiciones y se las pasa a la máquina. Es la tarea que hace que todo lo anterior tenga efecto real.

**Files:**
- Modify: `Firmware/motherBoard/src/system/security.cpp`
- Modify: `Firmware/motherBoard/include/main.h` (prototipos de `ongoing*` retirados)
- Modify: `Firmware/motherBoard/src/system/PID.cpp:201,211`
- Modify: `Firmware/motherBoard/src/main.cpp:393`

**Interfaces:**
- Consumes: toda la API de `alarm_machine` de las Tasks 3-7.
- Produces: `securityCheck()` con la semántica nueva.

- [ ] **Step 1: Sustituir la detección de desviación de temperatura**

En `checkAlarms()`, reemplazar la llamada única a `evaluateAlarm(TEMPERATURE_ALARM, ...)` por las cuatro condiciones direccionales que exige 201.15.4.2.1. En modo aire el umbral es 3 °C y en modo piel 1 °C:

```cpp
  if (in3.temperatureControl) {
    const bool airMode = (in3.controlMode == CONTROL_AIR);
    const float measured = airMode ? in3.temperature[ROOM_DIGITAL_TEMP_SENSOR]
                                   : in3.temperature[SKIN_SENSOR];
    const float deviation = measured - (float)in3.desiredControlTemperature;
    // 201.15.4.2.1 dd): +-3 C en control por aire. ee): +-1 C en control por
    // piel. El calefactor solo se corta por el lado caliente.
    const float limit = airMode ? 3.0f : 1.0f;
    const AlarmId high = airMode ? ALARM_AIR_TEMP_DEVIATION_HIGH
                                 : ALARM_SKIN_TEMP_DEVIATION_HIGH;
    const AlarmId low = airMode ? ALARM_AIR_TEMP_DEVIATION_LOW
                                : ALARM_SKIN_TEMP_DEVIATION_LOW;
    const uint32_t now = millis();
    alarm_machine_condition(high, deviation > limit, now);
    alarm_machine_condition(low, deviation < -limit, now);
  }
```

- [ ] **Step 2: Sustituir el resto de detecciones**

En `checkThermalCutOuts()`, `checkStatusOfSensor()`, `checkFanSpeed()`, `checkAirBlockage()` y `powerSupplyCheck()`, sustituir cada `setAlarm(X)` / `resetAlarm(X)` por una sola llamada `alarm_machine_condition(<nueva AlarmId>, <condicion>, millis())`. Mapeo:

| Antes | Ahora |
|---|---|
| `AIR_THERMAL_CUTOUT_ALARM` | `ALARM_AIR_THERMAL_CUTOUT` |
| `SKIN_THERMAL_CUTOUT_ALARM` | `ALARM_SKIN_THERMAL_CUTOUT` |
| `AIR_SENSOR_ISSUE_ALARM` | `ALARM_AIR_SENSOR_FAULT` |
| `SKIN_SENSOR_ISSUE_ALARM` | `ALARM_SKIN_SENSOR_FAULT_SKIN_MODE` si `in3.controlMode == CONTROL_SKIN`, si no `ALARM_SKIN_SENSOR_FAULT_AIR_MODE` |
| `FAN_ISSUE_ALARM` | `ALARM_FAN_FAILURE` |
| `HEATER_ISSUE_ALARM` | `ALARM_HEATER_FAULT` |
| `POWER_SUPPLY_ALARM` | `ALARM_SUPPLY_UNDERVOLTAGE` |
| `AIR_BLOCKED_ALARM` | `ALARM_AIR_OUTLET_BLOCKED` |
| `HUMIDITY_ALARM` | `ALARM_HUMIDITY_DEVIATION` |

Al cambiar de modo hay que retirar la condición del par contrario, o quedaría colgada:

```cpp
  if (in3.controlMode == CONTROL_SKIN) {
    alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_AIR_MODE, false, millis());
  } else {
    alarm_machine_condition(ALARM_SKIN_SENSOR_FAULT_SKIN_MODE, false, millis());
  }
```

- [ ] **Step 3: Habilitar la detección de salida obstruida**

En `Firmware/motherBoard/include/config/board.h`, poner `AIR_BLOCKED_DETECTION_ENABLED` a `1`. Es un requisito de 201.12.3.101 y no puede enviarse deshabilitado.

**Aviso para quien ejecute:** `FAN_DUTY_BLOCKED_THRESHOLD` sigue sin calibrar en banco. Con la detección activa **y** cortando el calefactor, un falso positivo enfría al bebé. Esta tarea deja el código conforme, pero **el equipo no puede salir a campo hasta que el umbral se calibre**. Anotarlo en el commit y avisar al responsable del proyecto.

- [ ] **Step 4: Llamar al tick y sustituir las puertas de actuador**

Al final de `securityCheck()`, añadir `alarm_machine_tick(millis());`.

Sustituir el cuerpo de las tres funciones de puerta para que deleguen:

```cpp
bool ongoingCriticalAlarm() { return alarm_machine_heater_must_cut(); }
bool ongoingAlarms() { return alarm_machine_any_signalling(); }
```

`ongoingCriticalWiringAlarm()` y `ongoingFanCriticalAlarm()` se conservan tal cual: gobiernan el ventilador y el re-armado del lazo, que este plan no cambia. Solo hay que actualizar los identificadores que usan.

- [ ] **Step 5: Compilar y pasar los tests**

Run: `cd Firmware/motherBoard && pio test -e native && pio run -e IncuNest_V18`
Expected: todos PASS y build SUCCESS. Si el HMI deja de compilar por los identificadores renombrados, es la Task 10.

- [ ] **Step 6: Commit**

```bash
git add Firmware/motherBoard/src/ Firmware/motherBoard/include/
git commit -m "feat(motherboard): detectar condiciones contra la maquina de alarmas normativa"
```

---

### Task 10: Restaurar la compilación del HMI

El HMI incluye `alarm_ids.h` y referencia identificadores que ya no existen. Esta tarea solo persigue dejar el build verde; la presentación conforme es un plan aparte.

**Files:**
- Modify: `Firmware/Display_HMI/src/tasks/UITask.cpp:1212-1232`
- Modify: `Firmware/Display_HMI/include/main.h:295-296`

- [ ] **Step 1: Actualizar los identificadores**

En `UI_IsCriticalAlarmActive()` y `isFanHeaterAlarmActive()`, sustituir:

```cpp
    if (alarmList[i].id == ALARM_AIR_THERMAL_CUTOUT ||
        alarmList[i].id == ALARM_SKIN_THERMAL_CUTOUT ||
        alarmList[i].id == ALARM_FAN_FAILURE ||
        alarmList[i].id == ALARM_AIR_OUTLET_BLOCKED) {
```

y `FAN_ISSUE_ALARM` / `HEATER_ISSUE_ALARM` por `ALARM_FAN_FAILURE` / `ALARM_HEATER_FAULT`.

En `Display_HMI/include/main.h`, sustituir `constexpr int MAX_ALARMS = NUM_ALARMS;` por `constexpr int MAX_ALARMS = ALARM_COUNT;`.

- [ ] **Step 2: Compilar**

Run: `cd Firmware/Display_HMI && pio run -e main`
Expected: SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add Firmware/Display_HMI/
git commit -m "fix(hmi): adaptar las referencias al nuevo conjunto de alarmas"
```

---

### Task 11: El audio deja de apagarse solo

6.10 es taxativo: las señales acústicas cesan **solo** cuando el operador activa un estado de inactivación o resetea la alarma. Hoy el zumbador se calla a los ~250 s porque `buzzerAlarmBeepCount` se agota. Esta tarea convierte el zumbador en un esclavo del estado de la máquina.

**Files:**
- Modify: `Firmware/motherBoard/src/system/Buzzer.cpp`
- Modify: `Firmware/motherBoard/include/main.h:230-232`
- Modify: `Firmware/motherBoard/src/system/security.cpp` (retirar las llamadas a `buzzerTone` de `setAlarm`)

**Interfaces:**
- Consumes: `alarm_machine_audio_required()` y `alarm_machine_top_priority()` de las Tasks 4 y 7.
- Produces: `void buzzerAlarmUpdate(bool audioRequired, AlarmPriority priority)`.

- [ ] **Step 1: Sustituir las constantes de patrón**

En `main.h`, retirar `buzzerAlarmTone`, `buzzerAlarmBeepTime` y `buzzerAlarmBeepCount` — el primero es código muerto (`buzzerTone()` ignora su tercer argumento) y el tercero es la causa del corte a los 4 minutos. En su lugar:

```c
// Patron de rafaga segun la Tabla 3 de IEC 60601-1-8. ALTA: 10 pulsos con
// intervalo entre rafagas de 2,5 a 15 s. MEDIA: 3 pulsos, de 2,5 a 30 s.
// BAJA: 1 o 2 pulsos, intervalo > 15 s o sin repeticion.
#define ALARM_PULSE_MS            150u
#define ALARM_PULSE_GAP_MS        150u
#define ALARM_BURST_PULSES_HIGH   10u
#define ALARM_BURST_PULSES_MEDIUM 3u
#define ALARM_BURST_PULSES_LOW    1u
#define ALARM_BURST_PERIOD_MS_HIGH   10000u
#define ALARM_BURST_PERIOD_MS_MEDIUM 25000u
#define ALARM_BURST_PERIOD_MS_LOW    30000u
```

- [ ] **Step 2: Reescribir el motor del zumbador**

En `Buzzer.cpp`, sustituir `buzzerHandler()` por una máquina sin contador de agotamiento:

```cpp
// El patron se regenera indefinidamente mientras audioRequired sea true.
// IEC 60601-1-8 6.10: el audio solo cesa por accion del operador, nunca por
// haber sonado "suficiente" tiempo.
void buzzerAlarmUpdate(bool audioRequired, AlarmPriority priority) {
  static uint32_t phaseStart = 0;
  static uint32_t pulsesLeft = 0;
  static bool on = false;

  if (!audioRequired) {
    if (on) {
      ledcWrite(BUZZER_PWM_CHANNEL, 0);
      on = false;
    }
    pulsesLeft = 0;
    phaseStart = millis();
    return;
  }

  const uint32_t now = millis();
  const uint32_t burstPulses =
      priority == ALARM_PRIORITY_HIGH   ? ALARM_BURST_PULSES_HIGH
      : priority == ALARM_PRIORITY_MEDIUM ? ALARM_BURST_PULSES_MEDIUM
                                          : ALARM_BURST_PULSES_LOW;
  const uint32_t burstPeriod =
      priority == ALARM_PRIORITY_HIGH   ? ALARM_BURST_PERIOD_MS_HIGH
      : priority == ALARM_PRIORITY_MEDIUM ? ALARM_BURST_PERIOD_MS_MEDIUM
                                          : ALARM_BURST_PERIOD_MS_LOW;

  if (pulsesLeft == 0) {
    if ((uint32_t)(now - phaseStart) < burstPeriod) {
      return;  // silencio entre rafagas
    }
    pulsesLeft = burstPulses;
    phaseStart = now;
    on = false;
  }

  const uint32_t slot = on ? ALARM_PULSE_MS : ALARM_PULSE_GAP_MS;
  if ((uint32_t)(now - phaseStart) < slot) {
    return;
  }
  phaseStart = now;
  on = !on;
  ledcWrite(BUZZER_PWM_CHANNEL, on ? BUZZER_HALF_PWM : 0);
  if (!on && pulsesLeft > 0) {
    pulsesLeft--;
  }
}
```

- [ ] **Step 3: Quitar el disparo desde `setAlarm`**

En `security.cpp`, borrar las llamadas a `buzzerTone(...)` de `setAlarm()` y la de `shutBuzzer()` de `resetAlarm()`. El zumbador ya no se dispara por evento: se consulta por estado. Al final de `securityCheck()`, tras `alarm_machine_tick(millis())`, añadir:

```cpp
  buzzerAlarmUpdate(alarm_machine_audio_required(),
                    alarm_machine_top_priority());
```

- [ ] **Step 4: Compilar**

Run: `cd Firmware/motherBoard && pio test -e native && pio run -e IncuNest_V18`
Expected: todos PASS y build SUCCESS.

- [ ] **Step 5: Verificación en hardware (manual, la hace el responsable del proyecto)**

Provocar un `ALARM_FAN_FAILURE` y comprobar con un cronómetro que a los 10 minutos el zumbador **sigue** sonando. Es la comprobación de que 6.10 se cumple, y no es automatizable.

- [ ] **Step 6: Commit**

```bash
git add Firmware/motherBoard/src/system/Buzzer.cpp Firmware/motherBoard/include/main.h \
        Firmware/motherBoard/src/system/security.cpp
git commit -m "fix(motherboard): el audio de alarma deja de cesar por tiempo (60601-1-8 6.10)"
```

---

## Fuera de este plan

Requisitos identificados en el análisis normativo que **no** se cierran aquí, para que nadie los dé por hechos:

- **Independencia del corte térmico** (201.15.4.2.1 aa): necesita un segundo canal de temperatura. Hardware.
- **Alarma de corte de red durante 10 min** (201.12.3.103): necesita reserva de energía. Hardware. `ALARM_MAINS_INTERRUPTION` existe en el enum pero **nadie la levanta todavía**.
- **Detección de cortocircuito en la sonda de piel** (201.12.3.102): el timeout de 20 s no ve un corto.
- **Niveles acústicos** (201.9.6.2.1.102/103): ≥65 dB(A) a 3 m y ≤80 dB(A) en el compartimento. Medición de laboratorio.
- **Test de alarmas para el operador** (201.12.3.105), **protocolo extendido**, **presentación en el HMI** e **historial NVS**: planes separados.
