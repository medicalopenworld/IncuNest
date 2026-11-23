# Revisión del interfaz entre `incunest_afe4490` e IncuNest

Borrador de trabajo (2026-11-16). No es documentación del firmware.

Compara lo que la librería garantiza con lo que la motherBoard supone. La
librería se ha leído en la v0.122; IncuNest fija la v0.119 en
`motherBoard/platformio.ini`. Las líneas citadas de la librería son de la
v0.122.

## 1. Por dónde pasa el interfaz

| Pieza | Librería | IncuNest |
|---|---|---|
| Configuración | `setHgacEnable`, `setPPGDispMode`, `setPPGDispFilter`, `begin` | `initSPO2()` en `src/drivers/SPO2.cpp` |
| Datos | `getData(AFE4490Data&)`: cola de 10 muestras, 500 Hz, sin bloqueo | `SPO2_Task` la vacía cada 1 ms y copia cada muestra a `g_spo2_data` |
| Reparto | no interviene | `g_spo2_data` es una global `volatile` sin bloqueo que leen CommTask, la telemetría wifi y GPRS, el test de fábrica y `/debug/state` |
| Diagnóstico | `runAfeDiagnostics`, `getTimingConfig` | `factory_test_hw.cpp` |

IncuNest no usa ninguna de las funciones que la v0.122 elimina
(`setHR1DcTauS`, `setHR1LpCutoffHz`): subir de la v0.119 a la v0.122 no rompe
la compilación. Lo que cambia es el contrato de datos (apartado 4).

## 2. Qué garantiza la librería en cada campo

Regla de fondo, que hoy no está escrita en un solo sitio: **un valor que no
se publica vale NaN, y su SQI vale 0**. El SQI no dice si hay valor.

| Campo | Sin valor | Con valor | ¿`sqi > 0` equivale a «hay valor»? |
|---|---|---|---|
| `spo2` | NaN, con el motivo en `spo2_hold_reason` | finito, entre `spo2_min` y `spo2_max` | **No.** `spo2_sqi` es una rampa del PI entre 0,5 y 2 %; vale 0 con PI ≤ 0,5 % aunque haya SpO2 |
| `pi` | NaN (ventana llenándose, sonda no aplicada, DC ≈ 0) | finito y positivo, sin tope | no tiene SQI |
| `hr1` | NaN | finito, entre `hr_min_bpm` y `hr_max_bpm` | **No.** `hr1_sqi` es `1 − CV/0,15`; vale 0 con un ritmo irregular aunque haya HR1 |
| `hr2`, `hr3` | NaN | finito, en rango | **Sí**: la librería escribe `sqi > 0 ? valor : NaN` |
| `ppg_disp` | 0 (sonda no aplicada o calentamiento) | [−1, +1] en `NORM_PI` | — |
| `rsqi` | 0 | 1 = sonda aplicada y `diag_code == 0` | — |
| `probe_state` | — | `PROBE_APPLIED` es el único estado con medida | — |

Dos excepciones a la regla del NaN:

- **Arranque.** Hasta el primer resultado de cada algoritmo, `hr2` y `hr3`
  valen 0,0 y no NaN (lo dice el propio comentario de la puerta de pulso de la
  SpO2). Un consumidor que solo mire `isnan()` tomaría un 0 por valor.
- **Parada del AFE.** Si el AFE deja de dar muestras, la librería no publica
  nada: es IncuNest quien fuerza `probe_state = PROBE_DISCONNECTED` en
  `g_spo2_data` (`afe_watchdog`). Los demás campos se quedan con el último
  valor; los consumidores dependen de mirar antes `probe_state`.

## 3. Qué supone IncuNest, uso por uso

