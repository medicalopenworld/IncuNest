# Tasks — sb-phase3-mic

## 1. Red

- [x] 1.1 `mic_sensor` con DSP puro stub (`sb_audio_dsp.h/.c`) + `test_apps/mic_test` con TEST_CASE por Scenario — fallan

## 2. Green

- [x] 2.1 Implementar DSP puro (RMS, dB con suelo, offset, gate [0,140], builder)
- [x] 2.2 Driver I2S PDM RX (IO40 clk / IO39 data, 16 kHz/16 bit) + `audio_task` prio 4 con ventana Kconfig, publicación y `sensors.mic`
- [x] 2.3 `main.c`: init no fatal

## 3. Verify

- [x] 3.1 Builds en verde (app + mic_test); Scenarios cubiertos

## 4. Review

- [ ] 4.1 code-reviewer + security-reviewer; hallazgos resueltos

## 5. Docs / cierre

- [ ] 5.1 README/CHANGELOG (advertir dba sin ponderación A); archivar; retro; checkbox EPIC-001
