# ADR-0001: El contacto con soporte sale por ThingsBoard y por QR `mailto:`, no por SMTP

- **Estado**: aceptado
- **Fecha**: 2026-09-03
- **Placas afectadas**: Display_HMI
- **Cambio OpenSpec**: `openspec/changes/hmi-boton-ayuda`

## Contexto

El botón de ayuda del heading (spec `hmi-help-center`) incluye un formulario
que debe hacer llegar a `support@medicalopenworld.org` un mensaje del
operador con el número de serie en el asunto y un informe de depuración en
el cuerpo. Hay que elegir cómo sale ese correo de una pantalla que:

- Solo tiene un canal saliente: MQTT en claro hacia ThingsBoard
  (`Display_HMI/src/tasks/Wifi_OTA.cpp:39-42`, SDK v0.13.0, puerto 1883,
  `MAX_MESSAGE_SIZE 1024`). No hay `WiFiClientSecure`, no hay cliente HTTP
  real (`include/ArduinoHttpClient.h` es un stub vacío que existe solo para
  satisfacer al SDK, `platformio.ini:46` lo ignora) y no hay librería SMTP.
- Se despliega en hospitales donde la WiFi falta o cae durante horas
  (`Wifi_OTA.cpp` documenta `NO_AP_FOUND` "50+ min seguidos" en campo).
- Corre LVGL bajo un mutex recursivo; la regla ARQ-LOCK-001 prohíbe I/O de
  red dentro de un callback de LVGL.
- Guarda sus secretos en `Credentials.h`, no versionado y protegido por hook
  contra edición del agente.

## Opciones consideradas

| Opción | A favor | En contra |
|---|---|---|
| SMTP directo desde el ESP32 (ESP-Mail-Client o similar) | Correo "de verdad" desde el equipo | Librería nueva con TLS (heap y flash en un firmware al 78 %); **credenciales de un buzón dentro del firmware**, con un solo buzón compartido por toda la flota; depende del servidor de correo aceptar clientes embebidos; no funciona sin red |
| HTTP POST a un backend propio | Sin secretos de buzón en el equipo | No existe ni el backend ni el cliente HTTP; hace falta TLS igualmente; no funciona sin red |
| Telemetría ThingsBoard + regla *send email* en el servidor | Canal ya conectado, probado y con reconexión; cero librerías; cero secretos nuevos; la petición queda además registrada en el dispositivo de TB, con lo que se puede consultar aunque el correo falle | Hace falta configurar una regla en el servidor; no funciona sin red |
| QR `mailto:` para el móvil del operador | Funciona **sin red**; cero librerías (`LV_USE_QRCODE` ya activo); el correo sale de la cuenta real de quien lo envía, con lo que soporte puede responderle | Depende de que haya un móvil con correo a mano; el cuerpo debe ser compacto para que el QR se lea |

## Decisión

Se implementan las dos últimas opciones, combinadas:

1. **Con servidor**: ENVIAR publica la petición como telemetría con cuatro
   claves (`support_request` = asunto, `support_message`, `support_report`,
   `support_to` = `SUPPORT_EMAIL`). La publica la tarea WiFi/OTA a partir de
   una petición pendiente que deja la UI (`support_report.cpp`); el callback
   de LVGL no toca la red. Una regla de ThingsBoard reenvía por correo a
   `support_to`.
2. **Siempre**: la vista de resultado muestra un QR `mailto:` con
   destinatario, asunto y cuerpo (mensaje + informe) ya rellenos, degradando
   el contenido si no cabe.

Un único formateador produce el informe para las dos vías, así soporte ve lo
mismo llegue por donde llegue. Sin servidor no se encola nada: una petición
que saliera horas más tarde ya no contaría lo que el operador quiso contar.

Se descarta SMTP por los secretos en firmware y el coste de TLS; se descarta
HTTP por no existir ni backend ni cliente.

## Consecuencias

- El firmware no gana dependencias ni secretos; el cambio queda contenido en
  Display_HMI.
- La vía sin red es de primera clase, no un plan B: es la que va a usarse
  más en campo.
- **Deuda explícita**: la regla de ThingsBoard (filtro por `support_request`
  → *to email* con `support_to` → *send email*) hay que crearla en
  `mon.medicalopenworld.org`. Hasta entonces las peticiones quedan
  registradas en el dispositivo de TB y la pantalla dice "registrada", no
  "correo enviado". Documentada en `docs/thingsboard_dashboards.md`.
- El destinatario viaja en cada petición (`support_to`) para que la regla no
  tenga que conocerlo: si cambia el buzón, cambia en `Credentials.h` y nada
  más.
- **Revisar este ADR si**: el HMI gana un transporte TLS por otro motivo
  (OTA firmada, HTTPS), o si la motherBoard expone GPRS a la pantalla como
  canal de datos; en ambos casos `supportRequestService()` (`Wifi_OTA.cpp`)
  es el único punto a duplicar: el módulo `support_report.cpp` no conoce el
  transporte.
