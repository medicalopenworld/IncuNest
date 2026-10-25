# Diseño

## Contexto

La fuente de la señal ya existe: el hilo de sensores lee el BQ25730 cada 5 s (`charge_status()` en `sensors_Task`, `main.cpp`) y `g_bq_status.ac_present` es el bit AC_STAT del registro de estado. `g_bq_status_valid` dice si esa lectura I2C salió bien; `chargerPresent` dice si el chip existe.

La máquina de alarmas (`alarm_machine`) **no** garantiza una duración mínima de aviso para esta condición: lo único que garantiza tras volver a INACTIVE es la ráfaga mínima de audio de 6.10 (`ALARM_MIN_BURST_MS_HIGH` = 1300 ms). Quien tenga que mantener 10 min lo tiene que hacer antes de la máquina, y es el detector.

## La regla normativa, citada

`docs/alarms_normative_analysis.md` §2.5, sobre **IEC 60601-2-19 201.12.3.103**:

> audible alarm and visible indication shall warn of interruption of the power supply, and "shall be provided for a **minimum time of 10 min**".

El mismo documento lo lee como un requisito de reserva de energía («an energy reserve dedicated to the alarm… a hardware requirement, not a firmware one»), y `shared/include/alarm_policy.h` como «mantener el aviso un MÍNIMO de 10 min».

**Lo que la cita NO dice**: no contiene «o hasta que se restablezca la red, lo que ocurra antes». Con el texto de que dispone el repositorio caben dos lecturas:

1. *Durante la interrupción*: la señal debe poder sostenerse ≥ 10 min mientras dure el corte. Si la red vuelve antes, no hay nada más que avisar.
2. *Literal*: la señal se da durante un mínimo de 10 min desde que se declara, vuelva o no la red.

**Decisión D1: lectura literal (2).** Es la única que no se puede incumplir por defecto de interpretación, y es coherente con el resto del repositorio: la alarma se excluyó del silencio (`alarm_is_silenceable`) precisamente porque «la pausa dura justo eso», es decir, el código ya trata los 10 min como duración obligatoria del aviso y no como capacidad de la batería. El coste es que, tras un corte breve, el aviso persiste hasta 10 min con la red ya presente, y que no se puede silenciar (el operador solo puede reconocerla con ACK, si el HMI lo ofrece). Se acepta porque la alternativa falla en el lado peligroso: un corte de 20 s que el operador no llegó a ver (estaba fuera de la sala) se perdería sin dejar aviso.

El plazo corre **desde la declaración**, no desde el restablecimiento: un corte de 8 min no alarga el aviso a 18. Un nuevo corte confirmado que llega con el aviso aún mantenido por uno anterior **reinicia** el plazo, porque es otra interrupción con su propio mínimo.

La duración es una constante (`MAINS_ALARM_MIN_HOLD_MS`) y el análisis de riesgos puede cambiarla a 0 si adopta la lectura (1) sin tocar la lógica. **Pregunta abierta Q1 para el responsable de riesgos.**

## Decisiones

**D2 — Antirrebote de entrada: 2 lecturas válidas consecutivas (≈ 10 s con el periodo de 5 s), en ambos sentidos.**

- La norma no fija latencia de detección; el cuerpo de 201.12.3.103 habla de avisar de la interrupción, y 10 s sobre un aviso que va a durar ≥ 10 min es un retardo proporcionado.
- Un microcorte de red de milisegundos o unos pocos ciclos lo sostiene el condensador del adaptador y de la entrada del BQ; ni siquiera suele llegar a AC_STAT, y si llegara, cogerlo en **dos** muestreos separados 5 s exigiría que estuviera ausente en ambos instantes. Con 1 sola lectura bastaría un parpadeo coincidente con el muestreo para levantar una alarma ALTA no silenciable durante 10 min: el escenario de falso positivo más caro del sistema. Con 2, hace falta que persista o que se repita justo en las dos.
- 3 lecturas (15 s) no compra nada medible y retrasa un aviso de seguridad.
- La liberación usa el mismo número, por simetría: un rebote en el enchufe no debe alternar el estado. Como el aviso se mantiene ≥ 10 min de todos modos, la liberación solo cuenta cuando ese plazo ya ha vencido.
- La muestra que cuenta es la **válida**; una lectura no válida ni suma ni reinicia la racha (D3).

**D3 — Lectura no válida: no cambia nada.** No declara (un fallo de I2C no es un corte de red) ni retira (un fallo de I2C no es un restablecimiento). Tampoco rompe la racha de confirmación. Un fallo persistente del cargador ya lo cubre el autotest/`HW_error` del cargador, no esta alarma. Consecuencia conocida: si el cargador deja de responder *durante* un corte ya declarado, el aviso se mantiene hasta que vuelva una lectura válida de red presente (falla hacia el lado seguro).

**D4 — Sin cargador: nunca declara.** Con `chargerPresent == false` no hay fuente de la señal y la placa no puede saber si hay red. Se anota **una vez** en el log, y la condición se declara `false` (idempotente).

**D5 — `vbat_mv == 0` invalida la lectura.** Con la placa en marcha, la tensión de batería leída del ADC es siempre > 0; un 0 significa que el ADC del BQ aún no ha convertido (arranque, tras `init_BQ25730`) y el resto de la trama no es de fiar. Se trata como lectura no válida (D3). Es la única salvaguarda de plausibilidad con `vbat_mv`; **no** se usa la tensión de batería para inferir la red (una batería baja con red presente y en carga es normal).

