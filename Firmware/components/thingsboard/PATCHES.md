# ThingsBoard Client SDK — copia local parcheada

**Upstream:** https://github.com/thingsboard/thingsboard-client-sdk
**Version base:** `v0.13.0` (misma que pineaba `platformio.ini`)
**Licencia:** MIT, ver `LICENSE.txt` (sin cambios)

## Por que hay una copia local en vez de la dependencia del registro

El SDK **si** esta en el registro de componentes (`thingsboard/thingsboard`) y
**si** trae su propio `CMakeLists.txt`. La copia local no existe por comodidad:
existe porque **la version 0.13.0 no compila contra ESP-IDF 6.0.1** y no hay
ninguna version publicada que lo haga.

La razon es mbedtls. ESP-IDF 6 pasa a **mbedtls 4**, y el SDK toca estructuras
internas que ahi ya no existen. Y no se puede subir de version para esquivarlo:

- **v0.15.0** (la ultima, de diciembre de 2024) no compila con *ningun*
  `IMQTT_Client`: su propio `ThingsBoard.h` llama a
  `IMQTT_Client::get_buffer_size()`, un metodo que la interfaz ya no declara
  (se partio en `get_receive_buffer_size()` / `get_send_buffer_size()`). Es un
  bug del SDK, no de nuestra eleccion de transporte. Ya estaba comprobado y
  documentado en el `platformio.ini` original.
- **v0.14.0** compila pero sigue con la API de un solo bufer, igual que
  0.13.0: no aporta nada y arrastra el mismo problema de mbedtls.
- El proyecto **no publica una version desde diciembre de 2024**.

Es decir: la dependencia esta de facto sin mantenimiento y su ultima version es
la rota. Tarde o temprano hay que hacerse cargo de ella; esta copia lo hace
ahora, con la deuda escrita y acotada a seis parches.

## Los seis parches

Todo lo tocado lleva el marcador `PARCHE INCUNEST` en el codigo, para que
`grep -rn "PARCHE INCUNEST" .` liste el delta completo frente a upstream.

### 1. `HashGenerator` deja de mirar dentro de `mbedtls_md_context_t`

`src/HashGenerator.h`, `src/HashGenerator.cpp`

`HashGenerator::free()` tiene que distinguir "contexto nunca inicializado" de
"contexto en uso", porque `mbedtls_md_free()` sobre un contexto sin
inicializar revienta y `free()` se llama desde `start()` antes del primer
`mbedtls_md_init()`. Upstream lo resolvia leyendo los campos internos
`hmac_ctx`, `md_ctx` y `md_info`.

Con mbedtls 4 eso no compila: `hmac_ctx` solo existe bajo `MBEDTLS_MD_C`, que
ESP-IDF 6 ya no activa (el camino paso a PSA), y los otros dos son privados.

El parche lleva la cuenta con una bandera propia, `m_started`. Es ademas lo
correcto de por si: es exactamente el dato que la condicion queria saber, y no
depende de la version de mbedtls ni de campos privados de nadie.

### 2. `Callback_Watchdog.h` no incluia `<cstdint>`

`src/Callback_Watchdog.h`

Declara `void once(uint64_t const &)` sin incluir `<cstdint>`. Con las
cabeceras de IDF 5 le llegaba por inclusion transitiva; con las de IDF 6 ya
no, y falla con `'uint64_t' has not been declared`. Se anade el include.

### 3. `CMakeLists.txt` declara `app_update` como dependencia

`Configuration.h` decide `THINGSBOARD_USE_ESP_PARTITION` con
`__has_include(<esp_ota_ops.h>)`. El `CMakeLists.txt` de upstream solo pide
`mqtt` y `mbedtls`, asi que dentro del propio componente esa cabecera NO esta
en el path de inclusion: `Espressif_Updater.cpp` se compila vacio. La
aplicacion, que si tiene `app_update`, ve la clase completa y al enlazar
faltan `Espressif_Updater::begin/write/end/reset` y su vtable.

Se anaden `app_update` y `esp_app_format` a `dependencies` (publicas). Es un
fallo real de upstream para cualquier proyecto ESP-IDF que use la OTA del
SDK.

### 4. Los eventos MQTT se despachan al cliente que los registro

`src/Espressif_MQTT_Client.cpp`

Upstream registra el manejador de eventos con `handler_args = nullptr` y el
manejador estatico despacha siempre contra `m_instance`, **un unico puntero
estatico** que el constructor sobrescribe. Con una sola instancia funciona;
con dos, el ultimo construido se queda los eventos de ambas.

Este firmware crea **dos**: `mqttClientGPRS` (`GPRS.cpp`) y `mqttClientWIFI`
(`Wifi_OTA.cpp`), uno por transporte. Medido en banco el 2026-09-15 con la
unidad conectada por celular: el `MQTT_EVENT_CONNECTED` del cliente de GPRS se
entregaba al objeto de WiFi, de modo que el `m_connected` del de GPRS no se
ponia nunca a `true`. ThingsBoard veia la sesion viva y el equipo marcado como
activo, mientras el firmware reintentaba conectar cada 30 s, no llegaba a
suscribir ni un RPC (el servidor respondia 409) y no pedia la OTA jamas. Los
datos entrantes -- peticiones de RPC y trozos de firmware -- tambien acababan
en el cliente equivocado.

