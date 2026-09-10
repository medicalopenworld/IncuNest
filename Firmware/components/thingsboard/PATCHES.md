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
ahora, con la deuda escrita y acotada a dos parches.

## Los dos parches

Todo lo tocado lleva el marcador `PARCHE INCUNEST` en el codigo, para que
`grep -rn "PARCHE INCUNEST" src/` liste el delta completo frente a upstream.

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
