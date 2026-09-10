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

## 9. Baja automática de la SIM de una unidad instalada

Una unidad instalada en un hospital con WiFi propia **ya no usa la SIM para
nada**: `GPRS_Handler()` deja de publicar telemetría en cuanto hay WiFi
(`motherBoard/src/tasks/GPRS.cpp`), y el módem solo sigue atado a la red móvil
para la posición y el reloj. La cuota, en cambio, se sigue pagando. Esta nota
es el lado servidor de `mb-sim-auto-deactivation`.

### El firmware no decide nada

El equipo publica cinco atributos de cliente y se detiene ahí:

| Atributo | Qué es |
|---|---|
| `wifi_ssid` | La red asociada, saneada a ASCII imprimible |
| `wifi_is_default` | `true` mientras sea la `WIFI_SSID` compilada: nadie ha aprovisionado el equipo |
| `wifi_dwell_days` | Días UTC **distintos** con esa red |
| `wifi_dwell_since` | Epoch de la primera asociación con reloj válido; `0` si aún no lo hubo |
| `wifi_dwell_span_d` | Días transcurridos desde ese epoch |

No lleva el umbral, ni la lista de redes propias, ni la clave de Onomondo, y no
habla con `api.onomondo.com` fuera del test de fábrica `sim_act`. **La razón es
que `ONOMONDO_API_KEY` controla TODA la flota y los `firmware.bin` de release
están publicados en un repo público**: ponerla en el firmware de campo sería
publicar el control de cada SIM de la organización. La otra razón es que un
equipo que no puede dar de baja su propia SIM no puede equivocarse al hacerlo.

### La política, y por qué la red de seguridad no es opcional

Dar de baja la SIM cuando la unidad reporte:

- `wifi_is_default` en `false`, **y**
- `wifi_dwell_days` ≥ **14**, **y**
- un `wifi_ssid` que **no** esté en la lista de redes propias de la
  organización (el taller, el aula de formación) que mantiene el operador.

La llamada es `PATCH /sims/{iccid}` con `{"activated":false}`. Detalles de la
API, comprobados en vivo: la cabecera es `authorization: <clave>` **cruda, sin
`Bearer`** — con `Bearer` devuelve 401, aunque la spec OpenAPI la etiquete
`bearerAuth` — y el ICCID vale directamente como `{id}`, sin traducirlo al SIM
ID de 9 dígitos del portal. Ver la cabecera de
`motherBoard/src/modules/factory_test/ftest_sim_activation.cpp`.

Y **obligatoriamente**: si tras la baja la unidad no reporta nada durante más de
**72 h**, reactivar la SIM automáticamente. Una unidad a la que se le ha quitado
la SIM **no tiene ningún transporte con el que recuperarla**: si el WiFi del
hospital se cae, se queda sin telemetría, sin posición GSM y sin fuente de
reloj en el siguiente arranque. El deshacer tiene que vivir donde vive la
clave. No armes la mitad que da de baja antes de que funcione la que reactiva.

Un atributo **rancio** no es evidencia: si `wifi_dwell_days` lleva sin
refrescarse más que la ventana de frescura de la política — por ejemplo tras
bajar de versión el firmware — no se toca la SIM.

### La clave de Onomondo no viaja en el firmware que se distribuye

Desde 2026-09-10 la activación de SIM del test `sim_act` **solo se compila en
los entornos `IncuNest_V18_factory` / `IncuNest_V17_factory`**
(`FTEST_SIM_ACT_ENABLED`). El motivo: `Credentials_public.h` hace
`#if __has_include("Credentials.h")`, así que en la máquina de quien tenga el
`Credentials.h` real **todos** los builds llevaban `ONOMONDO_API_KEY` dentro —
incluido el `IncuNest_V18` que alimenta `flasher_tool/data/firmware/` y los
assets de GitHub Releases, en un repo público. Y esa clave no es la credencial
de una unidad: controla todas las SIM de la organización.

Se apaga la **funcionalidad**, no se sobrescribe la clave: con el flag a 0 el
código no nombra la macro en ningún sitio, así que no puede acabar en el
binario. Verificado sobre los binarios reales:

| Entorno | `strings firmware.bin \| Select-String onok_` |
|---|---|
| `IncuNest_V18` (se distribuye) | 0 coincidencias |
| `IncuNest_V18_factory` (se queda en el taller) | 1 coincidencia |

**Regla operativa:** el binario de fábrica nunca va a
`flasher_tool/data/firmware/` ni como asset de release. Lo que se distribuye
es el entorno sin sufijo. En un build de campo `sim_act` da WARN
`solo fabrica`, que es lo correcto: ese firmware no tiene por qué activar
SIMs.

Pendiente aparte: el binario de fábrica sigue llevando la clave y llega al
portátil de quien monte. Con una tirada de 200 unidades eso son varias
máquinas con una credencial de flota. Las salidas son mover la activación al
flasher del PC, provisionar la clave en NVS al flashear, o aceptarlo y acotar
por proceso quién tiene ese binario.

### Riesgo aceptado

El único criterio es la permanencia en una red, así que **un montaje o un curso
que se alargue más de 14 días en la misma red daría de baja la SIM antes de
tiempo**. Lo que acota el daño son las otras dos piezas: la lista de redes
propias, que se lleva por delante el caso habitual, y la reactivación a las
72 h. La posición y las horas de uso (`Control_active_time`) también llegan al
servidor, así que se pueden añadir al criterio más adelante sin tocar el
firmware.

### Orden de despliegue

1. El firmware en campo, y los cinco atributos **llegando y con valores
   plausibles** en toda la flota.
2. La política en modo solo-informe durante al menos una ventana completa de
   14 días, revisando unidad por unidad si el sitio que reporta es el que
   realmente es.
3. Solo entonces, armar la baja — con la reactivación ya funcionando.

**Toda unidad ya desplegada necesita 14 días más** desde el momento en que toma
este firmware: las claves de NVS no existían antes, así que el contador arranca
en la primera asociación posterior a la actualización. Es el comportamiento
correcto — el firmware no tiene forma de saber cuánto llevaba en esa red.
