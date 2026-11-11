# Plan — ajustar el paso banda de `ppg_disp` al diezmado a 25 Hz de la pantalla

Fecha: 2026-11-09 · Origen: sesión IncuNest "Pulsioximetría en HMI" (tarea 1) · Estado: **plan, nada implementado**

Pedido por Alex: plan para ajustar o corregir el filtro paso banda de `ppg_disp` en función del diezmado a 25 Hz que se le aplica antes de dibujarlo, con el análisis de cómo se eligió el 25 Hz.

## 1. Cómo se eligió el 25 Hz (rastreo del 2026-11-05, confirmado hoy)

- **No se eligió: se escribió.** Nace en `d0260b90` (Pablo, 2026-04-17, "PPG keeps working when pop up displayed"), primer `CTRL,PPG`, con el comentario literal "25 Hz = every 40 ms" y sin justificación en el commit, sin PR ni discusión escrita. Mismo día `17ab4525` "Pulsioximeter v_0".
- Ningún `.md` del repo (PROTOCOL, planes, openspec) lo justifica; todos lo citan como hecho.
- El chart nació con 256 puntos en modo SHIFT (10,2 s). `f098e634` (Pablo, 2026-08-13) lo bajó a 128 puntos y modo CIRCULAR porque "los latidos salían pegados": **eligió los puntos por tiempo (5,1 s, como un monitor clínico), no por píxeles**, y dejó el 25 Hz como dato de partida, no como decisión revisada (`ElementsCreation.cpp:4393`).
- Precedente plausible: el plotter de PulseNest limitaba a 25 Hz desde el 2026-04-08 (1 de cada 2 ticks del flujo a 50 Hz, por coste de render en Qt), nueve días antes. Coincidencia verosímil, no demostrable.
- La librería lo revisó el 2026-11-06 (rationale §21.7) y lo dejó **abierto**: "la tasa de transporte debería ser una constante del contrato librería/motherBoard/HMI, elegida para la pantalla mayor que esperemos; candidatas 25 Hz (hoy), 50 Hz (`PpgSnapshot` ya), 62,5 Hz (Masimo)". A 25 Hz y 25 mm/s el frente sistólico (100–150 ms) son 3 muestras: poligonal, y la cresta puede caer entre dos muestras.
- Geometría de hoy (§21.7, medida de Alex con regla el 2026-11-06): chart 460 × 105 px ≈ 88 × 20 mm; 128 puntos a 25 Hz = 5,12 s ≈ **17,3 mm/s** de barrido. La convención clínica del pletismograma es 25 mm/s (heredada del ECG; la norma no fija ninguna): a esa velocidad el chart cabría 3,5 s = 88 puntos a 25 Hz o 176 a 50 Hz.

**Conclusión:** 25 Hz es un número redondo de diseño inicial, sin medida ni análisis de Nyquist frente al paso banda de `ppg_disp`. No está mal por sí mismo; lo que falta es que el filtro y el diezmado se hayan diseñado juntos.

## 2. La cadena tal como está (lib v0.118, motherBoard 18.57.0 sin commitear)

| Etapa | Dónde | Qué hace | Tasa |
|---|---|---|---|
| Paso banda de display | librería, `_ppgdisp_bpf` (`setPPGDispFilter`, 0,5–20 Hz por defecto, spec §2.2) | **Un solo biquad**: Butterworth paso banda de 2.º orden = **6 dB/octava por lado**. Reiniciado sin sonda y precargado al aplicar (v0.118). El 0,5–20 es "un valor heredado sin rationale escrito" (§21.7). | 500 Hz |
| Seguidor SLEW + A(PI) | librería (`NORM_PI`) | Centra y escala a [-1, +1]; no filtra. | 500 Hz |
| Copia a la motherBoard | `SPO2.cpp:62-64`: `getData()` + `memcpy` a `g_spo2_data` por muestra; `ppgSnapshotFeed()` | Guarda la **última** muestra. | 500 Hz |
| Envío al HMI | `CommTask.cpp:1584-1607`: cada 40 ms lee `g_spo2_data.ppg_disp` y manda `128 + ppg_disp × 127` | **Diezmado por "coge la última muestra"** (sample-and-hold), sin promediar ni filtrar. El tick sale de `millis()` en un bucle de 1 ms: jitter de ~1–2 ms sobre un periodo de 40 ms. | 25 Hz |
| Snapshot ThingsBoard | `PpgSnapshot.cpp:37`: 1 de cada 10 muestras | Mismo diezmado sin filtro, a 50 Hz. | 50 Hz |
| HMI | `UITask.cpp:5319`, `ui_LockPPGChart` 128 puntos, rango 0–255 fijo | Une los puntos con segmentos; sin filtrado. | 25 Hz |

Nyquist del transporte: **12,5 Hz**. El filtro deja pasar hasta 20 Hz a −3 dB.

## 3. Cuánto aliasing hay (cálculo con el prototipo analógico del biquad; el bilineal a 500 Hz solo cambia las cifras por encima de ~100 Hz)

Ganancia del paso banda 0,5–20 Hz de 2.º orden y dónde reaparece cada frecuencia (aliasing) al muestrear a 25 Hz:

| f de entrada | ganancia del BPF | aparece en la traza como | comentario |
|---|---|---|---|
| 5 Hz | −0,1 dB | 5 Hz | banda útil (HR 240 lpm = 4 Hz fundamental) |
| 10 Hz | −0,5 dB | 10 Hz | 2.º armónico a 240 lpm; muesca dicrótica |
| 12,5 Hz | −1,3 dB | 12,5 Hz (Nyquist) | límite |
| 15 Hz | −2,0 dB | 10 Hz | **aliasing casi sin atenuar** |
| 20 Hz | −3,0 dB | 5 Hz | aliasing, en plena banda del pulso |
| 25 Hz | −4,1 dB | 0 Hz | deriva de línea base |
| 37,5 Hz | −6,7 dB | 12,5 Hz | |
| **50 Hz** | −8,8 dB | **0 Hz** | red eléctrica; con el jitter del tick deja de ser continua y se convierte en ruido |
| **100 Hz** | −14,3 dB | **0 Hz** | parpadeo de iluminación / PWM de fototerapia (la propia spec, §7.x, lo señala como "riesgo residual real, no medido") |

Lectura: todo lo que hay entre 12,5 y ~40 Hz entra en la traza casi entero; la red y sus armónicos lo hacen atenuados solo 9–14 dB. En un PPG limpio del simulador esto es invisible (casi no hay energía por encima de 10 Hz); con un dedo real, movimiento, o fototerapia encendida, sale como "pelusa" y pequeñas ondulaciones que no están en la señal. **No está medido**: la tabla es teoría.

