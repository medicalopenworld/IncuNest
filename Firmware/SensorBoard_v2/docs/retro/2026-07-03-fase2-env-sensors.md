# Retro — Fase 2 · Sensores ambientales (`env_sensors`)

**Fecha de cierre:** 2026-07-03
**Change OpenSpec:** `openspec/changes/archive/2026-07-03-sb-phase2-env-sensors/`
**Rama:** `feat/sb-phase2-env-sensors` (encadenada sobre Fase 1)

## Qué se hizo

Componente `env_sensors` (3× SHT40 en dos buses `i2c_master`, ALS por ADC con cali curve-fitting, `sensor_task` prio 4, evento `sensor_data` con redundancia posicional — ADR-0002) + registro agnóstico de disponibilidad en `usb_comm` volcado en `status`. 17 TEST_CASE nuevos.

## Aprendizajes

1. **"CRC válido" no es "dato válido"** — la review de seguridad lo marcó bloqueante: un CRC-8 de 8 bits colisiona ~1/256 y un sensor con fallo interno emite datos bien formados. En un lazo de control clínico hace falta un **gate de plausibilidad física** (rango operativo del sensor) además del CRC. Patrón a repetir en Fases 3-5 (dBA, lux calibrado).
2. **Los invariantes de formato hay que blindarlos donde se emiten, no donde se producen.** Hoy ningún camino genera NaN, pero `%.1f` con NaN emitiría JSON roto con CRC bueno; `isfinite` en `append_val` cuesta una línea y protege contra todo productor futuro.
3. **Si el diseño promete un handle compartido, la implementación debe persistirlo el mismo día** — el bus I2C principal se creaba en una variable local; la Fase 5 no habría podido añadir el SCCB sin reabrir `env_sensors.c` (el driver no permite recrear un bus). El code-reviewer lo cazó contrastando `design.md` contra el código: valida el hábito de escribir el diseño antes.
4. **Fail-closed también debe ser observable**: `build_status` devolviendo 0 con la tabla llena era seguro pero mudo (el host solo vería timeout). Log + test del techo de payload documentan el límite real (8 nombres largos no caben en 256 B).

## Diferido (seguimiento)

- **Recuperación de bus I2C colgado** (9 pulsos SCL / re-init tras N fallos): un latch en el bus IO41/42 tumba sht0+sht1 a la vez hasta reboot — la redundancia 2-de-3 queda coja. Candidato a mini-loop propio antes de la Fase 5.
- Pool estático para cJSON (arrastrado de Fase 1).
- Calibración del ALS (`SB_ALS_UV_PER_LUX` es placeholder) — necesita luxómetro y la resistencia de carga real del esquema.
