# mic-sensor Specification

## Purpose
TBD - created by archiving change sb-phase3-mic. Update Purpose after archive.
## Requirements
### Requirement: Análisis de ventana puro con detección de señal viva

`mic_sensor` SHALL analizar cada ventana int16 en funciones puras (sin I2S/RTOS): RMS de la **componente AC** (media eliminada — el DC no es ruido acústico), y detección de **señal viva** (pico-a-pico ≥ umbral). Como el S3 genera el clock PDM, una línea DIN pegada o flotante produce lecturas válidas a nivel driver: una ventana casi constante SHALL marcarse como no viva y no publicarse.

#### Scenario: RMS AC de señal alternante

- **WHEN** las muestras alternan ±A
- **THEN** el RMS es A y la señal es viva

#### Scenario: Señal constante = línea pegada

- **WHEN** todas las muestras valen el mismo valor (incluidos 0 y 32767)
- **THEN** el RMS AC es 0 y la señal NO es viva

#### Scenario: RMS de ventana vacía

- **WHEN** la ventana tiene 0 muestras
- **THEN** el RMS es 0 y la señal no es viva (sin dividir por cero)

#### Scenario: Fullscale alternante ≈ 0 dBFS + offset

- **WHEN** las muestras alternan ±32767 y el offset es 120
- **THEN** el nivel es 120 dB ± 0.1

#### Scenario: Silencio no produce -inf

- **WHEN** el RMS es 0
- **THEN** el nivel resultante es el suelo definido (0 dB), finito

### Requirement: Gate de plausibilidad

Un nivel no finito o fuera de [0, 140] dB SHALL descartarse: no se publica y la disponibilidad de `mic` pasa a false.

#### Scenario: Valor fuera de rango

- **WHEN** el nivel calculado excede 140 dB
- **THEN** el gate lo marca inválido

#### Scenario: Límite exacto aceptado

- **WHEN** el nivel es exactamente 140.0 dB
- **THEN** el gate lo acepta

### Requirement: Evento sound_level

`mic_sensor` SHALL publicar periódicamente `{"type":"event","cmd":"sound_level","data":{"dba":<nivel>},"ts":<ms>}` con un decimal, y SHALL registrar `mic` en el status.

#### Scenario: JSON exacto

- **WHEN** se construye el evento con nivel 42.3 y ts=8100
- **THEN** el resultado es exactamente `{"type":"event","cmd":"sound_level","data":{"dba":42.3},"ts":8100}`

#### Scenario: Nivel inválido no emite

- **WHEN** el gate marcó el nivel como inválido
- **THEN** el builder devuelve 0 y no se envía frame