El parche pasa `this` como `handler_args` (la API de ESP-IDF ya lo reenvia al
manejador) y el manejador estatico lo usa como instancia, cayendo a
`m_instance` solo si llega `nullptr`, para no romper a quien registre al modo
antiguo. Sin esto, **los dos transportes no pueden coexistir**.

Con la version Arduino/PlatformIO no se veia porque alli el transporte era
`Arduino_MQTT_Client` (sobre PubSubClient), que no tiene ningun estatico de
este tipo.

### 5. `Handle_Request_Timeout()` no desreferencia un callback ya desmontado

`src/OTA_Handler.h`

`Stop_Firmware_Update()` hace `m_watchdog.detach()` y despues pone
`m_fw_callback = nullptr`, pero eso no cierra la carrera: el callback del
watchdog corre en la tarea de `esp_timer` y el desmontaje en otra. Si el
temporizador ya se disparo cuando la otra tarea anula el puntero,
`Handle_Request_Timeout()` desreferencia un nulo en la primera linea.

Medido en banco el 2026-09-15 en la primera OTA por celular que llego a
arrancar: la sesion MQTT se corto a media descarga
(`transport_read(): EOF, errno=128`) y nueve segundos despues:

```
Guru Meditation Error: Core 0 panic'ed (LoadProhibited)  EXCVADDR 0x3c
Handle_Request_Timeout() -> Helper::detectSize()
```

El panico es peor que perder la actualizacion: `initGPRS()` detecta un reset
anormal y **borra la tarea GPRS de esa sesion**, de modo que el equipo se queda
sin celular hasta que alguien le quita la corriente fisicamente. Es decir, un
corte de red a media OTA deja la unidad incomunicada en campo.

El parche anade la misma guarda contra nulo que upstream ya tiene en
`Process_Firmware_Packet()`. Por WiFi no salta nunca porque la peticion de
trozo no llega a vencer; hace falta un enlace con perdidas, que es
precisamente el caso del 2G.

### 6. El watchdog de la OTA avisa al manejador que lo armo

`src/Callback_Watchdog.cpp`

Mismo defecto que el parche 4, en otra clase: `Callback_Watchdog` guarda un
unico puntero estatico `m_instance` que el constructor pisa, y el callback del
`esp_timer` despacha siempre contra el. Con dos `ThingsBoard` hay dos
`OTA_Handler` y dos watchdogs, y el temporizador de la OTA **celular** acababa
invocando `Handle_Request_Timeout()` del manejador de **WiFi**, que nunca
arranco una actualizacion y tiene `m_fw_callback` a nulo.

Consecuencia medida en banco el 2026-09-15: al recibir un trozo corrupto
(payload de 0 bytes por perdidas en el enlace) el handler de GPRS devolvia el
control y dejaba armado su watchdog para reintentar; el reintento saltaba en
el handler equivocado, imprimia `OTA update callback is NULL` y la descarga
por 2G se quedaba muda sin reintentar jamas. Antes del parche 5 ese mismo
salto era un `LoadProhibited`: el parche 5 quitaba el panico, este quita la
causa.

El parche pasa `this` como `arg` del temporizador y el callback usa ese
puntero, con `m_instance` solo como respaldo para un `arg` nulo. La rama sin
`esp_timer` (Ticker de Arduino) se deja como estaba.

Nota aparte, no corregida: el mensaje `RECEIVED_UNEXPECTED_CHUNK_SIZE` de
`OTA_Handler.h` imprime los dos numeros al reves (`printfln(FMT, esperado,
recibido)` con un formato que dice "recibido... esperado"). Leelo cruzado.

## Que NO se ha tocado

- El transporte. Se usa `Espressif_MQTT_Client` (esp-mqtt) en vez de
  `Arduino_MQTT_Client` (PubSubClient), pero eso es eleccion nuestra en el
  codigo de la aplicacion, no un cambio en el SDK. Los ficheros `Arduino_*.cpp`
  se siguen compilando y quedan vacios porque su contenido esta bajo
  `#ifdef ARDUINO`.
- La logica de protocolo, OTA, provisioning o RPC.

## Al actualizar

Si algun dia sale una version que arregle lo de `get_buffer_size()` y lo de
mbedtls 4, lo suyo es **volver a la dependencia del registro** y borrar esta
carpeta. Antes de hacerlo, comprobar que los dos parches de arriba ya no hacen
falta.

La alternativa que se descarto fue quedarse en **ESP-IDF 5.5** (que lleva
mbedtls 3, donde el SDK compila tal cual). Se descarto porque dejaria la
motherBoard y el HMI en una IDF y la SensorBoard en 6.0.1, que es justo la
fragmentacion que este porte venia a quitar.
