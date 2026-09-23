# Particiones y boards en desuso — Display HMI

Nada de esta carpeta lo usa ningun `[env]` de `platformio.ini`. Se conserva
como referencia historica; si algo de aqui vuelve a hacer falta, moverlo de
vuelta a `partitions/` y apuntarlo desde `board_build.partitions`.

- **`display_v0.csv`** — reparto de 4 MB (app0/app1 de 1,75 MB, SPIFFS de
  384 KB) del primer prototipo del HMI, antes de pasar al modulo de 16 MB. La
  tabla viva es `partitions/hmi_16mb_ota.csv`.

- **`esp32-s3-devkitc-1-myboard.json`** — manifest de board personalizado
  ("ESP32-S3-DevKitC-1-N8 -ELECROW") que declaraba 4 MB de flash y apuntaba a
  `IncuNest_display_v0.csv`, un fichero que ya no existe con ese nombre desde
  la reorganizacion de carpetas de junio de 2026. Nunca llego a seleccionarse:
  `platformio.ini` usa `board = esp32-s3-devkitc-1`, el manifest de serie del
  platform. Al retirarlo se quito tambien `boards_dir = .` de `platformio.ini`,
  que solo existia para que PlatformIO encontrase este fichero.