**D6 — Un único dueño del estado.** El detector se **alimenta** únicamente desde `sensors_Task` (junto a la lectura del BQ) y se **consulta** desde `securityCheck()` con una función `const` (`mains_detector_condition`) que no muta. El vencimiento del plazo de 10 min se calcula en la consulta a partir de `since_ms` y `now`, sin escribir estado, así que no hay dos hilos escribiendo la misma estructura. Los campos son enteros/booleanos alineados de una palabra; la peor carrera es leer la condición un ciclo tarde, que el antirrebote ya absorbe.

**D7 — Un único punto de entrada: `mains_present_now()`.** Función en `main.cpp` que devuelve `{valid, present, vbat_mv}`. Hoy es solo `g_bq_status.ac_present` con `chargerPresent && g_bq_status_valid`. Es lo único que hay que tocar para añadir fuentes.

## Cómo encajará el pin de la V19

La rev. V19 (rama `feat/mb-hw19`, aún sin mergear) añade `MAINS_OK_PIN` (IO42, CHRG_OK del BQ25730) y un getter `bool mainsPresentPin(void)`. Este cambio **no depende** de esa rama. Cuando llegue, solo cambia `mains_present_now()`:

```
valid   = (chargerPresent && g_bq_status_valid) || HAS_MAINS_OK_PIN
present = (!regValid || reg.ac_present) && (!HAS_MAINS_OK_PIN || mainsPresentPin())
```

Es decir, **AND sobre las fuentes válidas: la red cuenta como presente solo si todas las fuentes que responden dicen que lo está.** Por qué AND y no OR:

- El fallo que importa es *no avisar de un corte real* (falso negativo), que incumple 201.12.3.103; el contrario (falso positivo) es una molestia que el operador ve. Con OR, una fuente atascada en «presente» enmascara el corte; con AND, cualquiera de las dos que vea el corte lo declara.
- Una fuente atascada en «ausente» daría una alarma permanente: se vería en la primera prueba de banco y se corregiría; un atasco en «presente» no se vería nunca.
- Ambas miden la misma cosa (el comparador de entrada del BQ: AC_STAT por I2C, CHRG_OK por pin), así que en uso normal coinciden; el retardo propio de CHRG_OK al enchufar (≈ 1,3 s en la hoja del BQ) lo absorbe el antirrebote D2.

La ventaja real del pin es que **no pasa por I2C**: con el bus colgado, la lectura del registro es no válida (D3) y el pin sigue decidiendo. Eso es lo que justifica que `valid` sea un OR de las fuentes y `present` un AND de las que responden. El pin se puede muestrear más rápido que 5 s, pero el detector cuenta *muestras*, así que si se acelera hay que convertir el antirrebote a tiempo (≥ 10 s); se anota para entonces. **A confirmar en banco con la V19.**

## Indicador «en batería» en el HMI

- **Fuente**: `(alarmBitmask >> ALARM_MAINS_INTERRUPTION) & 1`, con la guarda de «valor nulo» (`(uint32_t)-1`) y sin enlace (`Display_IsBoardLinkLost()`) = oculto: sin dato fresco no se afirma nada, igual criterio que el indicador de conectividad. Cubre PENDING/ACTIVE/SILENCED/ACKED, que es lo que significa el bitmask. Sin campo de protocolo nuevo.
- **Semántica**: el indicador sigue la **condición señalizada**, no la presencia física de red. Mientras dure el plazo mínimo tras volver la red seguirá mostrándose. El texto dice «batería», pero la señal es «aviso de corte de red activo»; se acepta porque es coherente con el pop-up y no añade un segundo estado que desincronizar. Documentado en `docs/alarms.md`.
- **Sitio**: Main, a la derecha del reloj *dentro* del botón del reloj (hay ≈ 80 px libres; el heading no tiene un slot libre: ayuda, reloj, conectividad, candado, bebés, alarmas y ajustes lo llenan, con 19 px de holgura entre ellos). Lock: el slot de ayuda (`HEADING_SLOT0_HELP`), vacío en esa pantalla por diseño. Icono `LV_SYMBOL_BATTERY_2` y una palabra corta, ámbar sobre fondo azul (Main) y sobre fondo oscuro (Lock).
- **Refresco**: `on_battery_heading_update()` en el mismo bucle que `clock_update()`, con caché «sin cambios» (señal + idioma) para no repintar a 20 Hz. Solo toca widgets de LVGL; ninguna I2C ni NVS dentro (ARQ-LOCK-001).
- **Idioma**: el texto sale de `TR(STR_ON_BATTERY)` en la pasada de refresco; la caché incluye el idioma, así que `UI_ApplyLanguage()` no necesita tocarse.

## Riesgos

- **Hardware (no resuelto aquí)**: sin reserva de energía el aviso no puede sostenerse 10 min si la batería no aguanta; y si zumbador o micro cuelgan de la entrada de red y no de VSYS, no suenan. Verificación de banco, listada en tasks.md.
- **Falso positivo por AC_STAT**: si el comparador del BQ se sitúa mal con un adaptador flojo, la alarma saldrá con la red conectada. El antirrebote lo mitiga; solo el banco con la fuente real lo descarta.
- **El aviso no se silencia**: un corte breve deja hasta 10 min sonando sin vía para callarlo salvo ACK. Consecuencia directa de D1.
