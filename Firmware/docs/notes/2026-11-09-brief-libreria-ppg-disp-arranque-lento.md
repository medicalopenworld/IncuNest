# Brief — librería `incunest_afe4490`: `ppg_disp` (NORM/NORM_PI) tarda > 5 s en escalar al aplicar la sonda

Fecha: 2026-11-09 (análisis del 2026-11-08) · Para: la sesión de PulseNest / `incunest_afe4490` · Origen: sesión IncuNest "Pulsioximetría en HMI", tarea 1 (brief `2026-11-05-brief-tarea1-autozoom-ppg.md`)

## Observado

IncuNest motherBoard 18.57.0 (rama `feat/hmi-pulse-oximetry`, sin commitear) con `incunest_afe4490` v0.117 y `setPPGDispMode(NORM_PI)`; `CommTask` envía `128 + ppg_disp × 127` al HMI. Flasheada en la V18 el 2026-11-08. Con el simulador, al aplicar la sonda **la traza tarda más de 5 s en llenar el chart**. Es comportamiento de la librería, no del transporte ni del HMI (los dos son lineales y sin estado).

## Causas (por lectura del código v0.117, no medido)

### 1. El escalón de OT al aplicar la sonda entra en el seguidor SLEW (causa principal)

- `_ppgdisp_bpf` **no se reinicia ni se precarga nunca con el estado de la sonda**: corre continuo desde `begin()` en todos los estados (`_process_sample()` → `_ppgdisp_bpf.process(raw_ppg)` → `_ppgdisp_update(-filtered, probe_state)`). `_pi_bpf`, en cambio, se hace `reset()` mientras la sonda no está aplicada y `BiquadFilter::process()` lo precarga al estacionario en la primera muestra.
- Al aplicar la sonda la OT salta de ~5,9e-4 (aire) a ~1,2e-4 (aplicada; SUBJ08–11, spec §5.8). Escalón ≈ 4,7e-4 frente a un latido de ≈ 1,2e-6 a PI 1 %: **~400× el latido**. El paso alto de 0,5 Hz lo deja decaer con τ = 1/(2π·0,5) ≈ 0,32 s: ~1,9 s hasta quedar por debajo del latido.
- `PROBE_APPLIED` se confirma a los 0,2 s (`rsqm_probe_state_min_s`). El seguidor arranca el **calentamiento de 1 s con los extremos crudos**, y el marco queda ≈ [~0, 2,5e-4]: 100–200× el latido. El primer `ppg_disp` sale pegado al raíl inferior, casi plano.
- Los extremos del transitorio siguen en la ventana de 1 s otro segundo más (`t_hi/t_lo` = máx/mín de la ventana).
- Después el marco **cae exponencialmente con τ = 1 s**: ×100 son ln(100) ≈ 4,6 s.

Total ≈ 0,2 + 1 + 1 + 4,6 ≈ **7 s**. La spec §5.9 documenta la bajada solo para un escalón ×0,5 (98 % a los 5 s); el caso ×100 de la aplicación no está contemplado y `test/test_ppgdisp/` arranca con sinusoides sin escalón de DC.

Contribución secundaria del mismo tipo: mientras HGAC nivela RF/ILED en los primeros segundos, cada cambio de RF deja en la OT un escalón residual (tolerancia de RF, ~1 % de la DC ≈ el latido), que el BPF convierte en transitorio y el seguidor sube a ×1,5/s.

### 2. PI es NaN los primeros 6 s → A = 0,5

`PI-METHOD-1` no publica hasta llenar `pi_window_s` = 6 s; `_ppgdisp_amp_of_pi(NaN)` devuelve el suelo 0,5 (decisión de diseño: "perfusión desconocida se muestra al mínimo", §5.9). En `NORM_PI` la traza queda a **media altura hasta t ≈ 6 s** aunque el PI sea alto, y entonces salta a tamaño completo.

## Alternativas valoradas

