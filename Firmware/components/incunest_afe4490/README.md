# incunest_afe4490

AFE4490 PPG/SpO2 driver for ESP32-S3 (Arduino + FreeRTOS) — [Medical Open World](http://medicalopenworld.org)

Part of the [IncuNest](https://github.com/medicalopenworld/IncuNest) neonatal incubator project.

## Features

- Non-blocking FreeRTOS task: DRDY ISR → SPI read → signal processing → queue
- SpO2 (R-ratio method, calibratable coefficients)
- HR1: peak detection
- HR2: autocorrelation
- HR3: FFT + Harmonic Product Spectrum
- Signal Quality Index (SQI) for SpO2 and each HR algorithm
- Runtime-configurable gain, LED current, filter cutoffs and sample rate

## Requirements

- ESP32-S3
- Arduino framework + FreeRTOS
- AFE4490 connected via SPI

## Usage

See [`examples/basic/main.cpp`](examples/basic/main.cpp) for a minimal working example.

```cpp
#include "incunest_afe4490.h"

INCUNEST_AFE4490 afe;

void setup() {
    SPI.begin(SCK, MISO, MOSI, -1);
    afe.begin(CS_PIN, DRDY_PIN);
}
```

In your `platformio.ini`:

```ini
lib_deps =
    https://github.com/medicalopenworld/incunest_afe4490.git#v0.18
```

## Specification

See [`incunest_afe4490_spec.md`](incunest_afe4490_spec.md) for the full design specification.
The spec and the library are versioned together (same semantic version).

## References

- [AFE4490 Datasheet (Texas Instruments)](https://www.ti.com/lit/ds/symlink/afe4490.pdf) — primary hardware reference for register map, timing formulas, and electrical characteristics.

## License

Proprietary — Medical Open World. All rights reserved.
