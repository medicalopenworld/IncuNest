## Context

El reloj de pared del sistema vive hoy en la motherBoard y **solo en RAM**. Lo
fijan cuatro rutas que no se hablan entre sí:

| Ruta | Dónde | Cómo fija la hora |
|---|---|---|
| SNTP por WiFi | `tasks/Wifi_OTA.cpp:1744`, `tasks/DriveUpload.cpp:287` | `configTime()`, el propio SNTP escribe el reloj |
| SNTP por PPP | `tasks/gprs_modem.cpp:182` | `configTime()` sobre el contexto PDP |
| NITZ / NTP del módem | `tasks/GPRS.cpp:456` | `settimeofday()` directo |
| Manual | `modules/util/system_clock.cpp` | `settimeofday()` + `esp_sntp_stop()` |

Solo la última declara prioridad, con un booleano (`s_manual`) y un método
brusco pero eficaz: parar SNTP para que no la desplace. Las otras tres se pisan
entre sí por orden de llegada.

Para el huso sí existe ya un árbitro bien hecho: `modules/util/tz_source.cpp`,
lógica pura, sin Arduino ni red, con su suite en `test/test_tz_source/`. Este
diseño **replica ese patrón** para el instante.

La pieza nueva de hardware es un **PCF8563** en el HMI, I2C `0x51`, en el mismo
bus que el táctil (IO15/IO16, 400 kHz) y que el controlador de
retroiluminación y zumbador (`0x30`). No hay conflicto de direcciones. El
firmware del HMI no lo usa hoy: `include/tasks/CommTask.h:249` afirma lo
contrario, que el HMI no tiene RTC.

Nota de entorno: `platformio.ini` ya no existe en ninguna de las dos placas
(commit `c5e7b40`). Los tests de host son CMake + Unity en
`Firmware/tools/host_tests`, con una suite por directorio `motherBoard/test/test_*`
descubierta por glob. Varias reglas de `.claude/rules/` siguen diciendo
`pio test -e native` y están desfasadas.

## Goals / Non-Goals

**Goals:**

- Que el equipo no vuelva a arrancar sin fecha después del primer arranque con
  red, aunque se despliegue sin cobertura.
- Un único árbitro del instante, con prioridad explícita y probada en host, en
  vez de cuatro rutas que se pisan.
- Conservar el huso entre apagados, no solo el instante.
- Que la extensión del protocolo sea aditiva y no rompa ningún cruce de
  versiones de firmware entre placas.
- Estrenar cobertura de host para lógica pura del HMI, que hoy no tiene ninguna.

**Non-Goals:**

- No se usa el RTC como referencia de precisión. Deriva minutos al mes y NTP lo
  corrige siempre que aparezca.
- No se usan la alarma, el temporizador ni la salida `CLKOUT` del PCF8563.
- No se añade base de datos de husos ni horario de verano. El huso sigue siendo
  un offset comunicado desde fuera, como hoy.
- No se toca el formato en que se almacena nada: todo sigue en UTC, y el offset
  solo se aplica al formatear.

## Decisions

### 1. Un módulo árbitro nuevo, `modules/util/time_source`, hermano de `tz_source`

Lógica pura en C, sin Arduino, sin red, sin `settimeofday()`. Guarda el rango
vigente y responde a una sola pregunta: ¿esta fuente puede fijar el reloj?

```c
typedef enum {
  TIME_SOURCE_NONE   = 0,
  TIME_SOURCE_NITZ   = 1,
  TIME_SOURCE_RTC    = 2,
  TIME_SOURCE_NTP    = 3,
  TIME_SOURCE_MANUAL = 4,
} TimeSource;

bool time_source_accepts(TimeSource src);   // ¿gana al rango vigente?
bool time_source_set(uint32_t epoch, TimeSource src);
TimeSource time_source_origin(void);
void time_source_reset(void);               // para los tests
```

**Rango creciente, gana el mayor.** La alternativa era numerar al revés, con el
1 como el mejor, que es como se suele hablar de prioridades. Se descarta porque
en código `if (nueva > vigente)` se lee solo, mientras que `if (nueva <
vigente)` invita al error de signo cada vez que alguien lo toca.

