# Matriz de transporte — GPRS vs WiFi

Qué se manda, por dónde y cada cuánto.

**La fuente de verdad es código**, no este documento:
`motherBoard/include/config/transport_policy.h`. Ahí se toca; esto lo explica.

Para comprobar que el código y la tabla siguen de acuerdo:

```
cd Firmware/motherBoard && python tools/check_transport_matrix.py
```

---

## 1. Periodos de transmisión

| Qué | GPRS | WiFi | Dónde se toca |
|---|---|---|---|
| Telemetría, actuando | **60 s** | **5 s** | `TX_GPRS_PERIOD_ACTUATING_S` / `TX_WIFI_PUBLISH_MS` |
| Telemetría, solo fototerapia | **180 s** | 5 s | `TX_GPRS_PERIOD_PHOTOTHERAPY_S` |
| Telemetría, en reposo | **3600 s** | 5 s | `TX_GPRS_PERIOD_STANDBY_S` |
| Comprobación de OTA | **10 min** | **1 min** | `TX_GPRS_OTA_CHECK_MS` / `TX_WIFI_OTA_CHECK_MS` |
| Reconexión de red | 10 s | 30 s | `TX_GPRS_RECONNECT_MS` / `TX_WIFI_RECONNECT_MS` |
| Reconexión a ThingsBoard | 30 s | 30 s | `TX_THINGSBOARD_RECONNECT_MS` |
| Triangulación por torre | 30 min | — | `GPRS_TRIANGULATION_INTERVAL` |
| Captura PPG automática | — | 15 min | `PPG_SNAPSHOT_AUTO_INTERVAL_MS` |

**Por qué GPRS es variable y WiFi no.** Por WiFi la red es gratis y no bloquea,
así que cadencia fija de 5 s. Por GPRS cada publicación son datos de pago y una
ráfaga AT que bloquea el módem, así que el periodo se adapta a lo que esté
haciendo la incubadora: 1 min actuando, 3 min en fototerapia, 1 h en reposo.
Los tres son **valores por defecto**: `/config` los sobrescribe en NVS.

**Consecuencia práctica**: un evento de paciente (peso, canguro, alta) puede
tardar hasta una hora en subir si la incubadora está en reposo y solo tiene
GPRS. No se pierde — la cola reintenta y cada evento lleva su propio `ts` —
pero no aparece en el dashboard al instante.

---

## 2. Telemetrías, por grupos

87 claves en total. Se agrupan así:

| Grupo | Claves | GPRS | WiFi | Motivo |
|---|---|:--:|:--:|---|
| **CORE** | 69 | ✅ | ✅ | Constantes vitales, alarmas, actuadores, datos de bebé. Lo clínico. |
| **CELLULAR** | 4 | ✅ | ❌ | `IMEI`, `APN`, `COP`, `CSQ`. Sin sentido físico por WiFi. |
| **DIAG** | 8 | ✅ | ❌ | Boot count, heap, uptime, resets del HMI, kills del módem. |
| **CALIBRATION** | 6 | ✅ | ❌ | Referencias y ajuste fino de los sensores de temperatura. |

Se cambian con un `0`/`1` en `TX_GROUP_<grupo>_<TRANSPORTE>`.

### Cuál de estas divergencias es intencionada y cuál no

**CELLULAR es correcta.** Por WiFi esos campos irían vacíos o rancios.

**DIAG y CALIBRATION no se decidieron.** Los dos bloques de montaje de
telemetría (`addConfigTelemetriesToGPRSJSON` en `GPRS.cpp` y su gemela en
`Wifi_OTA.cpp`) están escritos **dos veces a mano**, y alguien añadió 14 claves
solo a la copia de GPRS. Nada en ellas es específico del transporte: un equipo
conectado por WiFi simplemente no reporta diagnóstico ni calibración.

Están a `0` porque es **lo que hace el firmware hoy**, no porque sea lo
correcto. El código del lado WiFi ya está escrito y compilado en verde con los
flags a `1`; solo hay que decidirlo.