| Uso | Dónde | Qué comprueba | Estado |
|---|---|---|---|
| Onda `CTRL,PPG` | `CommTask.cpp` | `probe_state == APPLIED`, una lectura de `ppg_disp` | Correcto |
| HR de `CTRL,VIT` | `CommTask.cpp` | `hr2 > 0`, `hr3 > 0`, acuerdo < 8 lpm, SQI ≥ 0,8 / 0,5, rachas | Correcto, y cubre el 0 del arranque. No usa HR1 |
| SpO2 de `CTRL,VIT` | `CommTask.cpp` | finita y en (0, 100], una lectura | Correcto desde el PR #33 |
| PI de `CTRL,VIT` | `CommTask.cpp` | nada | **Desajuste A** |
| SpO2 y PI de telemetría | `Wifi_OTA.cpp`, `GPRS.cpp` | finitos, una lectura | Correcto desde el PR #33 |
| `spo2_sqi` de telemetría | ídem | se lee aparte de la SpO2 | **Desajuste B** (leve) |
| HR1, HR2, HR3 de telemetría | ídem | `hrN_sqi > 0`, valor y SQI leídos por separado | **Desajustes B y C** |
| Captura PPG | `PpgSnapshot` | `probe_state` y `rsqi` | Correcto |
| Test de fábrica | `factory_test_hw.cpp` | `probe_state` | Correcto |
| `spo2_hold_reason` | — | no se usa | **Oportunidad D** |

### Desajustes

- **A. El PI viaja sin validar.** Si no hay PI, `CTRL,VIT` lleva `nan`. Está
  documentado en `PROTOCOL.md` y el HMI lo descarta, pero es el único campo del
  mensaje donde «sin valor» no es 0. Un infinito se pintaría.
- **B. Lectura partida.** `g_spo2_data` se reescribe 500 veces por segundo sin
  bloqueo. Quien lee dos campos (SQI y valor) puede leer cada uno de una
  muestra distinta: SQI > 0 de una y NaN de la siguiente. En la telemetría de
  HR1, HR2 y HR3 eso acaba en `(int)(NaN + 0.5f)`, que es comportamiento
  indefinido. Es raro, pero no imposible.
- **C. HR1 filtrada por su SQI.** La telemetría solo envía HR1 con
  `hr1_sqi > 0`, así que no llega la HR1 de un ritmo irregular (CV ≥ 15 %),
  que la librería sí publica. Es el mismo supuesto que se corrigió para la
  SpO2. Puede ser lo que se quiere, pero hoy no es una decisión escrita.
- **D. Motivo de la retención sin usar.** `spo2_hold_reason` dice por qué no
  hay SpO2 (ventana llenándose, sin pulso, PI alto, fuera de rango). Es justo
  lo que necesita la tarea de valores no fiables para distinguir «buscando
  pulso» de «sin medida».

### La raíz

El supuesto `sqi > 0` apareció en tres sitios de IncuNest y en PulseNest
porque el contrato invita a ello:

- En la cabecera, `hr1_sqi` se describe como «0 = arrhythmia/artefact/invalid»
  y `spo2_sqi` no dice que la SpO2 puede ser válida con SQI 0.
- En la especificación, §2.1 va por detrás de la cabecera: describe
  `spo2_sqi` como «0 = no finger», no lista `spo2_r_method2_reg_corr` ni
  `spo2_hold_reason` y no menciona el NaN.
- HR2 y HR3 sí cumplen `sqi > 0 ⇔ hay valor`; SpO2 y HR1 no. Dos reglas
  distintas bajo el mismo nombre de campo.

## 4. Lo que trae subir de la v0.119 a la v0.122

| Versión | Cambio | Efecto en IncuNest |
|---|---|---|
| v0.120 | PI por mediana de subventanas (PI-METHOD-2), ventana de 6 a 7 s | El PI ya no sube tras un movimiento; tarda 1 s más en aparecer. La clave de telemetría del PI cambia de definición otra vez: anotarlo en `telemetry_keys.h` |
| v0.121 | HR2 sobre 6 s en vez de 8 | La primera HR en pantalla llega unos 2 s antes |
| v0.122 | Filtro común de HR1 | HR1 válida más a menudo y antes; solo afecta a la telemetría |

## 5. Contrato propuesto (tercera versión)

La primera versión tenía ocho reglas. Tras los comentarios de Alex quedan
cuatro para las magnitudes clínicas, y lo demás cambia de sitio: la
congelación pasa a una capa de valores de pantalla (5.2), y lo que era la
regla 8 deja de ser contrato porque es una obligación de IncuNest (5.3).

### 5.1 Magnitudes clínicas

Son `spo2`, `pi`, `hr1`, `hr2` y `hr3`: lo que calcula cada algoritmo, tal
cual. Sirven para pruebas, validación, calibración y telemetría. El nombre es
el de la librería («clinical output quantities»). En versiones anteriores de
esta nota se llamaban «valores de medida», que se confundía con las seis
señales del AFE.

