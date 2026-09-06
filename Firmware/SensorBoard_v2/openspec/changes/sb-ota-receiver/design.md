# Design — sb-ota-receiver

## Context

El SensorBoard es la única de las tres placas de IncuNest sin ninguna ruta de actualización remota, y la única cuyo enlace es un cable interno. Su tabla de particiones ya ofrece `ota_0`/`ota_1` de 2 MB con `otadata` en `0xD000` sobre una app de 356 KB — sobra espacio por un factor de cinco. El transporte también existe: `usb_comm` entrega frames `Magic(0xAB,0xCD) + Type(1B) + Length(4B LE) + Payload + CRC16(2B BE)` con un decoder de máquina de estados que resincroniza tras basura y descarta CRC inválidos.

Lo que falta es la mitad receptora y una decisión sobre cómo hacer llegar 356 KB de binario por un canal cuyo tipo de payload de entrada, hoy, es JSON de 256 bytes como máximo.

Restricciones que no se negocian:

- El buffer RX de CDC son 4096 bytes (`CONFIG_TINYUSB_CDC_RX_BUFSIZE`).
- El PID USB (0x4001) lo deriva TinyUSB de las clases habilitadas y la motherboard abre el dispositivo por él. Habilitar MSC lo movería a 0x4003 y el enlace desaparecería sin ningún error de compilación.
- Los SHT40 de esta placa son la variable del PID de aire de la motherboard. Reiniciar para estrenar imagen levanta `ALARM_AIR_SENSOR_FAULT`.
- `sensorBoard_host_watch` reinicia la placa cuando concluye que ha perdido el host.

## Goals / Non-Goals

**Goals:**

- Que una imagen empujada por la motherboard acabe arrancando, o que la placa vuelva sola a la anterior.
- Que una transferencia rota, un corte de corriente o una imagen que arranca pero no habla dejen siempre una placa que funciona.
- Que la motherboard pueda saber en qué estado está el receptor sin llevar un estado espejo.
- No tocar el framing, el CRC ni el descriptor USB.

**Non-Goals:**

- Firma criptográfica de la imagen y secure boot.
- Actualizar el bootloader o la tabla de particiones por esta vía (no viajan en un OTA; cambiarlos seguirá exigiendo cable).
- Cualquier decisión sobre *cuándo* actualizar: eso es la puerta de mantenimiento de la motherboard.

## Decisions

### D1 — Un tipo de frame binario nuevo (`0x02`), no base64 sobre JSON

`SB_PROTO_TYPE_OTA = 0x02`, con el primer byte del payload como subopcode y el resto como datos del subopcode.

Esto rompe la regla del proyecto de que `usb_comm` no se toca después de la fase 1, y conviene decir por qué en vez de disimularlo: esa regla se escribió asumiendo que las fases nuevas solo necesitan **emitir** (un sensor produce, `send_json`/`send_binary` transportan). Una imagen de firmware es el primer dato que tiene que **entrar** como binario, y el despacho de entrada por tipo vive necesariamente en el decoder.

*Alternativa descartada:* base64 dentro de `SB_PROTO_TYPE_JSON`. No tocaría `usb_comm`, pero el tope de 256 B por payload JSON convierte 356 KB en ~1.400 frames más el 33 % de la codificación, y obligaría a meter datos binarios por un parser cJSON. El coste de mantenimiento de una constante nueva es menor que el de esa deuda.

*Alternativa descartada:* reutilizar `SB_PROTO_TYPE_JPEG` (0x01) con un subheader. Ahorra una constante y confunde dos flujos sin relación en el mismo tipo, justo en el camino que escribe flash.

### D2 — Chunks de 4 KB, alineados a página

El tamaño de chunk se fija al buffer RX de CDC (4096 B), que además es el tamaño de borrado de página de flash. 356 KB son 87 chunks: la transferencia entera dura menos de dos segundos y cada `esp_ota_write()` cae alineado, sin escrituras parciales de página.

El tope de `SB_PROTO_MAX_BINARY_PAYLOAD` (128 KB) no se toca — es el límite del decoder para el camino de la cámara y no tiene por qué gobernar este.

### D3 — Un chunk pendiente, ACK por número de secuencia

La motherboard envía un chunk y espera el ACK con el número de secuencia que el receptor acaba de escribir. Con 87 chunks sobre un enlace de MB/s, el coste del round-trip es irrelevante y la ganancia es que el emisor sabe siempre exactamente cuánto se ha escrito.

Un chunk fuera de secuencia se rechaza en vez de escribirse en el offset equivocado: `esp_ota_write()` es secuencial y no tiene forma de detectar que le han dado el bloque de otro sitio.

### D4 — Rollback del bootloader, no de la aplicación

