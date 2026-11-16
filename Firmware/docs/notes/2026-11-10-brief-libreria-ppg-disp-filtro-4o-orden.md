# Brief — librería `incunest_afe4490`: lado alto del filtro de `ppg_disp` a 4.º orden, corte elegido por el consumidor

Fecha: 2026-11-10 · Para: la sesión de PulseNest / `incunest_afe4490` · Origen: sesión IncuNest "Pulsioximetría en HMI", tarea 1 (plan `2026-11-09-plan-bpf-ppg-disp-diezmado-25hz.md`, §8)

## Qué se pide

1. Que el lado alto del filtro de display de `ppg_disp` pase de 1.er orden (el biquad paso banda actual, 6 dB/oct por lado) a **4.º orden Butterworth** (`LowPass4`, el de HR3 y PI), en cascada con el paso banda actual, que queda como paso alto de 1.er orden en `f_low`. El lado bajo **no** debe empinarse: un paso alto de orden alto a 0,5 Hz tiene τ y sobreimpulso de segundos ante derivas de OT, justo lo que se arregló en v0.118.
2. **Misma API y mismas claves**: `setPPGDispFilter(f_low_hz, f_high_hz)` y `ppgdisp_f_low_hz` / `ppgdisp_f_high_hz`. Cambia solo lo que `f_high` significa: corte a −3 dB del paso bajo de 4.º orden (con las dos etapas, −6 dB en `f_high`). El orden **no** configurable: una implementación, un juego de tests.
3. Una **regla para el consumidor** en la spec §5.9: con 4.º orden, `f_high ≤ 0,4 × tasa de muestreo del consumidor` da ≥ 20 dB en Nyquist (0,3 × tasa, holgado). IncuNest va a muestrear `ppg_disp` a 50 Hz y fijará `f_high` = 15 o 20 Hz explícitamente en `initSPO2()`, aunque coincida con el defecto. El defecto de la librería lo decide Alex (20 Hz lo mantiene; 15 Hz da 10 dB más de margen al mismo coste visible).
4. El paso bajo nuevo entra en el **mismo `reset()`/precarga** que el paso banda (v0.118: reiniciado mientras `probe_state ≠ PROBE_APPLIED`, precargado en la primera muestra aplicada). `LowPass4::reset()` ya precarga cada sección.
5. Test en `test_ppgdisp`: un tono en el Nyquist del consumidor (25 Hz para 50 Hz) atenuado ≥ 20 dB con el defecto elegido; el test 10 (escalón de aplicación) sigue precargado.

## Por qué (medido en SUBJ08–11, HOSPNAV, 2026-11-10; solo lectura de `captures/sessions`)

Referencia = la salida del paso banda actual 0,5–20 Hz a 500 Hz (`x = −BPF(OT_LED1)`), ventanas de 10,24 s con sonda aplicada, artefactos descartados (p-p > 3× mediana). Mediana de ventanas. 1 px del chart de IncuNest ≈ 1,1 % de la amplitud pico a pico del latido.

**Armónicos del PPG neonatal** (amplitud respecto al fundamental; HR 158–170 lpm):

| | h2 (~5,5 Hz) | h3 (~8 Hz) | h4 (~11 Hz) | h5 (~13,5 Hz) | h6 (~16 Hz) | potencia > 12,5 Hz |
|---|---|---|---|---|---|---|
| SUBJ08 (PI p50 0,72 %) | 27 % | 9,8 % | 5,0 % | 2,3 % | 1,0 % | 0,07 % |
| SUBJ09 (PI 3,7 %) | 26 % | 8,3 % | 3,8 % | 1,4 % | 0,7 % | 0,03 % |
| SUBJ10 (PI 1,5 %) | 28 % | 7,8 % | 4,6 % | 1,9 % | 0,8 % | 0,05 % |
| SUBJ11 (PI 2,6 %) | 32 % | 11,8 % | 6,5 % | 3,3 % | 1,5 % | 0,15 % |

Hay armónicos hasta ~16 Hz; no hay muesca dicrótica visible. Masimo (7,5 Hz, 1997) no sirve de justificación: no conocemos sus razones.

**Coste visible de un paso bajo de 4.º orden** (retardo de grupo descontado por correlación):

| LP4 | diferencia RMS | diferencia máxima (pico sistólico) | retardo |
|---|---|---|---|
| 20 Hz | 0,3–0,55 % | 1,7–2,4 % (~2 px) | 20 ms |
| 15 Hz | 0,3–0,6 % | 1,6–2,2 % (~2 px) | 28 ms |
| 12 Hz | 0,5–1,0 % | 2,8–4,1 % (~3 px) | 36 ms |
| 10 Hz | 0,6–1,6 % | 3,9–5,3 % (~5 px) | 42 ms |
| 8 Hz | 0,8–2,2 % | 4,6–6,6 % (~6 px) | 54 ms |