Para comparar, lo que haría un filtro de display más bajo y más empinado (4.º orden Butterworth, como `LowPass4` que la librería ya usa para PI/HR3; cifras del paso bajo, sin contar el paso alto):

| corte | 12,5 Hz | 15 Hz | 20 Hz | 25 Hz | 50 Hz |
|---|---|---|---|---|---|
| 8 Hz | −15,6 dB | −21,9 dB | −31,8 dB | −39,6 dB | −63,7 dB |
| 10 Hz | −8,4 dB | −14,2 dB | −24,1 dB | −31,8 dB | −55,9 dB |
| 15 Hz (el de HR3) | −0,9 dB | −3,0 dB | −10,5 dB | −18,0 dB | −41,8 dB |

(la fila de 15 Hz coincide con la tabla de la spec §5.3.1: por eso HR3 no puede bajar de 50 Hz; para el display, con 25 Hz, el corte tiene que bajar a 8–10 Hz).

Y lo que hace un **promedio de bloque de 40 ms** (20 muestras) en la motherBoard, solo él:

| f | 5 Hz | 10 Hz | 12,5 Hz | 20 Hz | 25 Hz | 37,5 Hz | 50 Hz | 100 Hz |
|---|---|---|---|---|---|---|---|---|
| sinc | −0,6 dB | −2,4 dB | −3,9 dB | −12,6 dB | **nulo** | −13,5 dB | **nulo** | **nulo** |

El promedio de bloque tiene **ceros exactos en los múltiplos de 25 Hz**: justo las frecuencias (25, 50, 100 Hz) que el sample-and-hold hace caer por aliasing sobre 0 Hz. Es barato (una suma por muestra) y complementa al filtro de la librería; solo, no basta (20 Hz → 5 Hz queda a −15,6 dB sumando el BPF actual).

## 4. Reparto de responsabilidades (criterio de la librería, §21.7, que comparto)

- **Librería**: el ancho de banda de `ppg_disp`. Contrato a escribir: "`ppg_disp` está limitado en banda para que cualquier consumidor que lo muestree a ≥ 25 Hz esté a salvo sin saber nada más". Es donde está el conocimiento de la señal y donde se mide (PPGDISPLAB).
- **Consumidor (motherBoard)**: diezmar bien, no "coger la última": promedio de bloque. Es transporte, cambio mínimo, y vale para los dos consumidores (`CommTask` a 25 Hz y `PpgSnapshot` a 50 Hz).
- **Tasa de transporte y barrido (HMI/PROTOCOL)**: decisión aparte (§21.7 punto 2). Con los dos puntos anteriores hechos, 25 Hz es seguro; subir a 50 Hz sería por calidad visual (frente sistólico con 6 muestras en vez de 3) y por el barrido a 25 mm/s, no por aliasing.

## 5. Pasos

### Paso 0 — Medir antes de tocar (PPGDISPLAB, PulseNest; sesión de la librería) — **rebajado a opcional el 2026-11-10, ver §8.1**

Objetivos (en este orden):
1. Saber **si hay** energía por encima de 12,5 Hz en `ppg_disp` real y **cuánta**, y de dónde viene (señal, ruido del AFE, red/fototerapia, movimiento). Todo lo de la sección 3 es teoría; si no hay nada, el problema es de papel.
2. Fijar el **corte** del paso bajo con datos: el más alto que quite el aliasing visible, para no comerse la muesca dicrótica ni redondear el frente sistólico (8 o 10 Hz).
3. Comprobar que el paso bajo **no deforma** el latido de forma visible frente a la señal a 500 Hz (retardo de grupo, sobreimpulso).
4. De paso, con la misma captura: OT, RF e ILED frente al tiempo en los primeros 6 s tras aplicar, para el transitorio residual y el arranque alto del PI (sección 7).

Captura de `ppg_disp` en `NO_NORM` a 500 Hz (la salida del BPF actual) con: simulador MS100 a 135 lpm; dedo real de adulto quieto; dedo con perfusión baja (Raynaud, agua fría); y, si se puede, con LEDs de fototerapia encendidos cerca de la sonda y dentro de la incubadora con calefactor en marcha.

Offline (Python, el banco de PPGDISPLAB ya tiene la infraestructura):
1. Espectro de cada captura: fracción de potencia por encima de 12,5 Hz respecto a la banda 0,5–12,5 Hz.
2. Simular las cuatro cadenas y comparar la traza resultante a 25 Hz contra la de 500 Hz (error RMS y a ojo): (a) sample-and-hold actual; (b) promedio de bloque de 40 ms; (c) LP4 a 8 Hz y a 10 Hz tras el BPF + sample-and-hold; (d) LP4 + promedio de bloque.
3. Tabla de resultados. Criterio: elegir la combinación más simple en la que el aliasing quede por debajo de lo visible (p. ej. < 1 % de la amplitud del latido) en la peor captura.

Entregable: una nota en PulseNest y una línea en el rationale §21.7. Si en las capturas no hay nada por encima de 12,5 Hz ni con fototerapia, el paso 1 se puede reducir a bajar el corte y documentarlo.

### Paso 1 — Librería: filtro de display más bajo y más empinado (brief, no código desde IncuNest)

- Propuesta: **un solo paso banda con pendientes asimétricas**: lado alto de 1.er orden a 0,5 Hz (el de hoy) y lado bajo de 4.º orden a 8–10 Hz. En digital eso es una cascada de secciones de 2.º orden (biquads) de todos modos; la forma natural de escribirlo con lo que hay en la librería es el biquad paso banda actual seguido de `LowPass4`. No es "dos filtros", es un paso banda de orden distinto en cada lado. Por qué no un Butterworth paso banda simétrico de orden alto: el lado alto a 0,5 Hz **no debe** empinarse: un paso alto de 4.º orden a 0,5 Hz tiene τ y sobreimpulso largos (segundos) ante cualquier escalón o deriva de OT, justo lo que acabamos de pelear en v0.118; el de 1.er orden es el más benigno. El lado bajo sí tiene que ser empinado para que Nyquist (12,5 Hz) quede bien dentro de la banda de rechazo. Mantener `setPPGDispFilter(f_low, f_high)` como API, con `f_high` como corte del lado bajo. Elegir el corte con los datos del paso 0 (10 Hz conserva el 2.º armónico a 240 lpm y la muesca dicrótica; 8 Hz da 7 dB más de margen).
- Debe entrar en el mismo `reset()`/precarga del v0.118 (cada sección de `LowPass4` precarga sola).
- El SLEW se beneficia: menos factor de cresta que absorber (§21.7 punto 1).
- Retardo de grupo añadido (~40–60 ms a 10 Hz): irrelevante, nada algorítmico lee `ppg_disp` (spec §1.3).
- Tests en `test_ppgdisp`: un tono a 15 Hz atenuado ≥ N dB; el escalón de aplicación del test 10 sigue precargado.
- Changelog con el contrato de banda limitada y la versión; IncuNest sube el pin y recompila (gnu++11).

