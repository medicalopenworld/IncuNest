# ADR-0001: El contacto con soporte es un QR `mailto:` para el móvil del operador, no SMTP ni ThingsBoard

- **Estado**: aceptado (revisado 2026-09-04: se retira la vía ThingsBoard, ver
  "Decisión")
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

**Solo el QR `mailto:`.** La vista de contacto muestra un QR con destinatario
`SUPPORT_EMAIL`, asunto `IncuNest SN <serie> - Solicitud de soporte` y el
informe de depuración en el cuerpo; el operador lo escanea, escribe su
consulta encima del informe y lo envía desde su propia cuenta. El equipo no
envía nada por red.

La primera versión (2026-09-03) combinaba esto con la telemetría ThingsBoard
más una regla de correo en el servidor. Se implementó, pasó revisión y se
**retiró al día siguiente por decisión de producto**: exigía una regla en el
servidor que no existía, un formulario con teclado en pantalla y un puente
UI ↔ tarea WiFi con su propia casuística (timeout, reintentos, resultado
tardío), y en campo la vía que iba a usarse era el QR de todas formas. La
simplicidad ganó al "también desde el equipo".

Se descarta SMTP por los secretos en firmware y el coste de TLS; se descarta
HTTP por no existir ni backend ni cliente; se descarta ThingsBoard por la
complejidad que añadía frente al valor que aportaba.

## Consecuencias

- El firmware no gana dependencias ni secretos; el cambio queda contenido en
  Display_HMI y `Wifi_OTA.cpp` no se toca.
- Funciona igual con o sin red: el equipo solo dibuja un QR.
- Soporte recibe el correo desde la cuenta del operador, así que puede
  responderle directamente; el número de serie del asunto cruza con
  inventario y con el dispositivo de ThingsBoard (`IncuNest-Display-<sn>`).
- **Datos en el QR**: el informe lleva IP local y RSSI, consignas y
  actuación, temperaturas de aire y piel medidas, humedad, estado de sonda,
  fototerapia, bitmasks y títulos de alarmas, arranques y motivo de reset.
  No incluye nombre del bebé ni ningún dato del perfil, ni SSID, contraseña
  o token. Queda a la vista de quien mire la pantalla mientras el QR está
  abierto, igual que el resto de la pantalla principal; la ayuda se cierra
  sola a los 3 min sin tocar.
- **Límite**: un QR de ~650 B (versión ~20) a 340 px son 3 px por módulo;
  el botón SIN INFORME lo reduce a destinatario + asunto (versión ~5) para
  móviles que no lo lean.
- **Revisar este ADR si**: se quiere trazabilidad de las peticiones en el
  servidor, o si el HMI gana un transporte TLS por otro motivo. El módulo
  `support_report.cpp` produce el informe sin conocer el transporte, así que
  añadir una vía de red después no toca el formateador.