**Plegado de señal y ruido propios** al muestrear (RMS de lo que cae en banda, % de p-p):

| filtro | consumidor a 25 Hz | consumidor a 50 Hz |
|---|---|---|
| BPF actual (2.º orden) | 0,22–0,96 % | 0,04–0,09 % |
| + LP4 20 Hz | 0,20–0,91 % | 0,01–0,02 % |
| + LP4 15 Hz | 0,15–0,72 % | ≤ 0,01 % |
| + LP4 12 Hz | 0,09–0,43 % | ≤ 0,01 % |

A 50 Hz es despreciable con cualquier corte, incluso con el filtro actual, y el PI bajo (SUBJ08) está dentro de la muestra. **El motivo del 4.º orden no es la señal: es la interferencia determinista** (red 50/100 Hz, drivers de fototerapia, calefactor; spec §7, "riesgo residual real, no medido"), que a 50 Hz de transporte cae exactamente sobre la línea base:

| cadena completa | 20 Hz | 25 Hz | 50 Hz (red) | 100 Hz |
|---|---|---|---|---|
| BPF actual | −3,0 | −4,2 | −9,0 | −15,5 dB |
| + LP4 20 Hz | −6,0 | −12,7 | −41,8 | −76 |
| + LP4 15 Hz | −13,5 | −22,2 | −51,9 | −86 |
| + LP4 12 Hz | −20,9 | −29,9 | −59,7 | −94 |

Conclusión: lo que manda es el **orden**; el corte entre 15 y 20 Hz es margen, no fidelidad. 12 Hz ya no se justifica con 50 Hz de transporte; 8 Hz descartado.

## Lo que hace IncuNest en paralelo (misma rama `feat/hmi-pulse-oximetry`)

- `CTRL,PPG` de 25 a **50 Hz** (motherBoard `CommTask`), FIFO entre el parser y la UI del HMI (hoy un buzón de una plaza), chart a **25 mm/s** (ventana 3,5 s = 177 puntos a 50 Hz con 460 px; se estudia ensanchar el chart), `PROTOCOL.md`.
- `initSPO2()`: `afe.setPPGDispFilter(0.5f, <15 o 20>)` con el comentario de la regla del consumidor.
- Banco con el simulador y un dedo al llegar el pin: forma del latido, HWM del HMI, parpadeo, antes/después.

## Qué necesita IncuNest a cambio

- Versión etiquetada y una línea de changelog: qué cambia en `NO_NORM` (atenuación entre 10 y 20 Hz, retardo +20–28 ms; nada por debajo de ~8 Hz) y el defecto de `f_high` elegido.
- La regla del consumidor en la spec §5.9 (y en el rationale §21.7, donde el filtro y la tasa estaban "abiertos").
- Los scripts de medida son temporales (`ppg_harmonics.py`, `ppg_alias_noise.py`, en el scratchpad de la sesión IncuNest; leen solo `PulseNest/captures/sessions/SUBJ08–11/*_p01.csv`): si la sesión de la librería los quiere para PPGDISPLAB, se le entregan tal cual para que los versione en PulseNest `tools/`.

## Aparte, ya apuntado (no es de este brief)

- Transitorio residual 1–6 s tras aplicar la sonda con v0.118 (DC sube y vuelve, AC crece) y el PI que arranca alto y baja exponencialmente al reaplicar con perfusión baja: ver `2026-11-09-brief-libreria-ppg-disp-arranque-lento.md`, añadido del 9-nov. Pendiente de un PI mejor o más rápido (espectral).

## Respuesta de la librería (2026-11-10)

v0.119 publicada (11ae84d): dos bicuadráticas en `f_high` con las Q de los pares de un Butterworth de 5.º orden; `f_high` = −3 dB de toda la cadena (decisión de Alex, no −6 dB como pedía este brief); con 20 Hz: −10,3 dB a 25 Hz, −41,2 dB a 50 Hz, −76 dB a 100 Hz. La regla del consumidor queda `f_high ≤ 0,4 × R` para ≥ 10 dB en Nyquist (no 20 dB como decía aquí), `≤ 0,3 × R` para ≥ 20 dB. `LowPass4` lo usa solo HR3 (no PI). El coste en el pico que la librería dejó sin medir está en el plan, §8.9: < 0,5 px en el pico, ~2 px en los flancos con 20 Hz.