### Paso 2 — motherBoard: promedio de bloque en vez de "última muestra" (IncuNest; **solo provisional o si el paso 1 no se hace**)

Qué aporta y qué no (revisado 2026-11-09 tras las preguntas de Alex): si la librería limita la banda bien (paso 1), el promedio de bloque **es redundante**: el LP4 a 10 Hz ya deja 50 Hz a −56 dB y 25 Hz a −32 dB, y el promedio solo añade sus ceros en 25/50/100 Hz, que ya no hacen falta. Y tiene un coste: en banda cae −2,4 dB a 10 Hz y −0,6 dB a 5 Hz (redondea un poco el pico sistólico). Su valor real es otro: se puede hacer hoy en IncuNest sin tocar la librería y protege de la red y la fototerapia mientras tanto, o si alguien vuelve a subir `f_high`. Con el paso 1 hecho, no implementarlo.

- En `SPO2_Task` (`SPO2.cpp:62-64`), junto a `ppgSnapshotFeed()`, acumular `ppg_disp` por muestra en un acumulador pequeño (suma + cuenta) y publicar el promedio cada 20 muestras (25 Hz) en un campo propio de la motherBoard (no en `g_spo2_data`, que es la copia de la librería). `CommTask` lee ese promedio en su tick de 40 ms. Coste: una suma en `float` por muestra a 500 Hz, nada de heap, nada en el camino del calefactor.
- `PpgSnapshot`: promedio de 10 en vez de 1 de cada 10 (misma idea, 50 Hz).
- `PROTOCOL.md` (CTRL,PPG): "promedio de las 20 muestras del intervalo" en vez de "muestra". HMI sin cambios.
- Comentarios en inglés; `pio run -e IncuNest_V18`; si el acumulador se saca a una función de host, test en `pio test -e native`.
- "Sobre la línea base" quiere decir: al muestrear a 25 Hz, una componente a exactamente 25, 50 o 100 Hz se muestrea siempre en la misma fase y sale como un **desplazamiento constante** de la traza; a 50,2 Hz sale como una ondulación de 0,2 Hz (deriva de línea base). Un promedio de exactamente 40 ms integra un número entero de periodos de esas frecuencias y da cero: por eso tiene nulos ahí.

### Paso 3 — 50 Hz y 25 mm/s — **objetivo deseable desde el 2026-11-10 (Alex), análisis en §8.3 y §8.4**

- Geometría del plot (`ElementsCreation.cpp:4386-4427`): objeto de **460 × 105 px**, padding 0 y borde 0, así que el área de dibujo es la misma; rango Y 0–255 sobre 105 px (2,4 valores por px); línea de 2 px, marcadores de punto de tamaño 0. Pantalla 800 × 480 px ≈ 154 mm → ≈ 5,2 px/mm; el chart ≈ 88 × 20 mm. LVGL reparte los N puntos uniformemente en los 460 px (x = i · 459 / (N − 1)) y une consecutivos con segmentos rectos: caben hasta 460 puntos (1 px por punto) sin perder nada; hoy 128 puntos = 3,6 px por punto; a 50 Hz serían 256 puntos (5,1 s) = 1,8 px por punto, o 176 (3,5 s a 25 mm/s) = 2,6 px. Caben.
- Subir `CTRL,PPG` a 50 Hz (UART 115200: 13 B × 50 = 650 B/s, despreciable) y el chart a 176 puntos (3,5 s a 25 mm/s) o 256 (5,1 s). Hay que medir el coste de render en el CrowPanel (Pablo tuvo que pasar a CIRCULAR por el coste a 25 Hz) y cambiar `PROTOCOL.md`, `CommTask`, `PpgSnapshot` (ya está a 50 Hz) y el HMI.
- Solo si tras 1+2 la traza sigue viéndose poligonal o Alex quiere el barrido clínico. No lo exige el aliasing.

### Paso 4 — Verificación en banco (V18)

- Simulador a 135 lpm y dedo real: traza sin "pelusa"; latido con la misma forma que en PPGDISPLAB a 500 Hz.
- Con fototerapia encendida y sonda aplicada: antes/después del paso 2 (foto de pantalla).
- Snapshot de ThingsBoard coherente con la traza (misma escala y filtrado).

## 6. Orden propuesto y dependencias

1. Paso 0 (medir) → paso 1 (brief a la librería, como el del arranque lento) → subir el pin → paso 4.
2. Paso 2 solo si el paso 1 se retrasa o no se hace.
3. Paso 3 solo si se decide (calidad visual / barrido clínico). → Superado por §8.4.

## 8. Revisión 2026-11-10 (comentarios de Alex)

### 8.1 El paso 0 deja de ser requisito

Alex no ve su utilidad y ofrece colocar el simulador en la tarjeta que haga falta. Revisado: tiene razón en lo esencial.

- Lo que el paso 0 decidía (el corte) **se puede fijar por diseño**: HR neonatal 100–240 lpm = fundamental 1,7–4 Hz; **los bebés no muestran muesca dicrótica visible** (observación de Alex), así que no hay que conservar armónicos altos por morfología; con 2 armónicos la forma del latido ya es la que se ve en los monitores. Corte del paso bajo: **8 Hz, 4.º orden** (conserva el 2.º armónico hasta 240 lpm; precedente Masimo 7,5 Hz, §21.7). 10 Hz solo si en banco el frente sistólico se ve redondeado, cosa improbable (tiempo de subida del filtro ≈ 50 ms frente a 100–150 ms del frente).
- Lo que el paso 0 *confirmaba* (si hay aliasing visible hoy) no cambia la decisión: el filtro hay que bajarlo de todos modos porque 0,5–20 Hz con Nyquist a 12,5 Hz es incorrecto de diseño, haya o no energía ahí hoy. Y la captura que lo demostraría (dedo real, movimiento, fototerapia encendida) no la da el simulador, que es una señal limpia.
- Desde IncuNest no se puede capturar `ppg_disp` a 500 Hz (sin puerto serie en banco, sin ThingsBoard provisionado); sería un endpoint de diagnóstico temporal con 8–16 kB de RAM en una placa con el heap justo. No compensa.
- Queda: **verificación visual en banco (paso 4)** cuando la librería publique el filtro, con el simulador que Alex ofrece (135 lpm y una amplitud baja) y un dedo, antes/después. Si algo se ve raro, entonces sí se mide.

