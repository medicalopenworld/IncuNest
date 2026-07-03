# env-sensors Specification

## Purpose
TBD - created by archiving change sb-phase2-env-sensors. Update Purpose after archive.
## Requirements
### Requirement: Conversión SHT4x pura y validada

El componente `env_sensors` SHALL convertir las palabras crudas del SHT4x a °C y %RH con las fórmulas del datasheet (T = −45 + 175·raw/65535; RH = −6 + 125·raw/65535, RH acotada a [0,100]) y SHALL validar cada palabra con CRC-8 Sensirion (poly 0x31, init 0xFF) antes de convertir.

#### Scenario: CRC-8 vector conocido

- **WHEN** se calcula el CRC-8 Sensirion de los bytes `0xBE 0xEF`
- **THEN** el resultado es `0x92`

#### Scenario: Conversión de temperatura

- **WHEN** raw_temp = 0x6666 (26214)
- **THEN** la temperatura es 25.0 °C ± 0.01

#### Scenario: Humedad acotada

- **WHEN** raw_rh produce un valor fuera de [0,100]
- **THEN** el resultado se satura al límite correspondiente

### Requirement: Evento sensor_data con redundancia posicional

`env_sensors` SHALL publicar periódicamente `{"type":"event","cmd":"sensor_data","data":{"temp":[t0,t1,t2],"hum":[h0,h1,h2],"lux":L},"ts":<ms>}` donde la posición i corresponde al sensor i (sht0/sht1/sht2) y un sensor en fallo aparece como `null` sin omitir la posición.

#### Scenario: Los tres sensores responden

- **WHEN** las tres lecturas SHT40 son válidas y el ADC responde
- **THEN** el JSON contiene tres números en `temp`, tres en `hum` y un número en `lux`

#### Scenario: Un sensor en fallo

- **WHEN** sht1 no responde (NACK/timeout/CRC inválido)
- **THEN** `temp[1]` y `hum[1]` son `null`, las demás posiciones llevan número y el evento se publica igualmente

#### Scenario: Fallo total no bloquea

- **WHEN** ningún sensor responde
- **THEN** la tarea sigue viva, el evento lleva `null`/valores ausentes y la disponibilidad registrada pasa a false

### Requirement: Lectura ALS por ADC

`env_sensors` SHALL leer el ALS-PT19 por ADC oneshot (IO1/ADC1_CH0) y convertir mV→lux con el factor de Kconfig `SB_ALS_UV_PER_LUX`, documentado como no calibrado.

#### Scenario: Conversión lux

- **WHEN** la lectura es 1000 mV y el factor es 1000 µV/lux
- **THEN** el valor publicado es 1000 lux