Las cuatro reglas hablan solo de estas cinco magnitudes. Las seis señales
crudas del AFE y la onda quedan fuera (ver el final de este apartado).

1. **Una magnitud es un número finito o NaN. NaN significa «no hay valor».**
   No hay otra forma de decirlo: nunca 0, tampoco en el arranque, y no hace
   falta mirar el SQI ni `probe_state`.
2. **Una magnitud publicada está siempre dentro de su rango declarado**
   (`getConfig()`): 30–260 lpm, 70–100 %, PI ≥ 0.
3. **Una magnitud puede llevar al lado una nota de calidad, su SQI, entre 0
   y 1. La nota solo significa algo cuando la magnitud tiene valor.** Hoy la
   llevan todas menos el PI; si un método de PI futuro (por ejemplo con un
   peine espectral) da una nota de calidad, `pi_sqi` entra bajo esta misma
   regla. Primero se
   mira si hay valor; si no lo hay, el SQI se ignora.

   | `spo2` | `spo2_sqi` | Cómo se lee |
   |---|---|---|
   | NaN | 0 | No hay SpO2. El SQI no se mira |
   | 97 | 0 | Hay SpO2, sacada de la señal más débil con la que la librería todavía publica (PI ≤ 0,5 %) |
   | 97 | 1 | Hay SpO2, sacada de una señal fuerte (PI ≥ 2 %) |

   La regla existe porque hoy el 0 aparece en las dos primeras filas y es
   fácil tomarlo por «no hay valor»: es el fallo que tuvo IncuNest.
4. **La librería nunca deja un resultado viejo en el sitio de uno nuevo que
   no ha salido.** Cada algoritmo recalcula su magnitud cada cierto tiempo,
   mirando los últimos segundos de señal:

   | Magnitud | Recalcula cada | Mirando los últimos |
   |---|---|---|
   | `spo2` | 100 ms | 6 s |
   | `pi` | 100 ms | 7 s |
   | `hr2` | 0,5 s | 6 s |
   | `hr3` | 0,5 s | 10 s |
   | `hr1` | cada latido | 5 latidos |

   Cada recálculo escribe un resultado nuevo o NaN. Entre dos recálculos, las
   muestras llevan el último resultado: eso no es congelar. Congelar sería
   que un recálculo fallara y se siguiera publicando el resultado anterior.
   (En el borrador anterior esos «últimos segundos de señal» se llamaban
   «ventana», el término de la librería: `spo2_window_s`, `pi_window_s`. No
   es el PRP.)

**Las seis señales crudas** (`led1`, `led2`, `aled1`, `aled2`, `led1_sub`,
`led2_sub`) no entran en estas reglas. Son enteros, hay un juego nuevo en cada
PRP (2 ms a 500 Hz) y siempre son un número. Comportamiento actual, leído en
el código: durante el asentamiento tras un cambio de ganancia (milisegundos)
la librería repite el último juego válido y lo marca en `diag_code`.

### 5.2 Valores de pantalla (propuesta de Alex, por diseñar)

`spo2_disp`, `hr_disp`, `pi_disp`: el número que un monitor debe enseñar, con
su propio contrato. La librería ya hace lo mismo con la onda (`ppg_disp`).

Hoy nadie congela nada. Las rachas de CommTask no mantienen el número: con un
segundo malo se envía 0 y el HMI pinta «--». Solo exigen dos segundos buenos
seguidos para empezar a mostrar. El HMI pinta lo que recibe. (La versión
anterior de esta nota decía que las rachas congelaban: era un error.)

Lo que condiciona mantener un número en pantalla está todo en la librería:

- **La cadencia y la latencia de cada algoritmo** (ventanas de 6 a 10 s,
  actualización cada 100 ms o cada intervalo).