### 8.2 Barrido a 25 mm/s como objetivo deseable

- Precisión sobre "como dice la norma": ISO 80601-2-61 **no fija** la velocidad de barrido del pletismograma (§21.7, revisado el 2026-11-06). 25 mm/s es la convención clínica heredada del ECG (IEC 60601-2-27), con 12,5 y 50 mm/s como opciones habituales. Es un objetivo de usabilidad, no de conformidad, y bueno.
- En el HMI es solo el número de puntos: `N = tasa × ancho_px / (barrido_mm_s × px_mm)`. Con 460 px y 5,2 px/mm: 25 mm/s = 130 px/s → **3,54 s de ventana** → 88 puntos a 25 Hz o **177 (→ 176) a 50 Hz**. Conviene escribirlo así en el código (constantes `PANEL_PX_PER_MM`, `PPG_SWEEP_MM_S`, `PPG_RATE_HZ`) y no como 128 a pelo.
- 12,5 mm/s: 7,1 s de ventana, 176 puntos a 25 Hz o 352 a 50 Hz; cabe. Pero requiere un ajuste de usuario (pantalla de ajustes + NVS) y nadie lo ha pedido: dejar la fórmula preparada y no exponerlo. 50 mm/s cabría igual (1,77 s), misma objeción.
- A 25 mm/s un latido a 160 lpm ocupa 9,4 mm ≈ 49 px; 3,5 s son 9 latidos. Razonable en 88 mm.
- El hueco de borrado (`LOCK_PPG_GAP` = 6 puntos = 0,24 s) debería ir también en segundos: 12 puntos a 50 Hz.

### 8.3 Análisis: cambiar el transporte de 25 Hz a 50 Hz

**Qué cambia físicamente.** Nyquist pasa de 12,5 a 25 Hz; el frente sistólico (100–150 ms) pasa de 2–4 muestras a 5–7; a 25 mm/s cada muestra ocupa 2,6 px en vez de 5,2 (con línea de 2 px, a 5,2 px/muestra el polígono se ve; a 2,6 no).

| | 25 Hz (hoy) | 50 Hz |
|---|---|---|
| Ventana a 25 mm/s | 3,54 s / 88 puntos / 5,2 px por punto | 3,54 s / 176 puntos / 2,6 px por punto |
| Muestras en el frente sistólico | 2–4 | 5–7 |
| Error de cresta (pico entre dos muestras) | unos pocos % (§21.7) | la cuarta parte |
| Nyquist | 12,5 Hz | 25 Hz |
| LP4 a 8 Hz en Nyquist | −15,6 dB | −39,6 dB |
| Red (50 Hz) y parpadeo (100 Hz) | caen en 0 Hz; con LP4 8 Hz a −64 dB | igual (50 y 100 Hz siguen siendo múltiplos) |
| UART MB→HMI (115200 = 11,5 kB/s) | 13 B × 25 = 325 B/s (2,8 %) | 650 B/s (5,6 %) |
| Parseo en el HMI (`sscanf`, bucle de 10 ms) | 25 líneas/s | 50 líneas/s; 6,5 B por vuelta, FIFO UART 128 B: sin riesgo |
| Coherencia con `PpgSnapshot` | distinta (50 Hz) | la misma tasa |
| Jitter del tick de `millis()` (±1 ms) | 2,5 % del periodo | 5 %; 0,13 px en x: invisible |
| RAM del chart | 128 × 2 B | 176–256 × 2 B: despreciable |

**Ventajas.** Traza visiblemente más suave al barrido clínico; cresta más fiel; el doble de margen para el filtro (y tolerancia a que algún día alguien suba `f_high`); misma tasa en los dos consumidores (HMI y snapshot) y que las cadenas HR2/HR3 de la librería (50 Hz es el MCD de la rejilla de PRF, spec §5.3.1: factor entero en todas las PRF validadas); sirve también para una pantalla mayor o para 50 mm/s (ahí 50 Hz daría los 5,2 px/muestra de hoy).

**Inconvenientes y costes.**
- Dos firmwares que cambiar y flashear juntos (motherBoard y HMI) y `PROTOCOL.md`. No hay negociación de tasa en el protocolo (solo el `HMI,UI_READY`): una pareja desparejada no rompe nada pero muestra la ventana mal (MB nueva + HMI viejo: 128 puntos = 2,6 s; MB vieja + HMI nuevo: 176 puntos = 7 s). Aceptable si se flashean en la misma sesión de banco; si se quiere blindar, un campo de tasa en `CTRL,PPG` o un `CTRL,PPG_RATE` al aplicar la sonda, pero es más protocolo por un caso transitorio.
- Coste de render en el CrowPanel: en CIRCULAR cada muestra invalida solo las tiras alrededor del cursor, y LVGL refresca cada 10 ms (`LV_DISP_DEF_REFR_PERIOD`): a 50 Hz entra una muestra cada 2 refrescos, así que el área invalidada **por refresco** apenas cambia (una tira de 2,6 px + hueco en vez de 3,6 px). El coste total sube poco; lo que sí hay que mirar en banco es la `ui_hwm`/`comm_hwm` del log DIAG y que no reaparezca el parpadeo que Pablo arregló en `f098e634`.
- Motherboard: 40 → 20 ms en `CommTask` y el comentario; nada más.

**Riesgos.**
1. **Pérdida de muestras en el HMI (el riesgo real, y ya existe a 25 Hz).** `ctrl_ppg_msg` es un buzón de **una sola plaza** (`ppg` + `updated`): si `lv_timer_handler()` tarda más que un periodo, la muestra siguiente pisa a la anterior. Los propios comentarios de `UITask.cpp` documentan repintados de más de 40 ms con una alarma en pantalla; a 20 ms de periodo ocurrirá el doble de veces. Corrección: una cola pequeña (FIFO de 32 bytes, escritor `CommTask`, lector `UITask`, sin heap) y que `UITask` vacíe la cola en cada vuelta. Es una mejora también para 25 Hz, y es la parte "cara" del cambio (~30 líneas en el HMI).
2. **Aliasing si se sube la tasa antes que el filtro:** a 50 Hz con el 0,5–20 Hz actual, 30 Hz cae en 20 Hz a −5 dB y la red en 0 Hz a −8,8 dB: mejor que hoy, pero sigue mal. Orden: primero el filtro en la librería, o ambos a la vez.
3. Versiones desparejadas MB/HMI (arriba).
4. El eje x del chart supone muestras equiespaciadas; el jitter es despreciable, pero si la cola del riesgo 1 acumula varias muestras y `UITask` las vuelca de golpe, el chart las dibuja igual de espaciadas: correcto en forma, con un retardo de pantalla de hasta un refresco. Sin consecuencias.