**Escala independiente de `TzSource`.** Comparten forma pero no significado:
`TzSource` tiene IP en el 2 y no tiene NTP ni RTC. Fundirlas ahorraría un enum
y costaría un bug el día que alguien pase un valor de una a la otra. Se
mantienen separadas y el protocolo las transmite en campos distintos.

### 2. `system_clock` pasa a ser el único que escribe el reloj

`systemClockSetManual()` se generaliza a `systemClockSet(epoch, src)`, que
consulta a `time_source` y solo entonces llama a `settimeofday()`.
`systemClockSetManual()` se conserva como envoltorio de un renglón, para no
tocar sus dos llamantes actuales.

Las tres rutas que hoy escriben el reloj por su cuenta pasan por ahí. La que
más cuidado exige es `tasks/GPRS.cpp`: hoy **mezcla NITZ y NTP sobre PDP en la
misma variable `got`** y ambas acaban en el mismo `settimeofday()` de la línea
456. Son rangos distintos, 1 y 3, así que hay que separar las dos ramas antes
de llamar al árbitro. Es el punto donde este cambio es más fácil de
implementar mal.

Para SNTP el reloj no lo escribe el firmware sino la pila LwIP, así que la
integración es por *callback*: `sntp_set_time_sync_notification_cb()` informa
al árbitro de que el rango vigente pasa a NTP. El truco actual de
`esp_sntp_stop()` cuando la hora es manual se mantiene, porque un *callback*
solo se entera después de que el reloj ya se haya movido.

### 3. El RTC es del HMI, pero la autoridad sigue siendo la motherBoard

El chip está físicamente en el bus del HMI, así que el HMI es el único que
puede leerlo y escribirlo. Pero dos relojes que se fijen por separado acaban
discrepando, y toda la fecha del historial de alarmas y de los perfiles cuelga
del reloj de la motherBoard.

Se descarta que el HMI mantenga su propio reloj. El HMI **lee** el RTC al
arrancar y ofrece el resultado por UART; la motherBoard decide si lo acepta.
Después el HMI **escribe** el RTC con lo que la motherBoard difunda, cuando
merezca la pena. El HMI no decide nada sobre la hora, exactamente como hoy.

Flujo de arranque:

```
HMI: lee PCF8563 -> valida VL + ventana -> lee terna de NVS
HMI -> MB:  HMI,RTC_TIME,epoch,tzq,tzsrc
MB:         time_source_accepts(RTC)? -> settimeofday + tz_source_set
MB -> HMI:  CTRL,TIME,epoch,tzq,tzsrc,src        (cada 10 s, ya existía)
HMI:        src mejor que el guardado, o deriva > 2 s? -> escribe PCF8563 + NVS
```

### 4. `HMI,RTC_TIME` es episódico, y esto no es un detalle

`docs/known_issues.md` #2 documenta un desbordamiento real del enlace por
tráfico periódico. El mensaje se envía al arrancar y después **solo** mientras
la motherBoard siga anunciando `epoch=0`, como mucho uno por cada `CTRL,TIME`
recibido. En cuanto la motherBoard tenga hora, se calla hasta el reinicio.

El campo `src` no añade ningún mensaje: viaja dentro del `CTRL,TIME` que ya se
difunde cada 10 s.

### 5. El huso va a NVS porque el PCF8563 no tiene dónde guardarlo

El PCF8563 no tiene RAM de usuario respaldada por pila. A diferencia de un
DS1307, que tiene 56 bytes, aquí no hay hueco para el huso ni para el rango.
Van a la NVS del HMI, que ya guarda fechas de mantenimiento.

La terna (`tzq`, `tzsrc`, `src`) se escribe y se lee **como una unidad**. Si se
guardaran sueltas, una siembra podría combinar el instante de una escritura con
el huso de otra, y el error resultante son horas enteras en la fecha que sella
el historial. La `Preferences` del HMI no da transaccionalidad, así que se
serializa la terna en **una sola clave** como un `uint32_t` empaquetado.

### 6. La convención de siglo se fija ahora, no cuando falle