- **Las medidas con referencia.** La librería ya probó a mantener la última
  SpO2 y empeoró (justificación §17). El caso es SUBJ10, un neonato que se
  movía:
  - Sin la puerta de pulso se publicaba el 81 % del tiempo, con un error
    típico de 16 puntos frente al monitor de referencia.
  - Con la puerta (SpO2 solo si HR3 reconoce un pulso; si no, NaN) se publica
    el 2,6 % del tiempo, con un error de 2,1.
  - Se probó a no pasar a NaN al instante, sino seguir enseñando el último
    valor 5 o 10 s. Lo publicado subió al 7–10 % del tiempo, y lo añadido
    tenía 12 a 13 puntos de error.
  - Motivo: HR3 mira 10 s de señal, así que tarda unos segundos en dejar de
    ver pulso cuando empieza el movimiento. En esos segundos la SpO2 ya sale
    mal y todavía se publica. Ese valor es el «último» que se mantendría.

  Mantener el último valor es mantener el más sospechoso. Solo se puede
  ajustar con capturas, y hoy hay un único sujeto en movimiento con
  referencia.
- **Los requisitos externos**, que la librería ya tiene recogidos (su
  especificación §5 y §13 y su justificación §4 y §17):
  - OMS/UNICEF 2019 (especificaciones técnicas de compra, §6.5 y tabla
    A1.11, pulsioxímetros de sobremesa para UCIN): actualización de dato
    válido **≤ 10 s**. Es el límite que manda para IncuNest.
  - ISO 80601-2-61: indicación obligatoria si el valor lleva más de 30 s sin
    actualizarse. No publicar con señal inadecuada ya cumple como indicador.
  - Referencia de otros fabricantes: Philips avisa a los ~10 s; Nellcor no
    enseña nunca un valor de más de 40 s.

No se ha encontrado ningún dato que sea de IncuNest y que la librería
desconozca. Lo único ajeno a la librería es la preferencia del operador (por
ejemplo el tiempo de promediado), que entraría con un `set…()`.

Esta capa absorbe tres cosas que estaban sueltas: la fusión de HR2 y HR3
(`hr_disp` es el `pr` del apartado 7), la tarea de valores no fiables y la
base de las alarmas.

Boceto del contrato, por discutir:

- NaN = el monitor pinta guiones. Se cumplen las reglas 1 y 2 de 5.1.
- En lugar de SQI y motivo, **un estado por valor** con pocos casos: actual,
  mantenido, sin valor porque se está adquiriendo, sin valor por señal
  inadecuada.
- **Sin SQI** mientras no aparezca un consumidor que lo necesite (Alex,
  2026-11-17).
- La librería garantiza un tiempo máximo de mantenimiento, nunca mayor que
  los 10 s de OMS/UNICEF.

Cautelas:

- Se diseña con capturas, no a ojo (ver el resultado de SUBJ10).
- Hay que decidir sobre qué capa trabajan las alarmas.
- Telemetría: se propone enviar las dos capas; la de magnitudes clínicas para analizar y
  la de pantalla para que quede registrado lo que vio la enfermera.
- La decisión del 2026-10-22 (retener con motivo en vez de publicar con una
  marca de baja calidad) sigue valiendo para las magnitudes clínicas.

### 5.3 Lo que es de IncuNest y no del contrato

Era la regla 8. La librería entrega cada muestra como una foto: todos los
campos son del mismo instante. Dos cosas dependen solo de cómo la usa
IncuNest:

- **Repartir la foto entera.** `SPO2_Task` sobrescribe una variable global
  500 veces por segundo y las demás tareas la leen campo a campo. Ejemplo: la
  telemetría lee `hr2_sqi` (0,9), la global se sobrescribe, y al leer `hr2`
  ya es NaN: se convierte a entero un NaN. Solución: copiar la muestra entera
  de una vez y trabajar sobre la copia.
- **Si el AFE se para, todo pasa a «sin valor».** La librería solo produce
  una muestra cuando el chip entrega datos. Si el chip se cuelga no hay
  muestras nuevas y la global conserva la última: una SpO2 de 98 que ya no se
  mide. Hoy el vigilante de IncuNest lo detecta por tiempo pero solo cambia
  `probe_state`.

Los formatos de salida también son de IncuNest y cada uno aplica su convención
en un solo punto: en `CTRL,VIT`, 0 = sin valor; en la telemetría, la clave no
se envía.

### 5.4 Para qué sirve hoy un SQI

Usos reales, hoy:

- **La fusión de HR2 y HR3** en CommTask: pondera con los SQI y exige 0,8 y
  0,5. Es el único uso en el producto, y se va a la librería (5.2).
- **Telemetría**: `SpO2_SQI` y `HRn_SQI` van a ThingsBoard para filtrar las
  series al analizarlas después.
