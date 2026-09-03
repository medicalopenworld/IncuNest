## Context

Display_HMI, LVGL 8.3 sobre ESP32-S3 (CrowPanel 7", 800×480). Restricciones
que fijan el diseño:

- **Heading sin hueco libre**: `HEADING_SLOT1_CLOCK..SLOT6_SETTINGS`
  (`ElementsCreation.cpp:845-864`). El lado derecho ya está al paso mínimo
  que permite la zona táctil del candado (`TOUCH_EXT_SMALL=40`). El lado
  izquierdo tiene margen 20 y tres huecos iguales entre reloj, conectividad y
  centro.
- **Mutex de LVGL recursivo** (`UITask.h`): los callbacks corren con el lock
  tomado; nada de red, NVS ni I2C dentro (ARQ-LOCK-001).
- **Sin transporte HTTP ni TLS**: `ArduinoHttpClient.h` es un stub vacío,
  no hay `WiFiClientSecure`, y no hay librería SMTP. El único canal saliente
  es MQTT en claro a ThingsBoard (`tb_wifi`, `Wifi_OTA.cpp:39-42`), con
  `MAX_MESSAGE_SIZE 1024`.
- **Auto-bloqueo a 20 s** (`INACTIVITY_TIMEOUT_MS`), solo exento
  `ui_ScreenAlarms`.
- **Teclado**: `lv_btnmatrix`, nunca `lv_keyboard` (el keymap de
  `lv_keyboard` es un global de fichero en LVGL 8.3; ver `TimeDialog.cpp:50`).
- **Textos ASCII** sin acentos: las fuentes Montserrat compiladas no tienen
  esos glifos (misma regla que `alarm_text.h`).
- **Idiomas**: `g_lang` (ES/EN/FR) y helper local `TXT(es,en,fr)` en cada
  diálogo.
- `Credentials.h` está en `.gitignore` y protegido por hook: los defaults
  versionados van en `Credentials_public.h`.

## Goals / Non-Goals

**Goals**

- Un solo punto de entrada visible desde la pantalla principal para las tres
  vías de ayuda.
- El contacto con soporte debe funcionar **también sin red**: es el caso
  habitual en los hospitales de destino.
- El informe de depuración debe llegar a soporte sin que el operador tenga
  que leer ni copiar nada.
- Cero librerías nuevas, cero cambios de protocolo serie, cero cambios en
  `shared/`.

**Non-Goals**

- Enviar correo SMTP desde el dispositivo (ver decisión 1).
- Ayuda contextual por pantalla (un `?` en cada vista). El tutorial guiado
  cubre la orientación general; la ayuda por pantalla queda para otro cambio.
- Botón de ayuda en `ui_ScreenLock`.
- Vídeo embebido en el display (no hay decoders ni filesystem en LVGL:
  `LV_USE_FS_* 0`, `LV_USE_PNG 0`).
- Adjuntar el coredump de la partición `coredump` al informe.

## Decisions

### 1. Transporte del contacto: telemetría ThingsBoard + QR `mailto:`, no SMTP

**Opciones consideradas**

| Opción | A favor | En contra |
|---|---|---|
| SMTP directo desde el ESP32 (p. ej. ESP-Mail-Client) | Correo "de verdad" desde el equipo | Librería nueva con TLS (heap y flash), **credenciales de un buzón dentro del firmware**, dependencia del servidor de correo, y no funciona sin red |
| HTTP POST a un backend propio | Sin secretos de buzón | No existe backend ni cliente HTTP en el HMI; TLS igualmente |
| **Telemetría ThingsBoard + regla de correo en el servidor** | Canal ya conectado y probado, cero librerías, sin secretos nuevos, la petición queda además registrada en el dispositivo de TB | Hace falta una regla en el servidor; no funciona sin red |
| **QR `mailto:` para el móvil del operador** | Funciona **sin red**, sin librerías (`lv_qrcode`), el correo sale de una cuenta real del hospital | Depende de que el operador tenga móvil con correo; el cuerpo debe ser compacto |

**Decisión**: las dos últimas, combinadas. Cuando hay conexión con el
servidor, "ENVIAR" publica la petición como telemetría y el servidor la
reenvía a `SUPPORT_EMAIL`. En todos los casos el diálogo ofrece el QR
`mailto:` como vía alternativa. El correo destino viaja también en la
telemetría (`support_to`) para que la regla del servidor no tenga que
conocerlo: la fuente de verdad del destinatario sigue siendo el firmware,
como pedía el requisito.

**Consecuencia**: la regla de ThingsBoard (nodo *to email* + *send email*
filtrando `support_request`) es un entregable del servidor, documentado en
`docs/thingsboard_dashboards.md`. Hasta que exista, la telemetría queda
almacenada en TB y la vía del móvil cubre el caso.

### 2. La publicación la hace la tarea WiFi/OTA, el diálogo solo deja una petición pendiente

Mismo patrón que `g_pendingTimeAck` pero en sentido inverso: la UI llama a
`SupportRequest_Submit(msg)`, que copia el mensaje a un buffer estático bajo
`portMUX` y marca `PENDING`. `SupportRequest_Service()` corre en
`WIFI_TB_OTA()` (tarea OTA, core 1) cuando `tb_wifi.connected()`: compone
el JSON con `support_report_build()`, llama a `tb_wifi.sendTelemetryJson()`
y deja `SENT` o `FAILED`. `HelpDialog_Poll()` lee el estado y pinta el
resultado; si no llega nada en `SUPPORT_SEND_TIMEOUT_MS` (15 s) lo da por
fallido y ofrece el QR.

Antes de enviar, la UI comprueba `WIFIIsConnectedToServer()`: si no hay
servidor no se encola nada y se va directo a la vista del QR con el motivo
("Sin conexion con el servidor"). Así nunca queda una petición huérfana que
salga horas después sin relación con lo que el operador quiso decir.

### 3. Informe de depuración: un solo formateador, dos salidas

`support_report_build(char *out, size_t cap)` produce texto ASCII plano en
líneas `clave=valor`, separadas por `\n`, **≤ 400 bytes**. Es el mismo texto
que va en `support_report` (telemetría) y en el `body` del `mailto:`
(percent-encoded por `support_report_build_mailto()`). Un único formateador
garantiza que soporte vea lo mismo por las dos vías.

Contenido, en este orden (lo que más ayuda a triar primero):

```
sn=0042 hmi=4.0.0 mb=3.2.1 hw=18A
boots=57 rst=3 up=01:23:45
wifi=1 rssi=-61 ip=192.168.1.20 tb=1
link=ok bars=4 lang=es
mode=air act=1 setA=36.5 setS=36.0 setH=60
air=36.4 skin=36.1 hum=58 probe=1
photo=0 alarms=0x0000 sil=0x0000
active=Temp. aire alta|Sonda piel
heap=123456/98765 psram=7654321
```

Presupuesto del QR: `mailto:` + destinatario (30) + asunto (~45) + cuerpo
(mensaje ≤ 160 + informe ≤ 400) ≈ 640 bytes antes de percent-encoding, ≈ 900
después. `lv_qrcode` con ECC MEDIUM lo cubre hasta la versión ~24 (113×113
módulos). Con un canvas de 360 px salen ~3 px/módulo, legible por un móvil
actual a 15-20 cm. Si el texto no cabe (`lv_qrcode_update()` devuelve
`LV_RES_INV`), se reintenta sin el mensaje libre y luego sin informe, y se
avisa en pantalla de qué se ha recortado.

Presupuesto MQTT: mismo cuerpo dentro de un JSON de 4 claves ≈ 700 bytes <
`MAX_MESSAGE_SIZE` 1024.

### 4. Asunto del correo

`IncuNest SN <%04d> - Solicitud de soporte` (texto fijo en inglés/español
neutro sin acentos). El número de serie va delante porque es la clave por
la que soporte cruza con el inventario y con el dispositivo de ThingsBoard
(`IncuNest-Display-<sn>`). Se recomendó y se adopta el SN como
identificador de asunto porque, a diferencia del nombre del hospital o del
operador, siempre está disponible en la pantalla y es único por equipo.

### 5. Tutorial guiado: overlay en `lv_layer_top()` con marco sobre el control real

Alternativas: capturas dibujadas (no hay decoders ni memoria de sobra) o
pantallas de texto. Se elige **resaltar la UI real**: el tutorial es un
overlay en `lv_layer_top()` (para sobrevivir al cambio de pantalla) con
fondo negro al 45 %, un marco ámbar (`0xFFC107`, 4 px) posicionado sobre
`lv_obj_get_coords()` del control del paso, y un bocadillo con el texto y
los botones ANTERIOR / SIGUIENTE / SALIR colocado en la mitad de pantalla
opuesta al control.

La tabla de pasos es estática: `{ lv_obj_t **target; lv_obj_t **screen;
const char *es, *en, *fr; }`. Se guarda el **puntero al global** (`&ui_X`),
no el objeto, porque los objetos se crean después de la tabla. Al entrar en
un paso, si `lv_scr_act() != *screen` se hace `lv_scr_load(*screen)` (sin
animación: los coords deben ser válidos en el mismo frame tras
`lv_obj_update_layout()`). Si `!lv_obj_is_visible(*target)` el paso se
salta en la dirección del avance. Al salir se vuelve a `ui_ScreenMain`.

El overlay intercepta todos los toques, así que durante el tutorial no se
puede accionar nada por accidente. Es un recorrido, no un modo de prueba.

### 6. Colocación del botón en el heading

Izquierda del heading: margen 20, luego `?` (44 px), reloj (180), conectividad
(40) y 3 huecos iguales hasta la zona táctil del candado (400-19-40=341):

```
20 + 44 + g + 180 + g + 40 + g = 341  →  g = 19
```

`HEADING_SLOT0_HELP = 20` (LEFT_MID), `HEADING_SLOT1_CLOCK = (173 - 400)`
(CENTER), `HEADING_SLOT2_CONN = 282` (LEFT_MID). El lado derecho no se
toca. Los mismos slots valen para la réplica de `ui_ScreenLock` (sin botón).
El `?` es un `lv_btn` redondo azul `0x0075EE` (mismo azul que Bebés) con
label `?` en `montserrat_28`: sin asset nuevo.

### 7. Auto-bloqueo y alarma crítica

`inactivity_timer_cb` añade a la exención de `ui_ScreenAlarms` la
condición `HelpDialog_IsOpen() || HelpTour_IsOpen()`. Ambos `_Poll()`
cierran ante `UI_IsCriticalAlarmActive()`, igual que `TimeDialog_Poll()`:
una alarma crítica se lleva la pantalla por delante y el tutorial devuelve
la pantalla principal.

## Risks / Trade-offs

- **La regla de ThingsBoard no existe aún** → hasta entonces "ENVIAR" solo
  registra la petición en TB. Mitigación: la vista de resultado siempre
  muestra el QR `mailto:` como segunda vía, y el texto de éxito dice
  "registrada" y no "correo enviado".
- **QR denso** → si el móvil no lo lee, el operador puede pulsar "SIN
  INFORME" para un QR con solo destinatario, asunto y mensaje.
- **Redistribuir el heading** mueve reloj y conectividad ~26 px a la
  derecha. Riesgo visual, no funcional; se comprueba en banco que el `?` no
  invade la zona táctil ampliada del candado (341 px) ni del reloj.
- **Tutorial sobre la UI real** → si un paso apunta a un control que cambia
  de sitio en el futuro, el marco se mueve con él (ventaja); si el control
  se elimina, el compilador lo detecta (`&ui_X` deja de existir).
- **Coste de flash**: estimado 25-40 KB (código + textos ×3 idiomas +
  `lv_qrcode` que hasta ahora no se enlazaba). Queda margen (690 KB).

## Migration Plan

Sin migración: no hay datos persistidos nuevos, no cambia el protocolo ni
NVS. El firmware anterior y el nuevo son intercambiables por OTA.

Servidor: crear la regla de correo en ThingsBoard (documentada en
`docs/thingsboard_dashboards.md`) cuando convenga; no bloquea el despliegue.

## Open Questions

- URL definitiva del vídeo tutorial (`SUPPORT_TUTORIAL_URL`). Se deja
  `https://medicalopenworld.org/incunest/tutorial` como valor por defecto
  y se puede fijar en `Credentials.h` sin recompilar nada más.
- Si en el futuro la motherBoard ofrece GPRS a la HMI como transporte,
  `SupportRequest_Service()` es el único punto que habría que duplicar.