**Alternativas consideradas.** Quedarse en 25 Hz y poner 88 puntos (25 mm/s a coste cero: poligonal a 5,2 px/muestra, el frente con 3 muestras). Interpolar en el HMI (cosmético, no añade información; no). 62,5 Hz como Masimo (16 ms; sin ventaja sobre 50 y pierde la coherencia con el snapshot y las cadenas HR).

**Recomendación.** Hacer el cambio a **50 Hz junto con los 25 mm/s (176 puntos) y la FIFO del HMI**, en un commit de la rama `feat/hmi-pulse-oximetry` (toca `CommTask.cpp` de la MB, `PROTOCOL.md`, y en el HMI `ElementsCreation.h/.cpp`, `CommTask.cpp/.h`, `UITask.cpp`), **después o a la vez que** el filtro de la librería (8 Hz, 4.º orden). Coste estimado: una mañana de código más una sesión de banco con el simulador para flashear los dos firmwares y mirar HWM y parpadeo. El promedio de bloque (paso 2) queda descartado.

### 8.5 Corrección con datos (2026-11-10): el corte no es 8 Hz, y depende de la tasa de transporte

Alex: "no veo la muesca dicrótica, pero el espectro tiene armónicos más allá de los 14 Hz" (espectrogramas `Spectra/` de SUBJ08–11; la patente de Masimo del 97 con 7,5 Hz no es una justificación porque no sabemos sus razones). Comprobado con FFT de las capturas HOSPNAV (solo lectura, `OT_LED1` a 500 Hz, ventanas de 10,24 s con sonda aplicada, descartando artefactos; referencia = la salida del paso banda actual 0,5–20 Hz; script temporal `ppg_harmonics.py`, figura `subj08_display_filter_candidates.png`):

| | SUBJ08 | SUBJ09 | SUBJ10 | SUBJ11 |
|---|---|---|---|---|
| HR mediana | 164 lpm | 170 | 158 | 164 |
| ventanas limpias | 83 | 39 | 17 | 54 |
| h2 / fundamental | 27 % | 26 % | 28 % | 32 % |
| h3 | 9,8 % | 8,3 % | 7,8 % | 11,8 % |
| h4 (~11 Hz) | 5,0 % | 3,8 % | 4,6 % | 6,5 % |
| h5 (~13,5 Hz) | 2,3 % | 1,4 % | 1,9 % | 3,3 % |
| h6 (~16 Hz) | 1,0 % | 0,7 % | 0,8 % | 1,5 % |
| potencia > 8 / 10 / 12,5 Hz | 1,2 / 0,34 / 0,07 % | 0,87 / 0,20 / 0,03 % | 0,38 / 0,17 / 0,05 % | 1,6 / 0,56 / 0,15 % |

Coste visible de añadir un paso bajo de 4.º orden (retardo de grupo descontado; p-p = amplitud pico a pico del latido; mediana de ventanas, y p95 del máximo entre paréntesis):

| LP4 | diferencia RMS | diferencia máxima (en el pico sistólico) | cambio de p-p | retardo |
|---|---|---|---|---|
| 8 Hz | 0,8–2,2 % de p-p | 4,6–6,6 % (p95 7–9 %) | < 0,4 % | 54 ms |
| 10 Hz | 0,6–1,6 % | 3,9–5,3 % (p95 6–7 %) | < 0,7 % | 42 ms |
| 12 Hz | 0,5–1,0 % | 2,8–4,1 % (p95 5–6 %) | < 0,8 % | 36 ms |

En píxeles: el latido ocupa ~91 px de los 105 (margen del 15 %), así que 1 px ≈ 1,1 % de p-p. **8 Hz redondea el pico sistólico 5–6 px; 12 Hz, unos 3 px.** La diferencia RMS (la forma general) está en 1–2 px en todos los casos. SUBJ10 tiene p95 altos por artefactos de movimiento, no por el filtro.

Ganancias de los candidatos (filtro digital a 500 Hz):

| f | 5 | 8 | 10 | 12,5 | 15 | 20 | 25 | 50 Hz |
|---|---|---|---|---|---|---|---|---|
| BPF 0,5–20 actual | −0,1 | −0,5 | −0,8 | −1,3 | −1,9 | −3,0 | −4,2 | −9,0 dB |
| + LP4 8 Hz | −0,1 | −3,0 | −8,4 | −15,7 | −21,9 | −32,0 | −39,8 | −64,8 |
| + LP4 10 Hz | 0 | −0,7 | −3,0 | −8,4 | −14,3 | −24,2 | −32,1 | −57,0 |
| + LP4 12 Hz | 0 | −0,2 | −0,9 | −3,8 | −8,5 | −17,9 | −25,7 | −50,7 |

**Conclusión: el corte y la tasa de transporte son una sola decisión.**
- Con transporte a **25 Hz** (Nyquist 12,5) el corte tendría que ser ≤ 8 Hz, y aun así solo da −16 dB en Nyquist y cuesta 5–6 px en el pico. Mala pareja.
- Con transporte a **50 Hz** (Nyquist 25) el corte puede quedarse en **12 Hz**: −26 dB en Nyquist, −51 dB en la red, y el coste visible queda en el límite del píxel (3 px en el pico, 1 px RMS). 15 Hz (el de HR3) también sería defendible (−18 dB en Nyquist, −42 dB en la red; la potencia real por encima de 12,5 Hz es 0,03–0,15 %), pero 12 Hz da margen para interferencia sin perder nada visible.
- Recomendación revisada: **50 Hz + paso bajo de 4.º orden a 12 Hz**, el corte a decidir en la librería con este mismo análisis (el script lee solo capturas de PulseNest y se puede entregar a esa sesión). 8 Hz y el precedente de Masimo quedan descartados como justificación.

### 8.6 Criterios para elegir el corte, con datos (2026-11-10); refresco de LVGL; geometría del plot

**Ruido y señal con aliasing (medido en SUBJ08–11, script `ppg_alias_noise.py`).** RMS de lo que cae dentro de banda al muestrear, en % de la amplitud pico a pico del latido (mediana de ventanas; 1 px ≈ 1,1 %):

