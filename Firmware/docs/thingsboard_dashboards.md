# ThingsBoard — Modelo de datos y dashboards de paciente

Referencia de **qué publica el firmware** y **cómo se monta el dashboard** encima.
El contrato de datos de esta página es independiente de la versión de
ThingsBoard; el JSON importable (`dashboards/`) sí es específico de la 4.2.

---

## 1. Por qué hay dos mecanismos

Los datos de paciente se dividen según su **vida útil**, no según su tema:

| | Mecanismo TB | Qué contiene | Por qué |
|---|---|---|---|
| Quién está dentro **ahora** | Atributos de cliente | Ocupante actual y sus contadores | Se sobrescriben, no generan histórico y alimentan tarjetas de estado sin ensuciar series |
| Qué **ha pasado** | Telemetría con `ts` propio | Puntos de peso, canguro, altas | Cada punto se sella con la hora real del evento, no la del publish |

Todo payload lleva **`baby_seq`**. Sin él la nube no puede distinguir un
paciente del siguiente, y los contadores acumulativos por bebé se leen como
corrupción de datos cuando entra un bebé nuevo y vuelven a cero.

---

## 2. Atributos de cliente (ocupante actual)

Publicados cuando cambia algo del perfil, y una vez al arrancar.

| Clave | Tipo | Notas |
|---|---|---|
| `baby_seq` | int | `0` = incubadora vacía |
| `baby_name` | string | Escapado para JSON |
| `baby_gest_weeks` | int | Edad gestacional |
| `baby_weight_g` | int | Último peso conocido; `0` = nunca informado (SKIP) |
| `baby_admission_epoch` | int | Unix UTC; `0` = sin hora sincronizada al ingresar |
| `baby_kangaroo_count` | int | Salidas con la madre |
| `baby_phototherapy_min` | int | Minutos acumulados **de este bebé** |
| `baby_thermo_min` | int | Minutos acumulados **de este bebé** |

Al quedar vacía la incubadora, todas las claves se publican **a cero o vacío**,
nunca se omiten: una clave omitida conserva su valor anterior en ThingsBoard y
dejaría al bebé dado de alta colgado en la tarjeta.

> **No confundir** `baby_phototherapy_min` / `baby_thermo_min` (por paciente)
> con `Phototherapy_active_time` / `Control_active_time`, que son contadores de
> vida del equipo y ya existían.

---

## 3. Eventos de telemetría

Formato `{"ts": <epoch_ms>, "values": {…}}`. Si el reloj de la placa no está
sincronizado el envoltorio `ts` se **omite** y sella el servidor, en vez de
mandar una fecha de 1970 que ensuciaría la serie.

### 3.1 Punto de peso
```json
{"ts":1786617536000,"values":{"baby_seq":7,"baby_weight_g":1460}}
```
Uno por cada peso registrado en el wizard. Es la curva de crecimiento.

### 3.2 Evento canguro
```json
{"ts":1786620000000,"values":{"baby_seq":7,"baby_kangaroo_event":1,"baby_kangaroo_count":5}}
```
`baby_kangaroo_event` vale siempre 1: sirve para contar eventos por ventana
temporal; `baby_kangaroo_count` es el acumulado en ese instante.

### 3.3 Alta (registro de fin de estancia)
```json
{"ts":1786700000000,"values":{
  "baby_seq":7,"baby_name":"ANA","baby_gest_weeks":31,"baby_weight_g":1460,
  "baby_kangaroo_count":5,"baby_phototherapy_min":90,"baby_thermo_min":300,
  "baby_admission_epoch":1786000000,"baby_discharge_epoch":1786700000,
  "baby_outcome":1,"baby_stay_days":8}}
```
**Autocontenido a propósito**: es la fila que lee la tabla de historial y la
exportación al Ministerio, así que no debe requerir cruzarse con nada más.

`baby_outcome`: `0` Desconocido · `1` Sobrevivió · `2` No sobrevivió · `3` Trasladado.

`baby_stay_days` se **omite** si las fechas no son utilizables, en lugar de
inventar una estancia de 19.000 días.

### 3.4 Entrega
Cola en anillo de 8 eventos con *peek → enviar → pop*: un evento solo se
descarta cuando el broker lo acepta, así que un fallo de publicación se
reintenta en el siguiente ciclo en vez de perder el registro.