### Antes de encender un grupo: el presupuesto

`GPRS_JSON` y `WIFI_JSON` son `StaticJsonDocument<JSON_OBJECT_SIZE(64)>`
(`THINGSBOARD_FIELDS_AMOUNT` en `main.h`), pero el máximo posible por transporte
son **87 campos**. Al llenarse, **ArduinoJson descarta campos en silencio** — no
hay ninguna llamada a `overflowed()` en el código.

Solo desborda si una publicación concreta pasa de 64 campos, y muchas claves son
condicionales, así que puede que hoy no ocurra nunca. **No está verificado en
hardware.** Encender un grupo acerca el margen, así que conviene resolverlo
antes: subir `THINGSBOARD_FIELDS_AMOUNT` (≈16 bytes de RAM por campo y
documento) o registrar `overflowed()` para saber si está pasando.

---

## 3. Funcionalidades

| Funcionalidad | GPRS | WiFi | Nota |
|---|:--:|:--:|---|
| OTA | ✅ | ✅ | `GPRSCheckOTA()` / `WIFICheckOTA()` |
| Provisioning en ThingsBoard | ✅ | ✅ | |
| Eventos de paciente (cola) | ✅ | ✅ | Peso, canguro, alta |
| Sincronización de hora | ✅ | ✅ | NITZ + `AT+CNTP` / SNTP |
| Triangulación por torre | ✅ | ❌ | No existe equivalente por WiFi |
| Snapshot PPG (RPC `capturePPG`) | ✅ | ✅ | Bajo demanda, ~23 KB por captura |
| Snapshot PPG, captura automática | ❌ | ✅ | Cada 15 min ≈ 2,2 MB/día: por GPRS va apagado |

---

## 4. RPC — la tabla que más sorprende

**Las listas de RPC de los dos transportes son independientes.** Un RPC solo
responde por el transporte donde está registrado; por el otro, ThingsBoard
espera y muestra *"el equipo no respondió"* aunque el equipo aparezca conectado.

| RPC | GPRS | WiFi |
|---|:--:|:--:|
| `restart` | ✅ | ❌ |
| `getDiag` | ✅ | ❌ |
| `wipeBabies` | ✅ | ❌ |
| `setWifi` | ✅ | ✅ |
| `checkOta` | ✅ | ✅ |
| `capturePPG` | ✅ | ✅ |

`checkOta` estaba solo en GPRS y por eso el botón del dashboard fallaba en
equipos por WiFi. Ya está en los dos.

**Quedan tres huecos.** `restart`, `getDiag` y `wipeBabies` no responden por
WiFi. Significa que **un RPC probado en el banco por WiFi puede no responder en
campo por GPRS**, y al revés — justo el escenario en el que uno querría
reiniciar un equipo en remoto.

### Por qué el snapshot PPG no se veía en ThingsBoard

No era el dashboard ni el envío: **toda la funcionalidad vivía dentro de
`WIFI_TB_OTA()`**, que empieza con `if (WiFi.status() == WL_CONNECTED)`. La
captura automática, la publicación y el RPC `capturePPG` estaban los tres ahí
dentro. En un equipo conectado por GPRS no pasaba nada de PPG: ni siquiera se
capturaba. El módulo de captura funcionaba bien; nadie le pedía nunca nada.

Ahora la publicación es común (`PpgSnapshotPublish.cpp`) y el RPC está en los
dos transportes.

### Y por qué el botón "Capturar PPG" no hacía nada (fallo distinto)

El botón era un `system.cards.html_card` con un `<script>` inline. **TB no
ejecuta scripts inline** en el HTML de una tarjeta: se sanea al insertarlo. Así
que `capturePpg()` nunca llegaba a existir y el `onclick` moría con un
`ReferenceError` en la consola del navegador, sin ninguna señal en la página.
Y aunque el script se hubiese ejecutado, dentro de un `<script>` suelto `self`
es `window`, no el contexto del widget: `self.ctx.controlApi` era `undefined`.