| filtro de display | transporte 25 Hz | transporte 50 Hz |
|---|---|---|
| BPF 0,5–20 Hz actual (2.º orden) | 0,22–0,96 % | 0,04–0,09 % |
| + LP4 20 Hz | 0,20–0,91 % | 0,01–0,02 % |
| + LP4 15 Hz | 0,15–0,72 % | ≤ 0,01 % |
| + LP4 12 Hz | 0,09–0,43 % | ≤ 0,01 % |

PI de los cuatro bebés (p50): 0,72 % (SUBJ08, perfusión baja), 3,7 %, 1,5 %, 2,6 %: el caso de PI bajo está dentro de la muestra y su ruido con aliasing a 50 Hz es 0,09 %. **A 50 Hz el aliasing de la propia señal y del ruido del AFE es despreciable con cualquier corte, incluso con el filtro actual.** Lo que queda como motivo para empinar el filtro es la interferencia determinista (red 50/100 Hz, drivers de fototerapia, calefactor), que no está en estas capturas de hospital y que la spec §7 señala como riesgo residual no medido.

Coste de forma (añadido a la tabla de §8.5): LP4 20 Hz: RMS 0,3–0,55 %, máx 1,7–2,4 % (~2 px), retardo 20 ms. LP4 15 Hz: RMS 0,3–0,6 %, máx 1,6–2,2 % (~2 px), retardo 28 ms. Es decir, **15 y 20 Hz cuestan lo mismo a la vista** (2 px en el pico); 12 Hz, 3 px; 8 Hz, 5–6 px.

Rechazo de la cadena completa (BPF + candidato), dB:

| | 20 Hz | 25 Hz (Nyquist a 50 Hz) | 50 Hz (red) | 100 Hz |
|---|---|---|---|---|
| BPF actual solo | −3,0 | −4,2 | −9,0 | −15,5 |
| + LP4 20 Hz | −6,0 | −12,7 | −41,8 | −76 |
| + LP4 15 Hz | −13,5 | −22,2 | −51,9 | −86 |
| + LP4 12 Hz | −20,9 | −29,9 | −59,7 | −94 |

**Criterios, por orden de peso:**
1. **Fidelidad de la forma** del latido frente a la señal a 500 Hz: ≤ 1 px RMS y ≤ 2–3 px en el pico sistólico. Lo cumplen 15 y 20 Hz (2 px) y 12 Hz (3 px); 8 Hz no.
2. **Aliasing de señal y ruido propios** para la tasa de transporte: medido, despreciable a 50 Hz con todo; ~1 px a 25 Hz con el filtro actual. Favorece 50 Hz más que cualquier corte.
3. **Rechazo de interferencia que caiga en banda**: la red (50/100 Hz) cae sobre la línea base a 50 Hz de transporte; hoy −9/−15 dB; con 4.º orden a 20 Hz, −42/−76 dB. Aquí lo que cuenta es el **orden**, no tanto el corte: pasar de 2.º a 4.º orden gana 33 dB en la red aun sin bajar el corte.
4. **Perfusión baja**: el normalizador escala la traza al latido, así que el ruido relativo crece como 1/PI; cubierto por SUBJ08 (PI 0,7 %). A PI 0,5 % seguiría < 0,2 %.
5. **Retardo de grupo** (20–54 ms) y **transitorios** del paso bajo (τ de 10–30 ms): irrelevantes frente al paso alto de 0,5 Hz, que es quien manda en la aplicación de la sonda.
6. **Coste de CPU**: dos biquads más por muestra a 500 Hz; despreciable.
7. **Contrato para consumidores**: la librería debe poder decir "banda limitada para muestrear a ≥ 50 Hz"; con LP4 a 20 Hz lo cumple con −12,7 dB en Nyquist (justo), con 15 Hz con −22 dB (holgado).
8. **Reutilización**: `LowPass4` a 15 Hz es exactamente el filtro de HR3, ya caracterizado en la spec §5.3.1; coste de implementación y de documentación mínimo.
9. Lo que **no** es criterio: la norma (no fija ancho de banda de la onda), Masimo 7,5 Hz (razones desconocidas), la muesca dicrótica (no visible en neonatos).

**Conclusión sobre el corte.** Con transporte a 50 Hz, los datos no obligan a bajar de 20 Hz; obligan a subir el **orden** por la interferencia. Las dos opciones razonables son 4.º orden a **20 Hz** (mantiene la banda de siempre; −12,7 dB en Nyquist, −42 dB en la red) o a **15 Hz** (igual coste visible, 10 dB más de margen en Nyquist y en la red, mismo filtro que HR3). La elección es de la librería; esta sesión no ve razón de peso para 12 Hz una vez el transporte está a 50 Hz. 8 Hz descartado. **Lo que sí es firme: 4.º orden y 50 Hz.**

**Refresco de LVGL (pregunta 2).** `LV_DISP_DEF_REFR_PERIOD` pasó de 30 (valor por defecto de LVGL) a 10 ms en `4902f619` (Pablo, 2026-05-19, "misc fixes", junto con el periodo de lectura del táctil 30 → 10 ms y los timings RGB del panel: pclk 15 → 18 MHz, polaridades). Sin motivo escrito; lo más probable es la respuesta táctil y las animaciones (el candado de desbloqueo se anima "en pasos de 1 %"). Qué significa y cómo elegirlo:
- No es el refresco físico del panel. El panel RGB barre su frame buffer a `pclk / ((800+8+4+8) × (480+8+4+8))` = 20 MHz / 410 000 ≈ **48,8 Hz, 20,5 ms por cuadro**. `LV_DISP_DEF_REFR_PERIOD` es cada cuánto `lv_timer_handler()` mira si hay zonas invalidadas y las repinta en el buffer parcial y las vuelca. Repintar más rápido que 20 ms no enseña más cuadros; como mucho dos repintados por cuadro, con riesgo de desgarro parcial y trabajo tirado.
- Además `UITask` llama a `lv_timer_handler()` en un bucle con `vTaskDelay(10 ms)`, así que el periodo efectivo es ≥ 10 ms más lo que dure el repintado: con 10 ms en LVGL, "repinta cada vez que te llamen".
- Criterios: (a) ≥ 1 repintado por cuadro del panel (20,5 ms) para que la traza y las animaciones avancen en cada cuadro; (b) latencia táctil < 100 ms (se cumple con 10 o 30 ms); (c) una muestra de PPG por repintado si se puede (a 50 Hz, 20 ms: coincide con el cuadro); (d) coste: con nada sucio el handler cuesta < 1 ms, así que el periodo corto no gasta CPU por sí mismo; lo que gasta es el área repintada, y un periodo corto parte un movimiento en más repintados pequeños con más volcados; (e) el valor por defecto de LVGL (30 ms = 33 Hz) viene de pantallas lentas, no de un análisis de este panel.
- Recomendación: **20 ms**, igual al cuadro del panel y al periodo de `CTRL,PPG` a 50 Hz; 10 ms no hace daño pero no aporta. Medir con `ui_hwm` y el log DIAG antes y después. No tocarlo en este trabajo salvo que el banco lo pida.