El PCF8563 guarda el año en dos dígitos BCD más un bit de siglo en el registro
de meses. Se escribe siempre el bit a `0` y se lee el año como `2000 + YY`. La
ventana válida ya es `[2021, 2100)` por `civil_to_unix_utc()`, así que la
convención cubre todo el rango representable y 2100 se rechaza antes de llegar
al chip.

### 7. La lógica pura del HMI entra en los tests de host

`tools/host_tests/CMakeLists.txt` compila hoy fuentes de `motherBoard/src` y de
`shared/src`. Se añade una tercera lista con las fuentes puras del HMI:
conversión BCD ↔ epoch y decisión de escritura. Nada de I2C ni de LVGL, que
quedan detrás de la frontera del driver.

Las suites se descubren por glob sobre `motherBoard/test/test_*`, así que las
suites nuevas se recogen solas. Se descarta meter esta lógica en `shared/` solo
para que la globara: `shared/` es el vocabulario del protocolo entre placas, y
la codificación BCD de un chip que solo existe en una de ellas no lo es. El
enum `TimeSource` sí va a `shared/`, porque viaja en el protocolo.

## Risks / Trade-offs

- **Separar mal las ramas NITZ y NTP en `GPRS.cpp`** → quedaría NTP degradado a
  rango NITZ y el RTC pisando a un NTP bueno. Es el punto de mayor riesgo del
  cambio. Se cubre con una suite de host sobre el árbitro, y en banco
  comprobando que una unidad con GPRS y sin WiFi acaba con rango NTP y no RTC.
- **Contención en el bus I2C compartido con el táctil** → una escritura del RTC
  a 400 kHz son decenas de microsegundos, pero se acota además por diseño: solo
  ante mejora de rango o deriva mayor que 2 segundos, nunca periódicamente.
- **Deriva del PCF8563** → es un RTC de cristal sin compensación térmica, así
  que minutos al mes, y más en una incubadora, que es un ambiente caliente.
  Aceptable para sembrar una fecha, inaceptable como referencia. Por eso NTP
  está por encima y lo corrige en cuanto aparece.
- **Pila agotada devolviendo basura creíble** → el flag `VL` cubre la parada del
  oscilador, y la ventana `[2021, 2100)` cubre lo que `VL` no vea. Una hora
  corrupta que caiga dentro de la ventana pasaría el filtro, pero NTP la
  corrige en cuanto haya red y su rango es superior.
- **Cruce de versiones de firmware entre placas** → los campos son opcionales en
  ambos sentidos, igual que `tzq` y `tzsrc` ya lo son. Hay que probar las cuatro
  combinaciones en banco, no solo la nueva contra la nueva.
- **Las reglas del repositorio dicen `pio test -e native`, que ya no existe** →
  se corrigen en `.claude/rules/` dentro de este cambio, o el siguiente que las
  lea escribirá comandos que no funcionan.

## Migration Plan

No hay migración de datos: no se persiste ningún formato nuevo salvo una clave
de NVS que nace vacía y se interpreta como «sin huso guardado», caso que el
protocolo ya contempla como `tzsrc=0`.

El orden de despliegue es indiferente gracias a la compatibilidad en ambos
sentidos. Con la motherBoard nueva y el HMI viejo se gana el árbitro de
prioridades y se pierde solo la siembra. Con el HMI nuevo y la motherBoard
vieja el HMI envía `HMI,RTC_TIME`, la motherBoard lo descarta por desconocido
como ya hace con cualquier línea que no entiende, y el `CTRL,TIME` sin `src` se
lee como rango desconocido, con lo que el HMI no escribe el RTC. Ninguna
combinación empeora respecto a hoy.

Retirada: revertir el commit. El contenido que quede escrito en el PCF8563 es
inerte para un firmware que no lo lea.

## Open Questions

- ¿Hay que enseñar el origen de la hora en la interfaz, por ejemplo distinguir
  una hora sembrada por RTC de una sincronizada por NTP? El dato ya viaja en
  `CTRL,TIME`, así que es decisión de producto, no de arquitectura. Fuera de
  alcance salvo indicación contraria.
- El umbral de 2 segundos para reescribir el RTC es un valor razonado, no
  medido. Conviene confirmarlo en banco una vez se vea la deriva real del chip
  a temperatura de incubadora.
