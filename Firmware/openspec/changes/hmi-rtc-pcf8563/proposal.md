## Why

Afecta a **Display_HMI** y **motherBoard** (el protocolo UART entre ambas, y por
tanto `shared/`).

Hoy la motherBoard es la única fuente de hora del sistema y todas sus fuentes
—SNTP por WiFi, NITZ/NTP del módem, ajuste manual— **necesitan red o un
operador**. El reloj vive solo en RAM: un ciclo de alimentación lo pierde
entero. Una unidad desplegada sin cobertura arranca sin fecha, y de la fecha
dependen la edad gestacional del bebé, los sellos del historial de alarmas y
los del historial de telemetría. El HMI lleva montado un **PCF8563** con pila
en el mismo bus I2C que el táctil (IO15/IO16), y hasta ahora el firmware ni lo
lee ni lo escribe: `include/tasks/CommTask.h` afirma explícitamente que el HMI
no tiene RTC.

Este cambio pone ese RTC a trabajar como memoria de la hora entre apagados, de
modo que a partir del primer arranque con red el equipo **nunca vuelva a
arrancar sin fecha**.

## What Changes

- **Driver nuevo del PCF8563 en el HMI** (`drivers/rtc_pcf8563`): lectura y
  escritura de la hora, comprobación del flag `VL` de integridad, y conversión
  BCD ↔ epoch. Comparte el bus I2C ya inicializado (dirección `0x51`, sin
  conflicto con el táctil ni con el buzzer en `0x30`).
- **Jerarquía de fuentes de hora explícita en la motherBoard**, de mayor a
  menor: **manual (1) > NTP/SNTP (2) > RTC del HMI (3) > NITZ (4)**. Hoy esa
  jerarquía existe a medias, solo como el booleano `systemClockIsManual()`.
  Una fuente de rango inferior **nunca** desplaza un reloj ya fijado por una
  superior mientras no se reinicie el equipo.
- **La prioridad ordena el epoch, no el huso.** El RTC no aporta zona horaria.
  Para `tzq` se mantiene la regla actual, **NITZ gana a IP**, y NITZ sigue
  siendo la mejor fuente de huso aunque sea la peor de hora. Al llegar un NITZ
  sobre un reloj ya sembrado por RTC se aplica **solo su offset**.
- **Siembra del reloj por UART**: mensaje nuevo `HMI,RTC_TIME` del HMI a la
  motherBoard con la tupla completa (epoch, `tzq`, `tzsrc`), enviado al
  arrancar y cuando la motherBoard difunda `epoch=0`.
- **`CTRL,TIME` gana un cuarto campo opcional, `src`**, con el rango de la
  fuente que fijó ese epoch. Sin él, el HMI no puede decidir si lo que recibe
  merece escribirse en el RTC. Compatible hacia ambos lados, como ya lo son
  `tzq` y `tzsrc`: un firmware antiguo que no lo envíe se interpreta como
  origen desconocido, y uno antiguo que lo reciba lo ignora.
- **Persistencia de `tzq`/`tzsrc` en la NVS del HMI.** El PCF8563 no tiene RAM
  de usuario respaldada por pila, así que el chip guarda la hora y la NVS
  guarda la zona. Hoy la zona vive solo en RAM y al perderla el display pinta
  UTC sin avisar.
- **Escritura del RTC acotada**: solo cuando llega una fuente de rango mejor
  que la guardada, o cuando la deriva supera 2 segundos. Evita castigar el bus
  compartido con el táctil.

No es **BREAKING**: toda la extensión del protocolo es aditiva y opcional.

## Capabilities

### New Capabilities
- `hmi-rtc-clock`: el RTC PCF8563 del HMI como memoria de la hora entre
  apagados — validación de integridad, lectura, escritura acotada, persistencia
  del huso en NVS y siembra del reloj de la motherBoard por UART.
- `mb-time-source-priority`: la jerarquía de fuentes de hora de la motherBoard
  (manual > NTP > RTC > NITZ), la regla separada para el huso, y la
  publicación del rango de la fuente en `CTRL,TIME`.

### Modified Capabilities
<!-- Ninguna: no hay spec previa que cubra el reloj de pared. `CTRL,TIME` y
     `HMI,SET_TIME` están documentados en PROTOCOL.md pero no tienen spec en
     openspec/specs/, así que su comportamiento nuevo entra en las dos
     capabilities nuevas de arriba. -->

## Impact

**Código afectado**

- `Display_HMI/src/drivers/rtc_pcf8563.{h,cpp}` — nuevo.
- `Display_HMI/src/tasks/CommTask.cpp` y `include/tasks/CommTask.h` — parseo del
  campo `src`, envío de `HMI,RTC_TIME`, y corrección del comentario que niega
  la existencia del RTC.
- `Display_HMI/include/config/EEPROM_defines.h` — claves NVS del huso.
- `motherBoard/src/modules/util/system_clock.{h,cpp}` — el booleano `manual`
  pasa a ser un rango de fuente; punto único por el que entran SNTP, NITZ, IP,
  manual y RTC.
- `motherBoard/src/tasks/CommTask.cpp` — emisión del campo `src` y recepción de
  `HMI,RTC_TIME`.
- `motherBoard/src/tasks/Wifi_OTA.cpp` y la ruta NITZ del módem — pasan a fijar
  la hora por el punto único en vez de llamar a `settimeofday` por su cuenta.
- `Firmware/PROTOCOL.md` — `CTRL,TIME` con cuarto campo, y `HMI,RTC_TIME` nuevo.

**Riesgo conocido**

`docs/known_issues.md` **#2 (UART Flooding)** desaconseja añadir tráfico
periódico evitable por el enlace. `HMI,RTC_TIME` es **episódico por diseño**,
no periódico: se envía al arrancar y solo mientras la motherBoard anuncie
`epoch=0`. El campo `src` viaja dentro del `CTRL,TIME` que ya se difunde cada
10 s, sin añadir mensajes.

**Hardware y dependencias**

PCF8563MDTR (XBLW) en IO15_SDA / IO16_SCL, dirección `0x51`. Es un RTC de
cristal sin compensación de temperatura: su deriva es de minutos al mes, no de
segundos, así que sirve como semilla pero **no** como referencia frente a NTP.
El año son dos dígitos más un bit de siglo, lo que obliga a fijar una
convención de siglo compatible con la ventana `[2021, 2100)` que ya impone
`civil_to_unix_utc()`.

**Verificación**

Display_HMI no tiene entorno de test nativo. La lógica pura (BCD ↔ epoch,
decisión de escritura por rango y deriva) se ubicará donde el
`[env:native]` de motherBoard pueda cubrirla con Unity; el resto es
verificación manual documentada sobre la unidad de banco.