- **Validación en el laboratorio**: comprobar si el error contra la referencia
  crece cuando baja el SQI. Es la prueba de que un SQI vale algo.

Usos posibles que no existen: atenuar el número o añadirle una marca en
pantalla (lo hacen Masimo y Nellcor con su indicador de señal) y retrasar una
alarma cuando la calidad es baja.

Los SQI actuales dan poco para graduar: `spo2_sqi` es una función del PI, así
que no añade nada a enseñar el PI, y `hr3_sqi` es casi binario (mediana 1,00
y percentil 10 ≥ 0,98 cuando hay valor, según las medidas de la librería). La
librería tiene pendiente un análisis conjunto de los SQI. Por eso en este
contrato el SQI queda como dato de diagnóstico y el producto usaría el estado
de los valores de pantalla.

### 5.5 Quién usa el motivo de la retención

`spo2_hold_reason` solo lo usan herramientas de desarrollo: el firmware de
PulseNest lo envía en su trama, el programa de laboratorio lo enseña en el
título de la SpO2 y lo guarda en el CSV, el ejecutor fuera de línea lo saca
como columna, y lo comprueban los tests de la librería. IncuNest no lo usa.

Un producto no necesita ocho motivos. Necesita como mucho tres casos: sonda
no aplicada (ya lo da `probe_state`), adquiriendo y señal inadecuada. Eso es
el estado de 5.2. Conclusión: el motivo detallado se queda como diagnóstico
de las magnitudes clínicas, solo para la SpO2, y no se amplía a los demás valores.

### 5.6 A qué frecuencia se lee cada dato

Hay dos clases de dato, y piden cosas distintas:

- **Flujo**: cuenta cada muestra y el ritmo debe ser regular. La onda y los
  datos crudos.
- **Estado**: solo cuenta el último valor. Las cifras, el estado de la sonda
  y los diagnósticos.

Lo que hace IncuNest hoy (leído en el código):

| Dato | Clase | Quién lo lee | Ritmo hoy |
|---|---|---|---|
| Muestra completa | — | `SPO2_Task` vacía la cola de la librería y la copia a la global | 500 Hz (sondeo cada 1 ms) |
| `ppg_disp` | Flujo | `CTRL,PPG` y la captura PPG | 50 Hz |
| `probe_state` | Estado | Cada envío de onda; elegir entre `CTRL,VIT` y `CTRL,PROBE` | 50 Hz y 1 Hz |
| `hr2`, `hr3`, `spo2`, `pi` y sus SQI | Estado | `CTRL,VIT` | 1 Hz |
| Todas las cifras y sus SQI | Estado | Telemetría | Cada 5 s por wifi |
| `rsqi` | Estado | Puerta de la captura PPG | Bajo demanda y cada 5 min |
| `diag_code`, crudos | Estado | Traza de depuración | Ocasional |

IncuNest lee a 500 Hz, pero lo más rápido que necesita son 50 Hz. Los 500 Hz
vienen de que `getData()` es una cola con todas las muestras: es un interfaz
de flujo, y no hay otro. Por eso IncuNest se fabrica el «último valor» con
una global sin bloqueo, que es el origen del problema de 5.3.

Lo que necesita un consumidor, IncuNest o una consola hospitalaria:

| Dato | Ritmo necesario | De dónde sale |
|---|---|---|
| Onda (`ppg_disp`) | 50 Hz, regular | Decisión de IncuNest: banda del filtro de pantalla hasta 20 Hz, Nyquist a 25 Hz. No hay cifra de consolas comerciales |
| `probe_state` | Lo que importa es el retardo, no el ritmo: ≤ 0,2 s para quitar la onda y las cifras | Estimación. Leerlo con la onda lo cumple de sobra |
| Cifras de pantalla (`spo2_disp`, `hr_disp`, `pi_disp`) | 1 Hz | Nellcor actualiza cada 1 s y la ISO pide 1 Hz al oxímetro de referencia (anexo JJ), según la justificación de la librería. 0,5 Hz también cumple los 10 s de OMS/UNICEF |
| Alarmas | 1 Hz, sobre las cifras de pantalla | Por decidir con la tarea de alarmas |
| Telemetría y tendencias | De 5 s a 1 min | Uso actual |
| Crudos para captura y validación | 500 Hz, sin pérdidas | PulseNest |
| Latido (tono de pulso) | Un aviso por latido, con retardo < 0,1 s | Lo tienen las consolas; IncuNest no, y la librería no publica el latido. Hueco anotado, no se hace ahora |

