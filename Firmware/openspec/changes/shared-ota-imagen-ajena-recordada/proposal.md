## Why

Placas afectadas: **motherBoard** y **Display_HMI** (las dos corren el mismo
`FwGuardedUpdater` sobre la OTA de ThingsBoard). `shared/` cambia porque ahí vive
el updater. No hay nada de esto en `Firmware/docs/known_issues.md` todavía.

La guarda de placa funciona: si ThingsBoard ofrece a un equipo un binario de la
otra placa, el updater lo rechaza y `otadata` se queda intacto. Eso es correcto y
es lo que evitó repetir el incidente del 2026-09-08, en el que un Display HMI
acabó con firmware de motherBoard dentro.

El problema es lo que pasa **después** del rechazo. La marca de placa
(`IncuNestFW:<placa>`) va dentro del binario y no se puede conocer hasta haber
visto el flujo entero, así que `FwGuardedUpdater::write()` escribe la imagen en
la partición OTA **según llega** y solo al final, en `end()`, comprueba
`fw_image_is_foreign()` y llama a `inner_.reset()` para abortar
(`Firmware/shared/include/fw_guarded_updater.h:33-49`).

Abortar deja `otadata` como estaba — correcto — pero también deja al equipo
**reportando la misma versión de firmware que antes**. Sesenta segundos más
tarde (`WIFI_OTA_CHECK_INTERVAL`, `Display_HMI/include/tasks/Wifi_OTA.h:53`),
`WIFICheckOTA()` vuelve a anunciarse con esa versión, ThingsBoard vuelve a ver
la misma discrepancia, vuelve a ofrecer el mismo binario, y el equipo vuelve a
descargarlo y escribirlo entero para volver a rechazarlo.

El bucle no se detiene solo. Cuesta, por cada vuelta y cada minuto:

- **~2,5 MB de borrado y escritura** de la partición OTA. Con una resistencia
  típica de ~100.000 ciclos por sector, eso son ~1.440 ciclos al día: la
  partición se agotaría en unos **70 días** de condición mantenida.
- **~2,5 MB de datos**. Por WiFi es ancho de banda; en la motherBoard, que
  también hace OTA por GPRS con SIM Onomondo, sería coste de datos real y
  continuo. Ver `[[sim-baja-automatica-y-tirada-200]]`.

Nada de esto es un fallo del firmware en sentido estricto: la guarda hace su
trabajo. Lo que falta es **memoria del rechazo**. Hoy el equipo no distingue "no
he intentado instalar esto" de "ya lo intenté y no era para mí", y por eso una
mala configuración del servidor —un fichero en el slot equivocado— se convierte
en desgaste permanente del hardware en vez de en un aviso.

Se aplaza a propósito: el bucle sólo se dispara con ThingsBoard mal configurado,
que hoy no es el caso, y el arreglo toca la ruta de actualización, que es
delicada. Se anota ahora para que no se pierda el análisis.

## What Changes

- **El updater recuerda la última imagen rechazada por ser de otra placa** y no
  vuelve a descargarla mientras el servidor siga ofreciendo la misma. La
  identidad de "la misma imagen" tiene que salir de algo que ThingsBoard dé
  ANTES de descargar —título y versión del firmware, que es lo que viaja en la
  notificación— porque si hiciera falta descargarla para reconocerla no se
  ahorraría nada.
- **El rechazo se persiste en NVS.** Un reinicio no puede ser la forma de
  reintentar: un equipo en bucle de rechazo suele estar también reiniciándose
  por otras razones, y ahí volvería a empezar. Y al revés: el rechazo tiene que
  **caducar o poder limpiarse** cuando el servidor ofrezca un título/versión
  distintos, o el equipo quedaría inmune a la siguiente actualización legítima.
- **El rechazo se reporta.** Hoy sólo queda en el log local (`OTA rechazada: el
  binario es de otra placa`). Tiene que llegar a quien configuró el servidor,
  porque es la única persona que puede arreglarlo: como atributo o telemetría
  hacia ThingsBoard, y visible en el equipo.
- **No cambia la guarda.** Las dos barreras —cabecera declarada y marca dentro
  del binario— se quedan exactamente como están, con sus cinco pruebas de
  `tools/bench_tests/board_guard_test.py` en verde.

## Capabilities

### New Capabilities
- `ota-foreign-image-backoff`: qué recuerda el equipo de una imagen rechazada
  por pertenecer a otra placa, cuánto dura ese recuerdo, qué lo invalida, cómo
  se reporta, y la invariante de que el recuerdo **nunca** puede impedir instalar
  una imagen legítima distinta.

## Impact

- `Firmware/shared/include/fw_guarded_updater.h` — el updater gana el estado del
  rechazo. Es código compartido por las dos placas: cualquier cambio en su
  interfaz las toca a las dos.
- `Firmware/Display_HMI/src/tasks/Wifi_OTA.cpp` y
  `Firmware/motherBoard/src/tasks/Wifi_OTA.cpp` — el punto que decide si llamar
  a `Start_Firmware_Update()`.
- NVS: una clave nueva por placa. Recordar que `putFloat`/`putDouble` guardan
  BLOB por compatibilidad con Arduino; una cadena de versión no tiene ese
  problema, pero el namespace es el mismo y conviene no crecerlo sin mirar el
  tamaño de la partición (20 KB en el HMI).
- **Relación con `shared-cascade-ota-distribution`**: si la OTA en cascada se
  implementa antes, el HMI deja de ser un dispositivo de ThingsBoard y este
  bucle desaparece *para el HMI* — pero no para la motherBoard, que seguiría
  siendo el único dispositivo en la nube y la que descarga. El problema
  sobrevive a esa otra decisión; no lo cubre.
