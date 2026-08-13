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

Fichero en `Firmware/dashboards/`, escrito para **ThingsBoard 4.2**:

- `incunest_pacientes.json`

En TB: **Dashboards → + → Import dashboard** y suelta el fichero.

Es **un solo dashboard con dos páginas** (estados): *Paciente actual* e
*Historial*. Se navega con el botón de la cabecera de los widgets — el de la
tarjeta del bebé lleva al historial, y el de la tabla de altas vuelve.

Usa un alias `Incubadoras` que resuelve **todos los dispositivos**, así que al
abrirlo aparece el selector de entidad arriba para elegir incubadora. Si
prefieres acotarlo a un perfil concreto, edita el alias y cambia el filtro de
`entityType` a `deviceType` con el nombre de vuestro tipo.

### Notas de esquema (verificadas contra el código de import de TB)

- Solo `title` y `configuration` son obligatorios en la raíz; el importador
  normaliza y autocompleta el resto.
- Cada widget aparece **dos veces**: la definición en `configuration.widgets`
  y su posición en `states.default.layouts.main.widgets`. Si falta la segunda,
  el widget existe pero no se dibuja.
- Se usa `typeFullFqn` (formato 3.6+), no el `bundleAlias`/`typeAlias` antiguo.
- Las claves de atributo llevan `"type": "attribute"`; `entityField` es otra
  cosa (campos propios de la entidad como `name` o `label`), no sirve aquí.

### Widgets deliberadamente omitidos

La **tarta de resultados** (`baby_outcome`) no va en el JSON: no pude
confirmar cuál de las dos variantes de pie chart expone la 4.2 por defecto, y
prefiero no meter un `typeFullFqn` que haga fallar la importación entera.
Añádela a mano sobre el dashboard de historial: widget de tarta, datasource
alias `Incubadoras`, clave `baby_outcome` de tipo timeseries.

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