La librería calcula SpO2 y PI cada 100 ms, HR2 y HR3 cada 0,5 s y HR1 en cada
latido. Leer las cifras a más de 2 Hz no aporta nada.

Consecuencia para el interfaz, por decidir en la fase 2:

- Mantener la cola actual para los flujos.
- Añadir una lectura de estado: la última muestra, copiada entera y con su
  antigüedad, a cualquier ritmo. Resolvería en la librería la copia entera y
  la detección de parada del AFE de 5.3, para todos los consumidores.
- Opcional: la onda ya entregada al ritmo del consumidor. Hoy IncuNest toma
  la última muestra cada 20 ms, que vale porque el filtro corta en 20 Hz.

### Dónde no se cumple hoy

| Regla | Incumplimiento | Lado |
|---|---|---|
| 1 | El 0 del arranque. **Corregido en la v0.123 de la librería** (2026-11-17): la muestra publicada arrancaba con las cinco magnitudes a 0; el test nuevo lo detecta con la sonda aplicada desde la primera muestra (HR2 y HR3 a 0 hasta su primer resultado). En el firmware ninguna muestra de la cola lo llevaba. La global de IncuNest sigue arrancando a 0 | Librería (hecho) e IncuNest |
| 1 | La telemetría decide con `hrN_sqi > 0` | IncuNest |
| 3 | Cabecera y §2.1 describen el SQI 0 como «invalid» o «no finger» | Librería (texto) |
| 4 | HR1 no caducaba. **Corregido en la v0.123 de la librería** (2026-11-17): pasa a NaN cuando el intervalo en curso supera los 2 s (el de 30 lpm). Medido sobre las 19 capturas: caducan 368 de 24 386 valores (1,5 %), ninguno cambia | Librería (hecho) |
| 1 y 3 | HR1 se publicaba con `hr1_sqi` igual a 0 en 8 720 de 24 386 salidas (36 %; 93 % en el neonato que se movía). El banco de PulseNest solo da por publicado un HR1 con SQI > 0, y HR2 y HR3 ya ponían NaN con SQI 0. **Corregido en la v0.123** (Alex, 2026-11-17): HR1 es NaN cuando su SQI es 0. Quedan 15 645 salidas con valor y ninguna cambia | Librería (hecho) |
| 5.3 | `g_spo2_data` se lee campo a campo; en una parada del AFE solo cambia `probe_state` | IncuNest |
| 5.3 | El PI viaja como `nan` en `CTRL,VIT` | IncuNest |
| 5.3 | **Por verificar**: si dejan de llegar `CTRL,VIT`, el HMI parece conservar los últimos números | IncuNest |

## 6. ¿Sirve `spo2_r_method2_reg_corr` para un `spo2_sqi` mejor?

**No.** La librería ya lo estudió y lo descartó con medidas (justificación de
diseño §9 y §17). No se han repetido las medidas; estas son las suyas:

- **No detecta el artefacto de movimiento.** En SUBJ10 (neonato moviéndose) la
  SpO2 erraba 16 puntos con una correlación de 0,96. La regresión del rojo
  sobre el infrarrojo ajusta el movimiento igual de bien que el pulso.
- **Es un límite teórico, no un caso.** Con sangre arterial al 95 % mezclada
  con venosa al 70 %, la correlación no puede bajar de 0,954 por mucho
  movimiento que haya, y el error es de 12,5 puntos.
- **No tiene recorrido para graduar.** En señal buena el percentil 1 de la
  correlación es 0,944. Convertirla en un índice de 0 a 1 haría que un ajuste
  sano de 0,9 se leyera como «media calidad».
- **Subir el umbral no arregla nada.** Con 0,99 se sigue publicando el 11 % de
  SUBJ10, todo con −14 puntos, y se pierde un 10–15 % de los neonatos quietos.

La correlación sirve para lo que ya hace: una puerta (< 0,8 → sin valor) que
descarta ruido sin relación entre los dos canales.

