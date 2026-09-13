## Why

Placas afectadas: **motherBoard** (detecta la condición y la anuncia), **Display_HMI** (el gesto de override y su indicación permanente) y **shared/** (nuevo `AlarmId`, su prioridad y sus textos). Toca el área de alarmas, así que aplica el known issue nº 1 de `Firmware/docs/known_issues.md` («The Phantom Alarms»): cualquier condición nueva nace con antirrebote e histéresis y no puede parpadear.

> **Trabajo futuro, no planificado todavía.** Esta propuesta se registra para no perder el análisis. Nada de aquí está implementado y la Fase 0 tiene decisiones abiertas que hay que cerrar antes de escribir código.

**Todas las alarmas térmicas del equipo son relativas a la consigna.** El firmware nunca se pregunta «¿37 °C es mucho?», sino «¿me estoy alejando de lo que me pidieron?». Si no se aleja, calla — sea cual sea la temperatura absoluta.

Eso deja un punto ciego que ningún umbral relativo puede cubrir: **si la consigna en sí es alta y el lazo la sigue bien, no hay desviación que alarmar.** Con el umbral de ±1 °C que introduce `fix/mb-air-deviation-1c`:

| Consigna | Aire real | Desviación | ¿Alarma? |
| -------- | --------- | ---------- | -------- |
| 35.0 | 37.0 | +2.0 | Sí |
| **36.5** | **37.4** | **+0.9** | **No** |

En la segunda fila el aire está más caliente que en la primera y el equipo calla, porque está haciendo exactamente lo que se le mandó.

El único límite absoluto que existe hoy es el corte térmico del aire, topado a 38 °C (`ALARM_AIR_CUTOUT_MAX_C`). **Ese no es un aviso: es el disyuntor.** Es prioridad ALTA, corta el calefactor, es latching y exige reset manual — la última red, no algo que quieras ver en uso normal. Entre «todo bien» y «se ha disparado el disyuntor» no hay nada. Y además, por `Firmware/docs/alarms_normative_analysis.md` §2.4, ese corte lee `ROOM_DIGITAL_TEMP_SENSOR`, **el mismo sensor que usa el PID de control**: un fallo de sensor se lleva por delante el termostato y su propio corte a la vez. Apoyar toda la protección absoluta en él es apoyarla en un único punto de fallo.

El disparador real fue la unidad 353 el 2026-09-10: consigna de 35 °C de aire, la fototerapia la subió a 37 °C, y ningún aviso. `fix/mb-air-deviation-1c` cubre ese caso concreto. Este cambio cubre el que queda: la consigna alta seguida correctamente.

## What Changes

- **Nueva condición absoluta de sobretemperatura de aire**, independiente de la consigna: temperatura de aire por encima de **37 °C**. Prioridad MEDIA, no latching, con su propio antirrebote e histéresis. Es un aviso recuperable, un grado antes del disyuntor.
- **Camino de override para consignas por encima de 37 °C.** Poner el aire a 37 °C o más es legítimo para un gran prematuro, así que una alarma seca a 37 sonaría en uso correcto — justo la fatiga de alarma que queremos evitar. Por encima de 37 °C la consigna solo se alcanza con un **gesto deliberado** del clínico, y mientras dure, la pantalla lo indica de forma **permanente**. Sin override, la consigna de aire queda topada en 37 °C.
- **La alarma absoluta no se anuncia mientras el override está activo** — avisaría de lo que el clínico acaba de pedir a propósito. Lo que no desaparece es la indicación permanente de que el equipo está en override.
- **El corte térmico de 38 °C no se toca.** Sigue siendo el disyuntor, con su prioridad ALTA y su reset manual.

## Capabilities

### New Capabilities

- `air-overtemperature-guard`: el límite absoluto de temperatura de aire y cómo se sale de él. Cubre la condición de sobretemperatura independiente de la consigna, su umbral y antirrebote, el tope de consigna en 37 °C, el gesto de override que lo levanta, la indicación permanente mientras dure, la relación con el corte térmico de 38 °C (que no sustituye) y con las alarmas de desviación (que no duplica).

## Impact

- **Código — motherBoard**: `src/system/security.cpp` (`checkAlarms()`, nueva condición junto a las de desviación), `include/main.h` (`AIR_TEMPERATURE_SET_MAX` pasa a ser el tope *con* override; aparece el tope sin override), `src/tasks/CommTask.cpp` y `src/tasks/Wifi_OTA.cpp` (validación de consigna y clave de telemetría de la nueva alarma), `include/config/telemetry_keys.h`.
- **Código — shared/**: `include/alarm_ids.h` (nuevo id **al final del enum**, nunca en medio: el valor es el índice de bit del protocolo), `src/alarm_policy.cpp` (prioridad; **no** corta calefactor: el corte ya lo gobiernan la desviación y el corte térmico), `src/alarm_text.cpp` (título y descripción en ES/FR/PT/EN).
- **Código — Display_HMI**: el gesto de override en la pantalla de consigna y la indicación permanente mientras esté activo.
- **Docs**: `Firmware/docs/alarms.md` (tabla de condiciones y §5), `Firmware/PROTOCOL.md` si el override viaja por el enlace, `Firmware/docs/alarm_popup_image_prompts.md` (imagen de pop-up de la condición nueva).
- **Testing**: el umbral con histéresis y la lógica de «no anunciar bajo override» son lógica pura y **deben** vivir en `motherBoard/modules/control/` para entrar en `[env:native]` con tests Unity en el mismo commit, por `.claude/rules/testing.md`. El gesto de override, la indicación permanente y el comportamiento con un bebé dentro son **verificación manual en banco**, documentada como tal.
- **Conformidad, dicho claro**: la norma contempla un camino de override por encima de 37 °C, pero exige un **segundo corte térmico a 40 °C que este hardware no tiene** (`alarms_normative_analysis.md` §2.4). Con lo que hay hoy, este cambio mejora la seguridad real pero **no permite reclamar conformidad con el override normativo**. Hay que decidir en la Fase 0 si se implementa igualmente como mejora o si se espera a la revisión de hardware.
- **Out of scope (Non-goals)**:
  - El segundo corte térmico a 40 °C y la independencia de sensor del corte de 38 °C. Son hardware, no firmware (`alarms_normative_analysis.md` §2.4).
  - El equivalente en modo piel. El mismo razonamiento aplica, pero los números y el flujo clínico son otros y merece su propia decisión.
  - Refrigeración activa. La incubadora no puede bajar la temperatura; esto es un aviso al operador, no una corrección.
  - Tocar el umbral de desviación. Eso es `fix/mb-air-deviation-1c`, ya cerrado.
