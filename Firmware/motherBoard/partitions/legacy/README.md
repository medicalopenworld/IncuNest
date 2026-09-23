# Particiones en desuso — motherboard

Ninguna de estas tablas la usa un `[env]` de `platformio.ini`. Son del ESP32
clasico (hardware v14 y anteriores, placa FireBeetle32, ver `docs/hardware.md`);
desde la v15 la motherboard monta un ESP32-S3 y la tabla viva es
`partitions/ESP32S3_8MB.csv`.

Ojo con el nombre: `ESP32_16MB.csv` es del ESP32 clasico y no tiene nada que ver
con `partitions/ESP32S3_16MB.csv`, que es la tabla preparada por si el modulo
S3 montado resulta ser de 16 MB.

- **`ESP32_4MB.csv`** — app0/app1 de 1,9375 MB y 64 KB de SPIFFS.
- **`ESP32_16MB.csv`** — app0/app1 de 2,9375 MB y 10,0625 MB de SPIFFS.

Ambas sin particion de coredump, como estaba la tabla del S3 hasta septiembre
de 2026.
