# Proposal — sb-ota-receiver

## Why

El SensorBoard no tiene ninguna vía de actualización. No existe una sola llamada `esp_ota_*` en el código, y su único enlace con el exterior es el cable USB CDC a la motherboard: no lleva radio, no tiene consola (`CONFIG_ESP_CONSOLE_NONE=y`) y no expone ningún puerto accesible sin abrir la incubadora. Cada corrección de firmware, durante toda la vida del producto, exige hoy una visita con cable.

El reparto de particiones del 2026-09-06 dejó lista la mitad de hardware: la tabla anterior declaraba `factory` + `ota_0` + `ota_1` de 3 MB **sin partición `otadata`**, así que `esp_ota_set_boot_partition()` no tenía dónde escribir y el bootloader habría arrancado siempre `factory` — 6 MB reservados para una capacidad que no existía. `partitions.csv` declara ahora dos slots OTA reales de 2 MB con su `otadata` en `0xD000`, dimensionados sobre una app de 356 KB. Los slots están; no los usa nadie.

Este cambio añade la mitad receptora: un componente que acepta una imagen empujada por la motherboard sobre el framing de `usb_comm` que ya existe, la escribe en el slot inactivo, la verifica y la arranca bajo rollback supervisado por el bootloader. La mitad emisora — manifiesto de bundle, orquestación y la puerta de mantenimiento — vive en el root de OpenSpec hermano (`Firmware/openspec/`, cambio `shared-cascade-ota-distribution`) y no se mezcla nunca con este root, según la regla de scope que declaran los dos `config.yaml`.

Dos particularidades de esta placa hacen los requisitos más estrictos que los de un receptor OTA cualquiera:

1. **Sus SHT40 son la variable del PID de aire de la motherboard** y la fuente de sus cortes térmicos — no es telemetría auxiliar, y quedarse mudo tiene significado clínico. Que la placa se reinicie para estrenar imagen levanta `ALARM_AIR_SENSOR_FAULT` por el mecanismo normal de 5 s.
2. **El descriptor USB es un contrato.** `sdkconfig.defaults` fija VID 0x303A / PID 0x4001 porque la motherboard abre el dispositivo por esos números, y TinyUSB deriva el PID de las clases habilitadas. Cualquier cambio del conjunto de clases mueve el PID y rompe el enlace en silencio, sin error de compilación. Por eso este cambio añade un **tipo de frame**, nunca una clase USB.

## What Changes

- **Nuevo componente ESP-IDF `ota`** con su propia tarea FreeRTOS, siguiendo el patrón establecido: las fases nuevas añaden su componente y hablan a través de `usb_comm` en vez de reimplementar transporte.
- **Nuevo tipo de frame binario `SB_PROTO_TYPE_OTA` (0x02)** en `sensorBoard_comm_protocol.h`, con un subopcode (offer, accept/refuse, chunk, chunk-ack, commit, abort, confirm) y su payload. Es una **excepción deliberada** a la regla de este proyecto de no tocar `usb_comm` después de la fase 1: esa regla asume que las fases nuevas solo necesitan *emitir*, y una imagen de firmware es lo primero que tiene que **llegar** como binario. La alternativa —base64 dentro de `SB_PROTO_TYPE_JSON`, con payload tope de 256 B— costaría un tercio más de bytes y unos 1.400 frames en vez de 87. El framing en sí (magic, cabecera, CRC16-CCITT, validación de longitud) no cambia.
- **Nueva ruta de escritura OTA** con `esp_ota_begin`/`write`/`end` sobre el slot inactivo, con chunks dimensionados al buffer RX de CDC (`CONFIG_TINYUSB_CDC_RX_BUFSIZE=4096`) y alineados a página de flash.
- **Nuevo rollback supervisado por el bootloader**: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` en `sdkconfig.defaults`; la imagen nueva queda en pending-verify y solo se confirma cuando la motherboard avisa de que ha visto su heartbeat. Una imagen que arranca pero no habla la revierte el bootloader, no la aplicación.
- **Nueva interacción con el vigilante de host**: `sensorBoard_host_watch.c` reinicia la placa ante suspend/resume para poder re-enumerar. No debe dispararse a mitad de una transferencia, o el reinicio ocurriría con el slot inactivo a medio escribir.
- **Nuevo campo `ota_state` en la resp de `status`**, para que la motherboard distinguya idle / recibiendo / pending-verify sin mantener un estado espejo propio.

## Capabilities

### New Capabilities
- `ota-receiver`: el componente en sí — manejo de los subopcodes, validación de la oferta contra la capacidad del slot, escritura por chunks en el slot inactivo, verificación del digest, commit, abort, y la suspensión del vigilante de host mientras dura la transferencia.
- `ota-rollback`: el contrato de rollback — pending-verify en el primer arranque de una imagen nueva, resolución solo con la confirmación de la motherboard, reversión en cualquier otro caso, y qué reporta `status` en cada estado.

### Modified Capabilities
- `usb-transport`: gana un tipo de frame binario (`0x02`) y su despacho de entrada.
- `command-dispatch`: la resp de `status` gana el campo `ota_state`.

## Impact

- **Código afectado**: nuevo `components/ota/`; `components/usb_comm/include/sensorBoard_comm_protocol.h` (constante de tipo y subopcodes); `components/usb_comm/sensorBoard_frame.c` (despacho del tipo nuevo); `sensorBoard_cmd_builder.c` (`ota_state` en la resp de `status`); `sensorBoard_host_watch.c` (ventana de supresión); `main/main.c` (arranque de la tarea); `CMakeLists.txt` raíz y del componente.
- **Configuración afectada**: `sdkconfig.defaults` gana `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. Aplican literalmente las dos trampas registradas en el CHANGELOG: (a) `set(COMPONENTS ...)` mínimo deja componentes fuera del build, y entonces sus símbolos Kconfig no existen y `confgen` **descarta las líneas de `sdkconfig.defaults` en silencio** — compila, arranca y no hace nada; (b) `sdkconfig` está gitignored y **no** se regenera al cambiar `sdkconfig.defaults`, hay que borrarlo. Las dos hay que comprobarlas después de editar, no darlas por hechas.
- **Docs afectadas**: `README.md`, `CHANGELOG.md`, y el roadmap `docs/superpowers/specs/2026-07-03-sensorboard-roadmap.md`, que gana este trabajo fuera de las cinco fases originales.
- **Dependencia en el otro sentido**: el emisor lo aporta `shared-cascade-ota-distribution` en `Firmware/openspec/`. Este cambio se verifica solo hasta "la placa acepta frames y escribe un slot"; la verificación extremo a extremo necesita los dos.
- **Testing**: Unity en `test_apps/comm_test` para la lógica pura (validación de la oferta contra el tamaño del slot, parseo de subopcodes, secuencia y longitud de chunk, acumulación del digest, política del vigilante de host durante la transferencia), con `idf.py build` como puerta automatizable. Todo lo que toca flash, USB o un reinicio es **verificación manual** con `idf.py -p COMx flash monitor`, y nunca se automatiza en un hook. Ojo al ejecutar la suite: `idf.py` solo da resultado fiable desde PowerShell, y `test_sensorboard_frame` arrastra un ERRORED 0xC0000139 preexistente que no es de este cambio — hay que mirar el SUMMARY suite a suite.
- **Fuera de alcance**: verificación de firma e imagen firmada / secure boot; cualquier cambio del descriptor USB o del conjunto de clases habilitadas; la orquestación, el manifiesto y la puerta de mantenimiento de la motherboard.