---

## 4. Dashboards

### 4.1 "Incubadora — Paciente actual" (personal clínico)

| Zona | Widget | Fuente |
|---|---|---|
| Tarjetas superiores | Value card ×4 | Atributos: nombre, EG, peso, días de vida* |
| Tarjetas de terapia | Value card ×3 | Atributos: canguro, fototerapia, termorregulación |
| Curva de crecimiento | Time series chart | Telemetría `baby_weight_g` |
| Ambiente | Time series chart | `Air_temp`, `Skin_temp`, `Humidity` (ya existían) |
| Alarmas | Timeseries table | Claves `*_alarm` (ya existían) |

\* Los días de vida se derivan de `baby_admission_epoch`; si es `0`, mostrar “—”.

**Clave del diseño:** la curva de peso se acota con una ventana temporal que
empieza en `baby_admission_epoch`. Dentro de esa ventana la serie pertenece a
un único bebé por construcción, así que no hace falta crear una entidad por
paciente para separar curvas.

### 4.2 "Historial de pacientes" (dirección / Ministerio)

| Zona | Widget | Fuente |
|---|---|---|
| Tabla de altas | Timeseries table | Evento de alta: una fila por bebé |
| Resultados | Pie chart | `baby_outcome` agregado |
| Estancia media | Value card (avg) | `baby_stay_days` |
| Carga de terapia | Bar chart | `baby_phototherapy_min`, `baby_thermo_min` |

El truco es que un widget de tabla de series temporales pinta **una fila por
timestamp**; como cada alta se publica en un único instante con todos sus
campos, el historial sale directo sin modelar una entidad por bebé.

---

## 5. Cuándo se sube cada cosa

Importa entender que hay **dos relojes distintos**: el momento en que ocurre
el dato y el momento en que sale por la red.

| Dato | Se genera | Se sube |
|---|---|---|
| Atributos del ocupante | Al crear perfil, registrar peso, canguro, alta, o cambiar de terapia | En el siguiente ciclo de publicación |
| Punto de peso | Al confirmar el peso en el wizard | Un evento por ciclo, con el `ts` del momento en que se registró |
| Evento canguro | Al responder "con la madre" en el diálogo de idle | Ídem |
| Registro de alta | Al confirmar el alta con su motivo | Ídem |

**Ciclo de publicación**: cada **5 s** por WiFi (`WIFI_PUBLISH_INTERVAL`), o el
periodo GPRS configurado (por defecto **60 s** en actuación,
`actuating_gprs_period`, ajustable por `/config`).

Detalles que conviene tener claros:

- **Un evento por ciclo.** La cola se drena de uno en uno para que una ráfaga
  no monopolice el módem. Con 5 puntos de peso encolados y WiFi, tardan ~25 s
  en subir todos. No se pierde ninguno, solo se escalonan.
- **El retraso de subida no falsea la fecha.** Cada evento lleva el `ts` del
  instante en que ocurrió, así que aunque se publique un minuto más tarde (o
  al recuperar cobertura horas después) aparece en la gráfica en su sitio.
- **Sin hora sincronizada** el evento va sin `ts` y lo sella el servidor al
  llegar: el orden se mantiene, la hora real se pierde.
- **Si el publish falla, se reintenta.** El evento solo se descarta cuando el
  broker lo acepta.
- **La cola guarda 8 eventos.** Una desconexión larga con mucha actividad
  puede descartar los más antiguos, y queda un warning en el log.
- **Los atributos no se encolan**: reflejan el estado actual, así que si
  cambian dos veces antes de publicarse, se sube el valor final. Es lo
  correcto para tarjetas de estado.

## 6. Importar el dashboard

Fichero: **`Firmware/Thingsboard/incunest_TB_main_v5.json`**

No es un dashboard aparte: es **vuestro `incunest_TB_main_v2`** con dos páginas
nuevas (`Bebe actual`, `Historial bebes`) y la navegación rehecha.

### Navegación todos-con-todos
Cada estado lleva el juego completo de destinos como `headerButton` en su
widget de arriba a la izquierda, siempre en el mismo orden. Se generó de forma
canónica, lo que de paso corrigió tres defectos que había en el v2:

- `device_telemetries` y `device_connectivity` tenían **dos** tarjetas de
  navegación cada una, con juegos distintos y botones duplicados.
- `display`, `device_vitals` y `device_ota` no tenían navegación: eran
  callejones sin salida.
- El botón `Telemeteries` estaba mal escrito.

`setEntityId` va activado en todos los destinos salvo el de vuelta a la
portada, que gestiona la selección por sí misma.

> El estado `display` estaba **vacío** (cero widgets): abría una página en
> blanco. Le puse una tarjeta mínima con versión de FW e idioma para que
> alojase la navegación. **Decide tú qué debe mostrar realmente** — no me
> inventé contenido clínico.

En TB: **Dashboards → + → Import dashboard**. Al compartir título con el
existente, conviene importarlo y comprobarlo antes de retirar el v2.

Se construyó **clonando widgets reales de vuestro propio export** en vez de
escribirlos a mano, así que hereda exactamente:

- `system.charts.basic_timeseries` y `system.cards.attributes_card`, los
  `typeFullFqn` que ya usáis (no los que yo había supuesto).
- El alias `state_alias` (`stateEntity`), de modo que las páginas nuevas se
  vinculan al dispositivo que pasa la fila de la tabla, como las demás.
- La acción `openDashboardState` con `setEntityId`, clonada de un botón real.

## 7. Limitaciones conocidas

- `Skin_CAP` (capacitancia cruda de la sonda) ya **no se publica**: era un
  diagnóstico de puesta en marcha, no un dato clínico. El estado de la sonda
  sigue viajando en `CTRL,TEL` hacia el HMI.

- La curva de peso **histórica** de un bebé archivado no se retro-sube: solo
  llegan a la nube los puntos publicados mientras estuvo activo. El histórico
  completo sigue en LittleFS y se consulta desde la pantalla del HMI.
- Sin hora sincronizada (sin WiFi ni cobertura GPRS), los eventos llegan con
  la marca del servidor, así que el orden se mantiene pero la fecha real del
  evento se pierde.
- Un bebé dado de alta mientras el equipo está sin red se reintenta desde la
  cola, pero la cola solo guarda 8 eventos: una desconexión muy larga con
  mucha actividad puede descartar los más antiguos (queda un warning en el log).

---

## 8. OTA desde el dashboard

En ThingsBoard una OTA **se lanza asignando un paquete al dispositivo** (ficha
del equipo o perfil), no desde un widget: al asignarlo, TB empuja los atributos
compartidos `fw_title`/`fw_version`/`fw_checksum` y el equipo los descarga.
El firmware ya lo soporta (`Start_Firmware_Update` por WiFi y por GPRS).

La página `OTA` tenía solo una tabla de lectura. Ahora lleva:

| Elemento | Qué hace |
|---|---|
| Botón de fila **Ficha del equipo** | Acción `custom`: navega a `devices/<id>`, donde se asigna el paquete |
| Botón de fila **Comprobar OTA ahora** | RPC `checkOta`: fuerza la comprobación en el momento |
| Columna **Progreso OTA %** | Telemetría `ota_progress`, 0-100 durante la descarga |

**Por qué el RPC importa**: el equipo comprueba si hay firmware nuevo cada
minuto por WiFi pero **cada 10 minutos por GPRS**. Sin él, tras asignar el
paquete habría que esperar. El RPC llama a la `GPRSCheckOTA()` que ya existía
(con su gestión de `currentFWSent`), no a una copia.

`ota_progress` **solo se publica mientras hay una descarga en curso** y se
limpia al terminar, con éxito o sin él: así la tabla no se queda con un 87%
fantasma entre actualizaciones.

### Dos trampas encontradas al probarlo contra el equipo real

**`openEntityDetails` no existe.** No es un miembro de `WidgetActionType`; los
válidos son `doNothing`, `openDashboardState`, `updateDashboardState`,
`openDashboard`, `custom`, `customPretty`, `mobileAction`, `openURL` y
`placeMapItem`. ThingsBoard **ignora en silencio** una acción de tipo
desconocido: el botón se dibuja y al pulsarlo no pasa nada, sin error en
consola. Ahora es una acción `custom` que navega a `devices/<id>`; esa ruta
redirige a `/entities/devices/<id>`, así que vale en TB antiguo y nuevo.

