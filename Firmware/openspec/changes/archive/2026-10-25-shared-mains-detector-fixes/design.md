# Diseño

## A3 — plazo mínimo 0 (decisión de producto)

`docs/alarms_normative_analysis.md` §2.5 lee 201.12.3.103 («shall be provided for a minimum time of 10 min») como un requisito de **autonomía**: la alarma tiene que poder sostenerse 10 min sin red, lo cual es hardware (reserva de energía, zumbador y micro desde VSYS). No es la obligación de mantenerla con la red ya presente. Mantenerla 10 min tras el retorno:

- enmascara otras alarmas ALTAS (el zumbador ya suena a ALTA y no distingue),
- muestra «BATERÍA» en falso,
- y contradice 6.10 (la señal acompaña a la condición).

**Decisión:** `MAINS_ALARM_MIN_HOLD_MS = 0u`; la condición se retira cuando se confirman 2 lecturas de red presente. La constante y el mecanismo (plazo desde la declaración, reinicio en corte nuevo, cierre del plazo vencido) se conservan y se prueban con una constante de test (`mains_detector_init_hold`): revertir es poner `600000u`. Sustituye a D1 de `shared-mains-detector`.

## A1 — el cargador detectado no es el cargador configurado

`chargerPresent` significaba «configurado y legible ahora» y se usaba como «hay cargador». Se separan:

- `chargerDetected`: el chip contestó por I2C (lectura del ID) alguna vez; se fija en la primera configuración y **no baja nunca**.
- `chargerPresent`: configuración aplicada. `init_BQ25730()` (arranque) lo baja al empezar y lo sube al terminar bien; `reconfigure_BQ25730()` (retorno de la red) comparte el cuerpo y **no toca** `chargerPresent` ni `_initialized`, así `charge_status()` sigue leyendo durante la reconfiguración.

El bucle del cargador y `checkMainsInterruption()` pasan a `chargerDetected`; la validez de la lectura que alimenta al detector es solo `g_bq_status_valid`. Si la reconfiguración falla se registra con `logE`, no se anota la red como presente (el flanco se repite y se reintenta cada 5 s) y cuenta como fallo técnico del cargador (A2). El log «no se vigila la red» solo sale con `chargerDetected == false` y como `logE`.

## A2 — ceguera visible

`mains_detector_blind(d, charger_detected, now)`: `charger_detected` y más de `MAINS_DETECTOR_BLIND_MS` (30 s ≈ 6 lecturas) desde la última lectura válida; antes de la primera cuenta desde el arranque. Pura, `const`, probada. `sensors_Task` la evalúa cada pasada junto con la reconfiguración fallida y, al cambiar, escribe `g_charger_comm_fault` y hace `logE`. Se publica como `charger_comm_fault` en el grupo DIAG de GPRS (WiFi no tiene grupo DIAG hoy). Con la alarma declarada y el detector ciego, la alarma **se mantiene** (lado seguro).

**No se añade `AlarmId`.** `alarm_ids.h` termina en 19 y otra rama abierta añade el id 20: un id nuevo aquí colisionaría. La alarma técnica queda como deuda en `known_issues.md` hasta integrar las ramas.

## M1 / M2

- Main: solo icono de 14 pt, a la derecha en la fila de la fecha (la fecha acaba en ~95 px; la hora de «Sin hora»/«Sans heure» a 26 pt llena la fila superior). La palabra se queda en Lock. Verificación de banco con captura en ES y FR.
- `vbat_mv = adc*64 + 2880` nunca es 0: el filtro no filtraba nada. Se elimina con sus tests; la firma de `mains_detector_sample()` pierde el parámetro.