**Solo los widgets de tipo `rpc` pueden lanzar RPC.** `sendCommand()` comprueba
`rpcEnabled`, que se activa únicamente cuando la suscripción ha resuelto un
`targetDevice`; en una tabla o una gráfica falla con
*"rpc.error.target-device-is-not-set"* antes de salir del navegador.

Ahora los dos botones (`capturePPG` y `checkOta`) son widgets
`system.command_button`, de tipo `rpc`, con `targetDevice` apuntando a
`state_alias`.

### El caso de las credenciales WiFi: RPC con parámetros

La tarjeta de credenciales tenía el mismo `<script>` inline muerto, pero **no
se podía convertir en `command_button`**: ese widget manda un método fijo sin
parámetros de entrada, y aquí hacen falta el SSID y la contraseña que teclea
el usuario.

Solución: el HTML se queda (se renderiza bien; TB solo descarta el `<script>`)
y el disparo pasa a un `headerButton` con acción `custom`, que es donde el JS
sí se ejecuta y recibe `widgetContext`. Como el widget es de tipo `static` y
tampoco resuelve `targetDevice`, el RPC va **por REST** en vez de por
`controlApi`:

```
POST /api/plugins/rpc/twoway/<deviceId>
{"method":"setWifi","params":{"ssid":...,"password":...},"timeout":10000}
```

El `deviceId` sale de `entityId` y, si no está, de
`widgetContext.stateController.getStateParams().entityId`. Si la instancia solo
expone la ruta v2, reintenta contra `/api/rpc/twoway/<deviceId>`.

**Funciona igual por GPRS que por WiFi**: el RPC lo entrega ThingsBoard por la
sesión MQTT que tenga el equipo, sea cual sea. En firmware las dos
implementaciones ya eran equivalentes (misma validación, mismo
`applyWifiCredentials`); lo único que se añadió es una respuesta
`{"status":"ok"|"invalid"}` en ambas, para que el dashboard pueda distinguir
"aplicado" de "rechazado" en vez de no decir nada.

Ni el SSID ni la contraseña se escriben en el log, en ninguno de los dos
transportes.

**Dos detalles que también hacen parecer que no funciona:**

- **La primera captura automática tarda 15 minutos.** `lastPpgSnapshotAttempt`
  arranca en `0`, así que `millis() - 0 > 15 min` no se cumple hasta que el
  equipo lleva 15 minutos encendido. En una prueba corta no verás nada aunque
  todo esté bien. Usa el RPC `capturePPG` para no esperar.
- **La gráfica tenía una ventana de 30 segundos.** Un snapshot son 8 s de onda
  publicados cada 15 min: se salía de la ventana casi al instante, así que
  había que estar mirando en el momento exacto. Subida a 30 min.
- **La captura solo arranca con sonda puesta y señal válida**
  (`probe_state == PROBE_APPLIED && rsqi == 1`). Si el gate no pasa, el RPC
  responde `signal_not_ready` — mira la respuesta del RPC, que lo dice.

### Buffer compartido entre dos tareas

`GPRS_Task` y `OTA_WIFI_Task` son tareas FreeRTOS distintas y comparten un
único buffer de snapshot. Con la publicación en ambas, las dos podrían
publicar el mismo snapshot, o una limpiarlo mientras la otra lo serializa.

Por eso `ppgSnapshotTryAcquire()` / `ppgSnapshotRelease()` reclaman el snapshot
en exclusiva bajo un `portMUX`, y `ppgSnapshotRequestCapture()` devuelve `BUSY`
mientras hay uno reclamado, para no sobrescribir muestras que se están
enviando.

Se registran en `rpc_callbacks[]` (`GPRS.cpp`) y `wifi_rpc_callbacks[]`
(`Wifi_OTA.cpp`).