Lo que sí separó la SpO2 buena de la mala fue **identificar el pulso** (la
puerta de HR3, en uso desde la v0.110) y, algo más, **el acuerdo entre HR2 y
HR3** (SUBJ09: error de 2,39 a 1,87 a cambio de un 3–7 % menos de
disponibilidad; quedó «para más adelante»). Un `spo2_sqi` mejor saldría de
ahí, no de la correlación: por ejemplo, la rampa del PI multiplicada por un
factor de acuerdo entre HR2 y HR3. Es una hipótesis: hay que medirla sobre las
capturas con referencia antes de adoptarla.

## 7. ¿La fusión de HR2 y HR3 debe estar en la librería?

**Sí.** Hoy vive en `CommTask.cpp`: acuerdo de menos de 8 lpm, SQI máximo
≥ 0,8 y mínimo ≥ 0,5, media ponderada por SQI, y dos segundos buenos seguidos
para empezar a mostrar.

- Es conocimiento del algoritmo, no de la pantalla. Los umbrales dependen de
  cómo se calculan HR2 y HR3, y cada versión de la librería los mueve (HR2
  pasó de 8 a 6 s en la v0.121) sin que nadie revise la regla de IncuNest.
- No está medida. La librería valida cada cambio contra capturas con ECG; la
  regla de CommTask nunca ha pasado por ese banco.
- Cada consumidor repite o diverge. La pantalla enseña la HR fusionada, la
  telemetría envía HR1, HR2 y HR3 por separado, y PulseNest no tiene fusión.
  Lo que ve la enfermera no queda registrado en ThingsBoard.
- La librería ya lo tiene previsto: su hoja de ruta (§5.5) habla de una fase
  que combina métodos, y la puerta de pulso de la SpO2 ya mira HR3 por dentro.

Con la capa de pantalla de 5.2, el valor fusionado es `hr_disp`. `hr1`, `hr2`
y `hr3` siguen como magnitudes clínicas.

Dos cautelas:

- **En dos pasos.** Primero llevar a la librería la regla actual tal cual,
  comprobando sobre las capturas que da lo mismo que CommTask. Después
  mejorarla con medidas. Así el primer paso no cambia lo que ve el usuario.
- **Toca decisiones abiertas de la librería** (justificación §22.1): el nombre
  (`pr` frente a `hr`) y qué lee la puerta de pulso de la SpO2 si existe un
  valor combinado.

## 8. Cambios propuestos

### Fase 1: magnitudes clínicas (pequeña)

Alex decide el 2026-11-16 hacerla primero.

Estado de la librería a 2026-11-17: los tres puntos están hechos en el árbol
de trabajo como v0.123 (commit `86d112a`, publicada en GitHub el 2026-11-17 con su etiqueta y su release), junto con un cuarto que salió al
medir: HR1 es NaN cuando su SQI es 0. Pasan la comprobación de versión, la
compilación en gnu++11 y los tests (12 grupos, entre ellos el nuevo
`test_data_contract` y los cuatro nuevos de HR1).

Librería, en una sola versión:

1. **Texto**: escribir las cuatro reglas de 5.1 en la cabecera y en §2.1 de la
   especificación, y poner §2.1 al día. Borrador en el anexo.
2. **Test del arranque** (regla 1): comprobar que ninguna muestra sale con 0
   en lugar de NaN, y corregir si alguna sale.
3. **Caducidad de HR1** (regla 4): test y corrección. Propuesta: si desde el
   último latido pasa más que el intervalo más largo admitido (2 s, el de
   30 lpm), HR1 pasa a NaN. No añade ningún parámetro nuevo.

IncuNest:

1. **Copia entera** de la muestra para todos los consumidores, con todo en
   «sin valor» si el AFE se para (5.3).
2. **Un solo punto de conversión** a `CTRL,VIT` y otro a telemetría: finito y
   en rango, o «sin valor». Incluye el PI como 0.00 y actualizar
   `PROTOCOL.md`.
3. **Telemetría de HR1, HR2 y HR3** por la regla 1, no por el SQI.
4. **Subir la librería** y anotar el cambio de definición del PI.

Estado de IncuNest a 2026-11-17: los cuatro puntos están hechos en la rama
`fix/pulse-oximetry-data-contract` (desde `dev`), **sin commit**, como
motherBoard 18.58.0. El HMI no cambia.

- La librería sube a la v0.123. La V18 compila.
- La global `g_spo2_data` desaparece. Los consumidores piden una copia entera
  con `spo2Snapshot()`. Si el chip se para, la copia no lleva medida: las
  cinco magnitudes a NaN y la sonda como desconectada.