**Geometría (preguntas 3 y 4).** Datos de partida: 25 mm/s y 154 mm / 800 px = 5,19 px/mm → 130 px/s. El ancho del chart es variable. Lo que hay dentro del marco inferior (y 362..479, 117 px de alto, x 4..796): chart 460 × 105 px en x 10..470; hueco de 10 px; panel de visores `ui_LockVitCont` 310 × 105 px en x 480..790, tres columnas de 92 / 92 / 108 px (SpO2, HR, PI), cifras en Montserrat 48 (≈ 9 mm de alto) con título en Montserrat 16. Ancho útil total 780 px.

| ventana | mm | px de chart | px que quedan para los visores | cabe |
|---|---|---|---|---|
| 3,54 s (hoy) | 88 | 460 | 310 | sí, tal cual |
| 4,0 s | 100 | 519 | 251 | solo bajando las cifras a ~Montserrat 40 (tres columnas de ~83 px; "12.5" del PI es lo que aprieta) |
| 4,4 s | 110 | 570 | 200 | sí, si el panel pasa a **dos filas**: SpO2 y HR a 48 arriba (2 × 92 px), PI a 24–28 debajo con el título en línea (48 + 16 + 30 ≈ 94 de 105 px) |
| 5,0 s | 125 | 649 | 121 | **no** con tres visores legibles: ni en fila (hacen falta ≥ 250 px) ni apilados (3 × 48 px > 105 px); solo con cifras ≤ 28 px, un retroceso en legibilidad |

Alternativas si se quieren 5 s: dar más altura al marco (hoy 117 px de 480: redistribuir la pantalla de bloqueo, trabajo mayor) o sacar el PI del panel (ya no sería un visor sino un rótulo pequeño junto al título). Recomendación: fijar 25 mm/s y dejar que la ventana sea la que quepa: 3,5 s hoy sin tocar nada (9 latidos a 160 lpm) o **4,4 s con el panel a dos filas** si se quiere ganar traza sin perder cifra. Un monitor clínico enseña 4–6 s porque tiene 25–30 cm de ancho; aquí la cifra grande vale más que el medio segundo extra.

### 8.4 Orden revisado

1. Brief a la librería (forma acordada el 2026-11-10, §8.7): lado alto del paso banda de `ppg_disp` a **4.º orden**, corte **configurable** por el `setPPGDispFilter(f_low, f_high)` que ya existe, y el consumidor lo elige según su tasa; IncuNest pondrá 15 o 20 Hz para 50 Hz. Test con tono en Nyquist; sin medida previa.
2. IncuNest, esta rama: 50 Hz en `CommTask`, FIFO en el HMI, 176 puntos / hueco 12, constantes de geometría, `PROTOCOL.md`. Puede codificarse en paralelo con el 1 y flashearse cuando llegue el pin.
3. Banco (paso 4) con el simulador de Alex y un dedo: forma del latido, HWM, parpadeo, antes/después.

### 8.7 Propuesta a la librería (Alex, 2026-11-10): corte configurable y orden alto en el lado de arriba

- **El corte ya es configurable**: `setPPGDispFilter(f_low_hz, f_high_hz)` (defecto 0,5–20 Hz) existe desde v0.69, recalcula el biquad con la PRF real y lo reinicia (precarga en la siguiente muestra), bajo el mutex de estado; también es clave `$SET`/`$LCFG` (`ppgdisp_f_low_hz` / `ppgdisp_f_high_hz`). IncuNest no lo llama hoy, así que corre con el defecto. Elegir el corte desde `initSPO2()` es una línea, sin cambio de librería.
- **Lo que falta es el orden** del lado alto: hoy un solo biquad paso banda (1.er orden por lado). La propuesta a la librería es que `f_high` pase a ser el corte de un paso bajo Butterworth de **4.º orden** (`LowPass4`, ya existe para HR3 y PI) en cascada con el paso banda actual, que quedaría como paso alto de 1.er orden a `f_low` (el lado bajo no debe empinarse: §8 del paso 1). Misma API, mismas claves; cambia solo lo que `f_high` significa (de "−3 dB con pendiente de 6 dB/oct" a "−3 dB con 24 dB/oct" por encima; con las dos etapas, −6 dB en `f_high`).
- **El orden no debería ser configurable**: una sola implementación, un solo juego de tests, y 4.º orden ya cumple en todos los casos medidos; un parámetro más es combinatoria que nadie va a usar. Si la librería prefiere no cambiar el defecto (20 Hz), IncuNest fija el suyo explícitamente igual.
- **Regla para el consumidor** (a escribir en la spec §5.9 y en PROTOCOL/`SPO2.cpp` de IncuNest): con el paso bajo de 4.º orden, `f_high ≤ 0,4 × tasa_de_muestreo_del_consumidor` da ≥ 20 dB en Nyquist (15 Hz para 50 Hz: −22 dB); `f_high = 0,4 × tasa` es el máximo razonable, `0,3 × tasa` el holgado. A 25 Hz eso sería 8–10 Hz, que cuesta 4–6 px de pico: otro motivo para 50 Hz.
- **Reset**: el nuevo paso bajo debe entrar en el mismo `reset()`/precarga que el paso banda (v0.118, sin sonda) para no reabrir el transitorio de aplicación. `LowPass4::reset()` ya precarga cada sección.
- **Compatibilidad**: `NO_NORM` cambia bit a bit (más atenuación entre 10 y 20 Hz, retardo +20–28 ms); PPGDISPLAB y los tests de `test_ppgdisp` con senoides dentro de banda no se ven afectados en amplitud por debajo de ~8 Hz. Changelog y versión nueva; IncuNest sube el pin y añade `afe.setPPGDispFilter(0.5f, 15.0f)` (o 20) en `initSPO2()` con el comentario de la regla.
- Qué pide IncuNest de vuelta: versión etiquetada, una línea de spec con la regla del consumidor y el test con un tono en Nyquist (25 Hz) atenuado ≥ 20 dB para el defecto que se elija.
- **Brief redactado el 2026-11-10:** `2026-11-10-brief-libreria-ppg-disp-filtro-4o-orden.md` (enviado a la sesión de la librería el 2026-11-10).

