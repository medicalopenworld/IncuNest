# Proposal — sb-phase3-mic

## Why

El ruido ambiental dentro de la incubadora es clínicamente relevante para el neonato (rangos seguros de dB). La motherboard necesita un nivel sonoro periódico para monitorización/alarmas. Última fase de sensores antes de la cámara.

## What Changes

- Componente nuevo `mic_sensor`:
  - **ICS-41350** es un micrófono **PDM** (no I2S estándar): IO40 = clock, IO39 = data (ver `docs/hardware.md`). Se lee con el periférico I2S del S3 en **modo PDM RX** (`i2s_pdm.h`, canal RX con DMA), 16 kHz / 16 bit.
  - `audio_task` (prio 4): captura por ventanas (Kconfig, por defecto 1000 ms), calcula **RMS → dBFS → SPL estimado** con la sensibilidad del ICS-41350 (−26 dBFS @ 94 dB SPL ⇒ offset ≈ 120 dB, ajustable por Kconfig para calibración con sonómetro).
  - **Decisión de diseño (punto abierto del roadmap): muestreo periódico simple**, no detección de eventos por umbral — menor complejidad, la motherboard decide alarmas; un modo por umbral queda como extensión futura.
  - **Limitación documentada:** el valor publicado como `dba` es SPL **sin ponderación A** (el filtro A-weighting IEC 61672 queda diferido; requiere calibración con sonómetro de referencia que hoy no existe). El campo mantiene el nombre `dba` del protocolo del roadmap; el README lo advierte.
  - Gate de plausibilidad (lección Fase 2): valores fuera de [0, 140] dB o no finitos ⇒ no se publica y `sensors.mic` pasa a false.
  - Evento `{"type":"event","cmd":"sound_level","data":{"dba":42.3},"ts":…}` cada Kconfig (por defecto 5 s); registro `mic` en status.
  - Parte pura testable (`sb_audio_dsp`): RMS, conversión a dB, offset, gate, builder.

## Impact

- Affected specs: `mic-sensor` (nueva capability).
- Affected code: `components/mic_sensor/` (nuevo), `main/main.c`, `test_apps/mic_test/` (nuevo). No toca `usb_comm`.
- Riesgo del roadmap (contención I2S DMA vs USB CDC): `audio_task` a prio 4 < transporte 5, lectura por DMA con timeout — mismo patrón validado en Fases 2/4.