- Módulo nuevo `modules/util/vitals_out`, con tests nativos: es el único
  sitio donde un valor de la librería se convierte en un campo de `CTRL,VIT`
  (0 = sin valor, también el PI) o se decide si va a telemetría.
- La telemetría del pulsioxímetro pasa a una sola función para wifi y GPRS
  (`spo2AddTelemetry()`). Decide por «es finito», sin mirar el SQI ni el
  estado de la sonda.
- Verificado: compila `IncuNest_V18` y pasan los 828 tests nativos (820 más
  8 nuevos). **Sin probar en el banco.**

### Entre las dos fases: PI por peine espectral

Alex quiere cuanto antes un método nuevo de PI por peine espectral (medir la
amplitud del pulso en la frecuencia cardiaca y sus armónicos), que además
daría un `pi_sqi`. Orden propuesto: después de cerrar la fase 1 y antes de
diseñar la fase 2.

- Después de la fase 1, para comparar contra una versión cerrada (v0.123) y
  no mezclar dos cambios en la misma medida.
- Antes de la fase 2, porque `pi_disp` y el `spo2_sqi` (hoy una rampa del PI)
  dependen de cómo quede el PI: así no se diseñan dos veces.
- Como los estudios de HR1, HR2 y HR3: primero en el banco, sobre las
  capturas y con el simulador (PI conocido), con la regla de decisión fijada
  antes de medir; a la librería solo lo que gane.
- Por decidir al empezar: de dónde sale la frecuencia del peine (HR3 o el
  valor fusionado) y en qué sesión se hace el estudio de banco, que vive en
  PulseNest.

### Fase 2: capa de pantalla (diseño propio)

1. Diseñar el contrato de `spo2_disp`, `hr_disp` y `pi_disp` (5.2) con
   medidas sobre las capturas.
2. Librería: implementarla empezando por `hr_disp` con la regla actual de
   CommTask (apartado 7).
3. IncuNest: mostrar y enviar los valores de pantalla, y retirar la fusión de
   CommTask.

## 9. Decisiones pendientes de Alex

Decidido el 2026-11-16: se hace primero la fase 1 (magnitudes clínicas).

- ¿Se escribe en la librería el texto del anexo tal como está?
- ¿Se adopta la capa de valores de pantalla en la librería (5.2)? Si sí, la
  tarea de valores no fiables pasa a ser sobre todo de la librería.
- ¿Se añade a la librería la lectura de estado de 5.6?
- ¿Se mide la hipótesis del `spo2_sqi` con acuerdo HR2–HR3 (apartado 6), o se
  deja para el análisis conjunto de los SQI que la librería tiene pendiente?
- ¿IncuNest sube a la v0.122 antes, o directamente a la versión nueva?

## Anexo: texto propuesto para la cabecera de la librería

Borrador de las cuatro reglas de 5.1, en inglés como el resto del código de la
librería. Va junto a `AFE4490Data` y, con la misma redacción, en §2.1 de la
especificación. Escrito en la librería el 2026-11-17 (cabecera, §2.1.1 de la
especificación y §25 de la justificación), sin commit todavía. El texto final
dice `pi >= 0` y marca la caducidad de HR1 como hueco conocido.

```cpp
// ── Data contract of the clinical output quantities: spo2, pi, hr1, hr2, hr3 ──
// Not covered here: the six raw ADC signals (integers, a new set every PRP,
// always a number) and ppg_disp.
// 1. A quantity is either a finite number or NaN. NaN means "no value". Nothing
//    else does: never 0, not at start-up either, and no other field needs
//    checking (not the SQI, not probe_state). isfinite(x) is the whole test.
// 2. A published quantity is always inside its declared range (getConfig()).
// 3. Every quantity x comes with a quality score x_sqi in [0, 1]. The score has
//    a meaning only while x is finite: 1 = the cleanest signal, 0 = the poorest
//    signal the library still publishes x from. While x is NaN, ignore x_sqi.
//    Example: spo2 = 97 with spo2_sqi = 0 is a valid SpO2 from a weak signal.
// 4. Each algorithm recomputes its quantity at its own cadence, over the last
//    seconds of signal (its window). Every recomputation writes a new result or
//    NaN: the library never keeps an older result in place of one that failed.
```
