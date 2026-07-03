# mic-sensor (delta)

## ADDED Requirements

### Requirement: Cálculo de nivel puro y verificable

`mic_sensor` SHALL calcular el RMS de una ventana de muestras int16 y convertirlo a dB (20·log10(rms/fullscale)) más un offset de calibración, en funciones puras sin I2S/RTOS.

#### Scenario: RMS de señal constante

- **WHEN** todas las muestras valen A
- **THEN** el RMS es |A|

#### Scenario: RMS de ventana vacía

- **WHEN** la ventana tiene 0 muestras
- **THEN** el RMS es 0 (sin dividir por cero)

#### Scenario: Fullscale constante ≈ 0 dBFS + offset

- **WHEN** las muestras valen 32767 y el offset es 120
- **THEN** el nivel es 120 dB ± 0.1

#### Scenario: Silencio no produce -inf

- **WHEN** todas las muestras son 0
- **THEN** el nivel resultante es el suelo definido (0 dB), finito

### Requirement: Gate de plausibilidad

Un nivel no finito o fuera de [0, 140] dB SHALL descartarse: no se publica y la disponibilidad de `mic` pasa a false.

#### Scenario: Valor fuera de rango

- **WHEN** el nivel calculado excede 140 dB
- **THEN** el gate lo marca inválido

### Requirement: Evento sound_level

`mic_sensor` SHALL publicar periódicamente `{"type":"event","cmd":"sound_level","data":{"dba":<nivel>},"ts":<ms>}` con un decimal, y SHALL registrar `mic` en el status.

#### Scenario: JSON exacto

- **WHEN** se construye el evento con nivel 42.3 y ts=8100
- **THEN** el resultado es exactamente `{"type":"event","cmd":"sound_level","data":{"dba":42.3},"ts":8100}`

#### Scenario: Nivel inválido no emite

- **WHEN** el gate marcó el nivel como inválido
- **THEN** el builder devuelve 0 y no se envía frame
