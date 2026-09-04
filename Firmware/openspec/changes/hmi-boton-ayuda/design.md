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

**Decisión (revisada 2026-09-04)**: **solo el QR `mailto:`**. El operador lo
escanea, se abre un correo a `SUPPORT_EMAIL` con el número de serie en el
asunto y el informe en el cuerpo, escribe su consulta encima y lo envía desde
su cuenta. El equipo no envía nada por red.

La primera versión combinaba el QR con la publicación en ThingsBoard más una
regla de correo en el servidor. Se implementó y se retiró por decisión de
producto: exigía una regla en el servidor, un formulario con teclado y un
puente UI ↔ tarea WiFi con timeout y resultado tardío, y en campo la vía del
QR era la que iba a usarse. Ver ADR-0001.

### 2. Sin formulario: la consulta se escribe en el móvil

Sin envío desde el equipo no hace falta teclado en pantalla: el cuerpo del
`mailto:` empieza con dos líneas en blanco y luego el informe como pie
técnico, para que el operador escriba su consulta en el propio correo, con
el teclado de su móvil, que es mejor que cualquier `lv_btnmatrix`.

### 3. Informe de depuración: un solo formateador

`support_report_build(char *out, size_t cap)` produce texto ASCII plano en
líneas `clave=valor`, separadas por `\n`, **≤ 400 bytes**, que
`support_report_build_mailto()` percent-codifica dentro del `body`. Está
separado del transporte a propósito: si algún día se añade una vía de red,
el formateador no cambia.

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

Presupuesto del QR: `mailto:` + destinatario (30) + asunto (~45) + informe
(≤ 400) ≈ 480 bytes antes de percent-encoding, ≈ 650 después (los `=`, `\n`
y `|` del informe se codifican a tres bytes). `lv_qrcode` usa ECC MEDIUM
fijo: ~650 B es la versión 20 (97×97 módulos, 669 B); la capacidad máxima
antes de degradar es la versión 24 (914 B). El canvas es de **340 px**
(`QR_SIZE_MAILTO`): `lv_qrcode` escala a píxeles enteros por módulo, y
340/97 = 3 px/módulo, el mínimo que lee con soltura la cámara de un móvil a
15-20 cm (con 300 px se quedaría en 2 en el peor caso). Si aun así no
cupiera (`lv_qrcode_update()` devuelve `LV_RES_INV`), se regenera sin
informe y se avisa; y el operador puede pedirlo sin informe (versión ~5)
con el botón SIN INFORME si su móvil no lee el denso. El QR de la URL del
vídeo es corto y se queda en 300 px.

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

### 7. Auto-bloqueo con tope, y cesión ante cualquier alarma

`inactivity_timer_cb` añade a la exención de `ui_ScreenAlarms` la
condición `HelpDialog_IsOpen() || HelpTour_IsOpen()`, pero la exención tiene
tope: los dos `_Poll()` cierran la ayuda si `lv_disp_get_inactive_time()`
supera `HELP_IDLE_TIMEOUT_MS` (3 min). Sin ese tope una ayuda olvidada (la
vista del QR invita a dejarla puesta mientras se busca el móvil) podría
tapar horas la pantalla principal e impedir llegar a `ui_ScreenLock`, que
es donde se pinta el banner de alarma.

Criterio de cesión: el de `TelemetryHistory::mustYield()`, no el de
`TimeDialog`. `UI_IsCriticalAlarmActive()` es una lista fija de siete ids
que deja fuera cuatro condiciones de prioridad ALTA (fallo de sensor de
aire, fallo de sonda en modo piel, salida de aire obstruida, corte de red).
La ayuda es una vista sin información de alarma propia y su overlay se traga
los toques, así que cede ante **cualquier** alarma activa
(`UI_IsAnyAlarmActive()`) y ante `Display_IsBoardLinkLost()`, devolviendo
la pantalla principal. Coste asumido: con una alarma activa no se puede abrir la ayuda;
el centro de alarmas ya muestra la acción recomendada, que es lo que hay
que leer en ese momento.

Orden z en `lv_layer_top()`: el overlay del tutorial se crea en
`HelpTour_Init()` **antes** que el banner de alarma y el icono de AUDIO
PAUSED, y nunca se sube con `lv_obj_move_foreground()`: así el banner (que
solo vuelve a primer plano cuando cambia su texto) y el símbolo de audio
pausado siguen visibles encima del recorrido.

## Risks / Trade-offs

- **Sin trazabilidad en el servidor**: las peticiones no quedan registradas
  en ThingsBoard. Asumido: el correo llega a soporte con el número de serie
  en el asunto, que es lo que hace falta para triar.
- **QR denso** → si el móvil no lo lee, el operador puede pulsar "SIN
  INFORME" para un QR con solo destinatario y asunto.
- **Hace falta un móvil con correo**: es la única vía. En el contexto de
  destino es más probable tener un móvil a mano que WiFi funcionando.
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

Servidor: sin cambios.

## Open Questions

- URL definitiva del vídeo tutorial (`SUPPORT_TUTORIAL_URL`). Se deja
  `https://medicalopenworld.org/incunest/tutorial` como valor por defecto
  y se puede fijar en `Credentials.h` sin recompilar nada más.