| # | Dónde | Qué | Pros | Contras |
|---|---|---|---|---|
| **A** | Librería | Precargar `_ppgdisp_bpf` al pasar a `PROBE_APPLIED` (y/o `reset()` mientras no está aplicada), como `_pi_bpf`. | Ataca la raíz; coste cero; coherente con PI/SpO2. Esperado: 0,2 + 1 ≈ **1,2 s**. | Rompe la identidad bit a bit de `NO_NORM` con v0.69–v0.115 en el instante de aplicar (limitable a los modos NORM). No quita los escalones residuales de HGAC. |
| B | Librería | Blanking de 2–3 s tras `PROBE_APPLIED` antes del calentamiento. | Trivial. | Oculta el síntoma; 2–3 s fijos de línea plana; peor que A en todo. |
| C | Librería | Reiniciar el calentamiento cuando los objetivos quedan muy por debajo del marco durante una ventana (como ya se hace con el marco plano). | Cubre cualquier colapso grande de amplitud (sonda a medias, cambio de dedo). | Heurística nueva con umbral que justificar y testear; mal elegido, una bajada real de PI dispara 1 s de línea plana. |
| D | IncuNest | `setPPGDispNormTracker(0.5, 1.0, 0.2–0.3, 1.15)` (τ de bajada corto). | Una línea, sin tocar la librería. | El marco encoge dentro del ciclo cardíaco neonatal (0,3–0,5 s) y la traza "respira", lo que se descartó al RMS en PPGDISPLAB. Solo como diagnóstico. |
| **E** | Librería | Con PI NaN, A = 1 o el último A conocido, en vez del suelo. | Tamaño completo desde el primer latido. | Invierte una decisión explícita; con PI bajo la traza *encogería* a los 6 s (parece un deterioro). Alternativa: mantener 0,5 y documentar el salto. |
| F | IncuNest | Acortar `pi_window_s`. | — | **Descartada**: cambia la medida de PI/SQI por un motivo de pantalla. |
| G | HMI | Autoescalar en pantalla. | — | **Descartada**: regla de Pablo (e3a71567); el HMI no tiene la información. |

Recomendación de la sesión IncuNest: **A** (es un olvido, no una decisión: el filtro de PI ya lo hace), y decidir **E** conscientemente (dejar A = 0,5 con el salto a los 6 s, o como mucho el último A conocido para la reaplicación). PPGDISPLAB puede reproducir la aplicación de la sonda con una captura real antes de tocar nada; conviene un test en `test_ppgdisp` con un escalón de DC ×400 al aplicar.

## Qué necesita IncuNest a cambio

- Una versión etiquetada (v0.118+) y una línea de changelog con qué cambia en `NO_NORM` (si cambia) y el tiempo esperado hasta la traza completa.
- Si la solución añade un parámetro, su setter y su valor por defecto; IncuNest no lo tocará salvo que haga falta.
- Al llegar: subir el pin en `Firmware/motherBoard/platformio.ini`, compilar `IncuNest_V18` (gnu++11), flashear por OTA y repetir la prueba del simulador con PI bajo y alto. Hasta entonces la tarea 1 sigue abierta y 18.57.0 sin commitear.

## Añadido 2026-11-09 (tarde): v0.118 en banco y un pendiente de PI

- v0.118 flasheada en la V18 (18.57.0). Alex observa en NORM_PI: 0–1 s plano; 1–2 s la DC sube; 2–6 s la DC vuelve al centro y la AC crece; a los 6 s salto (A: 0,5 → A(PI), aceptado). El tramo 1–6 s es un transitorio residual que v0.117 enmascaraba: lectura = deriva lenta de OT tras aplicar (asentamiento y/o escalones de HGAC) que pasa por el HP de 0,5 Hz de 1.er orden y a la que el marco SLEW (subida ≤ 0,5×rango/s, bajada τ = 1 s) se ajusta despacio. Sin medir. Comunicado a la sesión de la librería como adelanto.
- **Pendiente, PI (Alex, 2026-11-09):** con un dedo de perfusión baja (Raynaud, agua fría), al reconectar la sonda el PI empieza mostrándose en valores muy altos y baja exponencialmente durante bastantes segundos hasta el valor definitivo. Se deja apuntado como mejora/corrección para cuando exista un método de PI mejor o más rápido (cálculo espectral, pendiente). Verosímilmente comparte causa con el transitorio anterior.
- Siguiente trabajo de display: plan del filtro frente al diezmado a 25 Hz en `2026-11-09-plan-bpf-ppg-disp-diezmado-25hz.md`.