`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. La imagen nueva arranca en `ESP_OTA_IMG_PENDING_VERIFY` y **solo** llama a `esp_ota_mark_app_valid_cancel_rollback()` cuando llega el frame de confirmación de la motherboard, que esta emite tras ver el heartbeat de la imagen nueva. Sin confirmación, el bootloader revierte al reiniciar.

Esto es deliberadamente más estricto que "he arrancado, luego estoy bien": el fallo que importa aquí no es que la app no arranque, es que arranque y no publique lecturas — porque entonces la motherboard se queda sin la variable de su PID. Autoconfirmarse al arrancar dejaría pasar exactamente ese caso.

*Consecuencia a aceptar:* si la motherboard está caída por otro motivo cuando el SensorBoard estrena imagen, la imagen nueva se revierte aunque fuese buena. Es el lado seguro — la placa vuelve a algo que sabemos que hablaba con esa motherboard — y el coste es reintentar la actualización.

### D5 — El vigilante de host calla durante la transferencia

`sensorBoard_host_watch` reinicia la placa cuando decide que ha perdido el host. Durante una transferencia OTA hay ráfagas largas sin tráfico normal y el reinicio caería con el slot inactivo a medio escribir. La política se suspende mientras la transferencia está activa y se rearma al terminar, con o sin éxito, y con un tope de tiempo absoluto para que una transferencia colgada no desactive el vigilante para siempre.

### D6 — `ota_state` en `status`, y ningún estado espejo en la motherboard

La resp de `status` gana `ota_state` (`idle`, `receiving`, `pending_verify`, `error`). Que la motherboard pregunte en vez de deducir evita el modo de fallo clásico de estos protocolos: dos máquinas de estados que se creen sincronizadas y no lo están tras un reinicio de una de ellas.

## Risks / Trade-offs

| Riesgo | Mitigación |
|---|---|
| Tocar `usb_comm` rompe el transporte del que depende todo lo demás | El cambio se acota al despacho de entrada por tipo; magic, cabecera, CRC y longitudes no se tocan, y los escenarios existentes de `usb-transport` siguen valiendo tal cual |
| Habilitar una clase USB movería el PID y rompería el enlace en silencio | Este cambio añade un tipo de frame, no una clase; `CONFIG_TINYUSB_CDC_COUNT=1` y los flags de descriptor quedan intactos |
| `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` descartado en silencio por `confgen` | Verificar el símbolo en el `sdkconfig` generado tras borrar el anterior, como tarea explícita — es exactamente la trampa que costó el core dump |
| Un reinicio del vigilante de host a mitad de escritura | D5; y aun así el slot que se escribe es el inactivo, así que el peor caso es una transferencia perdida, nunca una placa sin arrancar |
| La imagen nueva arranca y no publica lecturas | D4: sin confirmación de la motherboard el bootloader revierte |
| `ALARM_AIR_SENSOR_FAULT` durante el reinicio | Es correcto que salte. La seguridad la da la puerta de mantenimiento de la motherboard (sin bebé, sin control térmico), no silenciar la alarma |
| Corte de corriente a media escritura | El slot inactivo queda inconsistente y `otadata` sigue apuntando al anterior; la placa arranca la imagen vieja y la transferencia se repite desde el chunk 0 |

## Migration Plan

1. Añadir el tipo de frame y sus tests en rojo; después el componente.
2. Flashear por cable una vez con `ota_data_initial.bin` en `0xD000` (el `flasher_tool` ya lo escribe) para dejar `otadata` en un estado conocido.
3. Verificar en banco con la motherboard: transferencia completa, transferencia interrumpida, imagen deliberadamente rota para ejercitar la reversión del bootloader.
4. Solo entonces la motherboard puede fiarse de esta ruta.

No hay unidades desplegadas, así que la siembra del receptor no tiene coste de flota — es la razón para meter este código pronto aunque el orquestador llegue después.

## Open Questions

1. **¿Cuánto dura la ventana de pending-verify?** Depende de cada cuánto emite heartbeat la imagen nueva (30 s hoy, más un heartbeat inmediato al volver el host) y de la ventana que use el orquestador. Hay que fijar un número, no dejarlo implícito en dos sitios.
2. **¿El receptor debe rechazar una imagen cuya versión sea igual a la que corre?** Reescribir la misma versión es inofensivo y útil para recuperar una imagen corrupta; rechazarla evita transferencias inútiles. Se inclina a permitirlo y que decida la motherboard.
3. **`storage` (~11,9 MB) está sin montar.** Si algún día se usa como área de staging, el receptor podría verificar el digest antes de tocar el slot inactivo. Hoy no aporta: la transferencia dura dos segundos y el slot inactivo no arranca hasta el commit.
