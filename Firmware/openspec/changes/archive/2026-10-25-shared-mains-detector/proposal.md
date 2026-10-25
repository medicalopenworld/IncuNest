## Why

Placas afectadas: **motherBoard** (detector y cableado a la máquina de alarmas) y **Display_HMI** (indicador «en batería» en el heading). **shared/** no cambia: el id, la prioridad, los textos en cuatro lenguas, la clave de telemetría y la exclusión del silencio de `ALARM_MAINS_INTERRUPTION` ya existen.

`ALARM_MAINS_INTERRUPTION` (id 7, prioridad ALTA) está definida, traducida y excluida del silencio, pero **nadie la declara**: `docs/alarms.md` §9 lo dice con todas las letras. Una incubadora que se queda sin red y sigue funcionando de batería no avisa a nadie, y la norma (IEC 60601-2-19 201.12.3.103) exige un aviso sonoro y visual de la interrupción de alimentación durante un mínimo de 10 min.

El dato para detectarlo ya está en la placa: el cargador BQ25730 se lee cada 5 s (`charge_status()`, `main.cpp`) y `g_bq_status.ac_present` es el bit AC_STAT. Hoy solo se usa para reinicializar el chip al volver el adaptador y para publicar telemetría.

Toca el área de alarmas, así que aplica el known issue nº 1 de `Firmware/docs/known_issues.md` («The Phantom Alarms»): la condición nace con antirrebote y no puede parpadear, y el HMI deriva el indicador del `alarmBitmask` de `CTRL,STATE` (la «verdad binaria» que se barre cada segundo), no de un evento que pueda perderse.

## What Changes

- **Detector puro** `modules/control/mains_detector.{h,cpp}` (C-linkage, sin Arduino): antirrebote de entrada (2 lecturas válidas consecutivas), liberación con antirrebote y **señalización mínima de 10 min desde la declaración** aunque la red vuelva antes. Una lectura no válida del cargador no declara ni retira nada.
- **Cableado** en motherBoard: el hilo de sensores alimenta el detector tras cada lectura del BQ a través de un único punto, `mains_present_now()`, que es donde la rev. V19 añadirá el pin `MAINS_OK_PIN`; `securityCheck()` evalúa el detector y llama a `alarm_machine_condition(ALARM_MAINS_INTERRUPTION, ...)`. Sin cargador (`chargerPresent == false`) nunca se declara y se anota una vez en el log. No se declara durante el test de fábrica.
- **Indicador «en batería» en el HMI**: una etiqueta con icono de batería, en ámbar, en el heading de Main (a la derecha del reloj) y en la réplica del heading de la pantalla de bloqueo (en el slot de ayuda, que ahí está vacío), con texto en ES/EN/FR/PT. Se deriva del **bit 7 de `alarmBitmask`**; no hay campo de protocolo nuevo.
- **Telemetría**: sin cambios de código; `ALARM_MAINS_INTERRUPTION_KEY` ya se publica por `GPRS.cpp` y `Wifi_OTA.cpp`.
- **Docs**: `docs/alarms.md` (§1 fila 7 y §9), `docs/alarms_bench_verification.md`.

## Capabilities

### New Capabilities

- `mb-mains-interruption-detector`: cuándo la motherBoard declara y retira `ALARM_MAINS_INTERRUPTION`, con qué antirrebote, cuánto dura como mínimo, qué hace ante lecturas no válidas o sin cargador, y cómo se incorporará el pin de la V19.
- `hmi-on-battery-indicator`: el indicador persistente de «en batería» del heading, en Main y en Lock.

## Impact

- **Código — motherBoard**: nuevo `src/modules/control/mains_detector.{h,cpp}`; `src/main.cpp` (muestreo tras `charge_status()`, `mains_present_now()`); `src/system/security.cpp` (`checkMainsInterruption()` dentro de `securityCheck()`); `platformio.ini` (`build_src_filter` de `[env:native]`); `test/test_mains_detector/`.
- **Código — Display_HMI**: `src/ui/ElementsCreation.cpp` y `include/ui/ElementsCreation.h` (dos widgets nuevos), `src/tasks/UITask.cpp` (`on_battery_heading_update()`, junto a `clock_update()`), `include/ui/i18n_strings.def` (una cadena).
- **Sin cambios de protocolo**: `PROTOCOL.md` no se toca.
- **Testing**: el detector es lógica pura y entra en `[env:native]` con tests Unity (rojo antes que verde). El cableado, el hilo de sensores y todo el HMI **no tienen entorno de test**: verificación manual en banco, listada en `tasks.md` y marcada como manual.
- **Lo que este cambio NO resuelve — hardware**: que el equipo siga vivo y sonando 10 min sin red (reserva de energía) y que el zumbador y el micro cuelguen de VSYS y no de la entrada de red. Es verificación de banco, no firmware (`docs/alarms_normative_analysis.md` §2.5).
- **Out of scope (Non-goals)**: el pin `MAINS_OK_PIN` de la V19 (otra rama, `feat/mb-hw19`); cambiar prioridad, textos o política de silencio de la alarma; un campo de protocolo para el estado de red.
