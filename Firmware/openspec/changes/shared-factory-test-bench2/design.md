## Context

Tercera vuelta tras el banco del 2026-09-06 (ver `proposal.md`). El hallazgo
estructural es que **la motherBoard no tiene mutex de bus I2C**: `Wire`
serializa cada `beginTransmission/endTransmission` o `requestFrom`, pero no
una secuencia "escribir registro, leer respuesta". `sensors_Task` corre cada
1 ms y `PowerManagement_Task` cada 5 s sobre el mismo bus; una tarea nueva
que lea el BQ25730 o el ADS1110 por su cuenta se entrelaza con ellas. En el
autotest de arranque esto no pasaba porque no existía ninguna tarea todavía.

## Goals / Non-Goals

**Goals**: que ningún test de fábrica compita por el bus I2C con las tareas
de producción; que el test ambiental refleje la topología real (SensorBoard
**o** sensor ambiental); que lo opcional sea aviso, nunca error; que el
operario vea el progreso y un veredicto único.

**Non-Goals**: añadir un mutex de I2C a la motherBoard (cambio transversal a
`sensors_Task`, `PowerManagement_Task`, `initHardware`; rama propia); tests
de humidificador y de audio del display (vuelven cuando el jig pueda medirlos).

## Decisions

### D1. Los tests leen estado cacheado, no el bus

`charger` espera `g_bq_status_valid` (lo refresca `PowerManagement_Task` cada
5 s; plazo 12 s); `skin_adc` usa `skinProbeLastReading()` y
`lastSuccesfullSensorUpdate[SKIN_SENSOR]`; `ina3221` usa los flags de
presencia; `env_sensor` y `sb_env` usan `in3.temperature[]` y los sellos de
frescura. Las únicas excepciones son `testStandByCurrent()` y
`actuatorsTest()`, que ya funcionaron en banco y cuyo reemplazo no cabe aquí.
Coste: un test depende de la cadencia de otra tarea (5 s en el peor caso);
aceptado, los plazos lo cubren.

### D2. Un solo test ambiental con tres caminos

`env_sensor` PASA con el primer camino que entregue lectura fresca y finita
en rango: `usb`, `i2c` o `sht4x`. La cascada `sb_usb` solo se marca OK si el
camino fue USB, para que los `sb_*` sigan haciendo SKIP donde no aplican.
`sb_env` compara con el exterior solo si hay SHT4x con lectura fresca.

### D3. Aviso, no error, para todo lo que dependa del entorno

`gsm_signal` se une a `gsm_net`, `wifi`, `tb_provision` y `time` en WARN al
agotar el plazo. Siguen siendo FAIL solo `gsm_at` (el módem no responde) y
`gsm_sim` (sin `+CPIN: READY`), que son hardware o SIM ausente.

### D4. `HMI_I2C` decide con evidencia del arranque

Un `endTransmission()` vacío no prueba nada con el STC8H1K28 ni con el GT911.
El init del touch (con reintentos) y la secuencia del backlight (5 reintentos)
ya son la prueba de que ambos hablan; se exponen como getters y el sondeo de
direcciones queda en el detalle.

### D4b. Timeout cooperativo por test, en ambas placas

En la MB, `FTEST_TEST_TIMEOUT_MS` = 90 s por cuerpo, consultado en los mismos
bucles de espera de 250 ms que el ABORT: el test termina en FAIL `timeout` y
la batería continúa. En el display, `FTEST_ROW_TIMEOUT_MS` = 100 s por fila
sin cambio de estado como red de seguridad si la placa se cuelga en ese test;
por encima, el timeout de silencio de 120 s ya existente lleva al resumen.
Un cuerpo bloqueado en una llamada no cooperativa solo lo cubre el task WDT
(75 s → reinicio), motivo adicional para D1.

### D5. Páginas, barra y veredicto

Páginas de 3×N botones sin scroll, con `<`/`>` e indicador; la página sigue
al test en curso salvo que el operario haya paginado a mano. Barra de
progreso (terminales / esperados, SKIP cuenta como terminal). Veredicto único:
`HW OK` si ningún FALLA, `HW ERROR` si alguno o si la placa no respondió /
rechazó / se perdió; los avisos no lo cambian. Se persiste en `hmi_ftest`.
Código de colores único en botones, detalle y veredicto: **blanco** en
curso, **verde** OK, **amarillo** aviso, **rojo** error.

## Risks / Trade-offs

- [Estado cacheado rancio] → cada test exige sello de frescura (< 5 s) además
  del valor; un sensor muerto no pasa por tener un valor viejo.
- [Renumerar ids otra vez] → sin placas en campo; `PROTOCOL.md` v2.3.2.
- [Omitir humidificador y audio deja hardware sin cubrir] → explícito en
  `docs/hardware.md`, con la condición para reactivarlos (jig con medida).
- [Sin mutex I2C, `actuatorsTest()` sigue compitiendo con `sensors_Task`
  por el INA3221] → funcionó en banco; queda anotado como riesgo para la rama
  del mutex.