**El RPC tiene que existir en los dos transportes.** `checkOta` estaba
registrado solo en `rpc_callbacks` (GPRS). Las listas de RPC de GPRS y WiFi son
independientes, así que por WiFi el RPC no existía y el dashboard respondía
"el equipo no respondió" aunque apareciese conectado. Ahora está en las dos, y
cada una llama a la comprobación de su propio transporte (`GPRSCheckOTA()` /
`WIFICheckOTA()`), porque cada cliente ThingsBoard es un objeto distinto.

> Las dos listas siguen **asimétricas** por lo demás: `restart`, `getDiag` y
> `wipeBabies` solo existen por GPRS, y `capturePPG` solo por WiFi. Es anterior
> a este cambio, pero conviene tenerlo presente: un RPC probado en el banco por
> WiFi puede no responder en campo por GPRS, y al revés.
> El reparto completo está en [`transport_matrix.md`](transport_matrix.md).

### "FW state: Not synced" sin OTA asignada es normal

TB compara el firmware que reporta el equipo contra el paquete asignado al
dispositivo o a su perfil. Sin paquete asignado no hay nada con lo que
sincronizar, así que la columna se queda en *Not synced*. Deja de estarlo al
asignar un paquete y completarse la descarga.

---

## 9. Peticiones de soporte

Botón de ayuda del HMI (`docs/hmi.md`, §6; spec `hmi-help-center`,
`Display_HMI/src/ui/HelpDialog.cpp`). Cuando el operador pulsa ENVIAR con
conexión al servidor, la tarea WiFi/OTA (`supportRequestService()`,
`Display_HMI/src/tasks/Wifi_OTA.cpp`) publica **cuatro claves de texto**
como telemetría normal (mismo mecanismo que el resto de esta página, sin
`ts` propio: las sella el servidor):

| Clave | Contenido |
|---|---|
| `support_request` | Asunto: `IncuNest SN <serie a 4 cifras> - Solicitud de soporte` |
| `support_message` | Mensaje libre del operador (≤ 160 caracteres ASCII, puede venir vacío) |
| `support_report` | Informe de depuración ASCII (≤ 400 B), líneas `clave=valor` separadas por `\n` |
| `support_to` | Destinatario (`SUPPORT_EMAIL`, por defecto `support@medicalopenworld.org`) |

Ejemplo de payload (formato real de `support_report_build()`, un envío sin
mensaje libre):

```json
{
  "support_request": "IncuNest SN 0042 - Solicitud de soporte",
  "support_message": "",
  "support_report": "sn=0042 hmi=4.0.0 mb=3.2.1 hw=18A\nboots=57 rst=sw up=1h23m\nwifi=1 rssi=-61 ip=192.168.1.20 tb=1\nlink=ok bars=4 srv=1 lang=es\nmode=air act=1 setA=36.5 setS=36.0 setH=60\nair=36.4 skin=36.1 hum=58 probe=1\nphoto=0 alarms=0x0000 sil=0x0000\nactive=none\nheap=123456/98765 psram=7654321",
  "support_to": "support@medicalopenworld.org"
}
```

### Regla de correo pendiente (deuda del servidor)

Todavía no hay ninguna regla en `mon.medicalopenworld.org` que reenvíe esto
por correo. La recomendada (documentada también en ADR-0001,
`docs/adr/0001-contacto-soporte-via-thingsboard-y-mailto.md`):

1. **Filtro** (script): deja pasar el mensaje solo si trae la clave
   `support_request` (evita que cada punto de telemetría periódica dispare
   un correo).
2. **To email**: `to` = `${support_to}`, `subject` = `${support_request}`,
   `body` = `${support_message}` + `${support_report}`.
3. **Send email**: al SMTP configurado en TB.

Hasta que esa regla exista, la petición **queda registrada en el
dispositivo de ThingsBoard** (consultable como cualquier otra telemetría) y
la pantalla del HMI dice **"Peticion registrada"**, nunca "correo enviado":
el envío por correo depende de que la regla exista. La vía del QR
`mailto:` (móvil del operador) funciona igual en ambos casos, con o sin
regla y con o sin red.