## 7. Fuera de este plan, apuntado

- El PI tras reaplicar la sonda arranca en valores altos y baja exponencialmente en varios segundos (observación de Alex, 2026-11-09, dedo con perfusión baja). Queda para cuando haya un método de PI mejor o más rápido (espectral, pendiente en la librería). Plausiblemente comparte causa con el transitorio residual de `ppg_disp` entre 1 y 6 s (deriva de OT / escalones de HGAC tras la aplicación): la captura del paso 0 (OT, RF, ILED frente al tiempo en los primeros 6 s) lo aclararía de paso.

### 8.8 Implementado en IncuNest el 2026-11-10 (sin commitear, sin flashear)

- motherBoard: `CommTask.cpp` CTRL,PPG cada 20 ms (50 Hz); `SPO2.cpp` `initSPO2()` llama a `afe.setPPGDispFilter(0.5f, 20.0f)` con la regla del consumidor (15 Hz candidato cuando la librería publique el 4.º orden). `PpgSnapshot` sin cambios (ya a 50 Hz).
- HMI 4.7.0: FIFO de 32 muestras productor/consumidor entre el parser y la UI (`ctrl_ppg_fifo`, contador `dropped` en el log DIAG); `UITask` drena la cola en cada vuelta; `LOCK_PPG_POINTS` = 177 y `LOCK_PPG_GAP` = 12 derivados de 50 Hz, 460 px, 25 mm/s y 5,19 px/mm; el ancho del chart es ahora `LOCK_PPG_WIDTH_PX` (sigue en 460: ensancharlo es la decisión de §8.6).
- `PROTOCOL.md`: CTRL,PPG a 50 Hz, la tasa como constante del contrato, comportamiento con MB/HMI desparejados, FIFO.
- Compila: `pio run -e IncuNest_V18` y `Display_HMI -e main`. Falta: flashear las dos placas y el banco (paso 4) con el simulador; `ppg_dropped` del log DIAG debe quedarse en 0.

### 8.9 Respuesta de la librería (v0.119, 2026-11-10) y coste medido de la cadena nueva

- `incunest_afe4490` v0.119 publicada (commit 11ae84d, release con zip y SHA-256): tras el paso banda, dos bicuadráticas paso bajo en `f_high` con las Q de los pares de un Butterworth de 5.º orden (0,618 y 1,618; el polo real lo pone el paso banda). Decisión de Alex en la sesión de la librería: `f_high` es el punto de −3 dB de toda la cadena (no −6 dB como decía el brief). Misma API y claves; nada que llamar al actualizar; HR3 no cambia (el brief decía que `LowPass4` lo usaban HR3 y PI: solo HR3).
- **Regla del consumidor corregida** respecto al brief: `f_high ≤ 0,4 × R` da ≥ 10 dB en el Nyquist del consumidor (no 20 dB) y ≥ 40 dB en 2,5 × f_high; para ≥ 20 dB hace falta `f_high ≤ 0,3 × R`. Corregida en `SPO2.cpp` (`initSPO2()`) y en `PROTOCOL.md`.
- Respuesta de la cadena a 500 Hz (modelo con los coeficientes exactos de la librería; reproduce sus cifras):

  | f_high | 15 Hz | 20 Hz | 25 Hz | 50 Hz | 100 Hz | joroba | retardo de grupo 1–10 Hz |
  |---|---|---|---|---|---|---|---|
  | 20 Hz | −0,2 dB | −3,0 | −10,3 | −41,2 | −76 | +0,16 dB a 5,9 Hz | 29–88 ms |
  | 16 Hz | −1,8 | −10,3 | −19,8 | −51,0 | −86 | +0,20 dB a 5,0 Hz | 37–95 ms |
  | 15 Hz | −3,0 | −12,9 | −22,6 | −53,9 | −89 | +0,21 dB a 4,8 Hz | 40–98 ms |

  (El paso banda solo, v0.118: −3,0 a 20, −4,2 a 25, −9,0 a 50 Hz.)
- **Coste visible frente a la cadena de hoy** (paso banda 0,5–20 Hz de 2.º orden), SUBJ08–11, mediana de ventanas limpias de 10 s, retardo descontado, 1 px ≈ 1,1 % del p-p (latido a tamaño completo, 90 px):

  | f_high | diferencia máx. (p95) | en el pico sistólico | rms | p-p | retardo extra |
  |---|---|---|---|---|---|
  | 20 Hz | 1,9–2,5 % ≈ 2 px (2,4–4,3 %) | 0,1–0,35 % ≈ 0,3 px | 0,3–0,7 % | +0,2…+0,7 % | +18 ms |
  | 16 Hz | 2,4–4,1 % ≈ 3 px (4,1–5,1 %) | 0,1–0,4 % ≈ 0,4 px | 0,5–1,0 % | +0,2…+0,9 % | +24 ms |
  | 15 Hz | 2,4–4,0 % ≈ 3,5 px (4,6–5,3 %) | 0,2–0,4 % ≈ 0,4 px | 0,5–1,1 % | +0,2…+0,9 % | +26 ms |

  La diferencia máxima cae en los flancos (es el redondeo de la subida sistólica), no en el pico: el pico se mueve menos de medio píxel con cualquiera de los tres cortes. El p-p sube ligeramente por la joroba y porque el paso banda antiguo ya atenuaba 0,8 dB a 10 Hz. SUBJ10 tiene p95 alto (12–15 %) por artefactos de movimiento, no por el filtro.
- Script: `ppg_v0119_chain_cost.py` (scratchpad de la sesión, temporal; solo lectura de `PulseNest/captures/sessions/SUBJ08–11/*_p01.csv`).
- **Qué queda por decidir (Alex, sesión IncuNest):** subir el pin de `platformio.ini` a v0.119 y el `f_high` de `initSPO2()`: 20 Hz (defecto de la librería, nada visible cambia, 10 dB en Nyquist, 41 dB en la red) o 16 Hz (20 dB en Nyquist, 51 dB en la red, ~1 px más de redondeo en los flancos, +6 ms). 15 Hz ya no aporta nada sobre 16. Recomendación: 20 Hz, porque el contenido medido por encima de 25 Hz es despreciable y evita divergir del defecto.

- **Hecho el 2026-11-10 (noche):** Alex elige 20 Hz. Pin a v0.119 en `platformio.ini`, V18 compilado y flasheado por OTA en la motherBoard del banco (misma versión 18.57.0; reinicio verificado por el uptime). Queda el banco con la cadena nueva y el commit de la tarea 1.
