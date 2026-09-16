Cada fase es un commit atómico. No se mezclan dos placas en un commit salvo la
fase 1, que es `shared/` y afecta a ambas por diseño
(`.claude/rules/commits.md`).

Comando de tests de host en todas las fases que lo mencionen (PlatformIO ya no
existe, ver `design.md`):

```
cmake -S Firmware/tools/host_tests -B Firmware/tools/host_tests/build
cmake --build Firmware/tools/host_tests/build
ctest --test-dir Firmware/tools/host_tests/build --output-on-failure
```

## 1. shared: vocabulario del protocolo

- [x] 1.1 Añadir el enum `TimeSource` (`NONE=0`, `NITZ=1`, `RTC=2`, `NTP=3`, `MANUAL=4`) a `shared/include/`, documentando que su escala es independiente de `TzSource`.
- [x] 1.2 Documentar en `Firmware/PROTOCOL.md` el cuarto campo de datos `src` de `CTRL,TIME`, con su regla de compatibilidad opcional en ambos sentidos.
- [x] 1.3 Documentar en `Firmware/PROTOCOL.md` el mensaje nuevo `HMI,RTC_TIME,epoch,tzq,tzsrc`, incluyendo que es episódico y por qué (known_issues #2).
- [x] 1.4 Corregir en `PROTOCOL.md` la frase que afirma que el HMI no tiene RTC y no sincroniza por su cuenta.

## 2. motherBoard: árbitro de fuentes de hora (TDD, host)

- [x] 2.1 Escribir en rojo `motherBoard/test/test_time_source/test_time_source.cpp` con los escenarios de `specs/mb-time-source-priority/`: NTP sobre RTC, NITZ no desplaza RTC, manual gana a todo, rangos iguales gana el más reciente, reset al reiniciar.
- [x] 2.2 Registrar `modules/util/time_source.cpp` en la lista `MB_SOURCES` de `tools/host_tests/CMakeLists.txt`. Las suites se descubren por glob, así que la suite nueva no necesita registro.
- [x] 2.3 Implementar `modules/util/time_source.{h,cpp}` como lógica pura, sin Arduino ni red, siguiendo el patrón de `tz_source.cpp`. Verde en ctest.
- [x] 2.4 Añadir a la suite los casos de validación de ventana `[2021-01-01, 2100-01-01)` reutilizando los límites que ya usa `civil_time`.

## 3. motherBoard: punto único de fijación del reloj

- [x] 3.1 Generalizar `systemClockSetManual(epoch)` a `systemClockSet(epoch, src)` en `modules/util/system_clock.{h,cpp}`, consultando a `time_source` antes de `settimeofday()`. Conservar `systemClockSetManual()` como envoltorio para no tocar sus dos llamantes.
- [x] 3.2 **Separar las dos ramas de `tasks/GPRS.cpp`**: hoy NITZ y NTP sobre PDP comparten la variable `got` y el `settimeofday()` de la línea ~456. Cada una debe llamar a `systemClockSet()` con su rango propio, `NITZ` y `NTP`. Es el punto de mayor riesgo del cambio.
- [x] 3.3 Registrar el rango NTP cuando SNTP fije el reloj por su cuenta, vía `sntp_set_time_sync_notification_cb()`, en las rutas de `Wifi_OTA.cpp`, `DriveUpload.cpp` y `gprs_modem.cpp`.
- [x] 3.4 Comprobar que el `esp_sntp_stop()` que hoy protege la hora manual sigue en pie: un callback llega después de que el reloj ya se haya movido, así que no basta por sí solo.
- [x] 3.5 Verificar que la ruta del huso no cambia de comportamiento: NITZ sigue ganando a IP en `tz_source`, y un NITZ rechazado por rango insuficiente sigue aplicando su offset. Ampliar `test/test_tz_source/` con ese caso cruzado.

## 4. motherBoard: protocolo

- [x] 4.1 Emitir el campo `src` en el `CTRL,TIME` de `tasks/CommTask.cpp`, sin tocar la cadencia de 10 s.
- [x] 4.2 Parsear `HMI,RTC_TIME` en `tasks/CommTask.cpp` validando número de campos y parseabilidad numérica antes de indexar, y descartando en silencio la línea mal formada (`.claude/rules/security.md`).
- [x] 4.3 Enrutar la semilla aceptada a `systemClockSet(epoch, TIME_SOURCE_RTC)` y su huso a `tz_source_set()`.
- [x] 4.4 Escribir la suite de host del parseo de `HMI,RTC_TIME` (campos de menos, campo no numérico, epoch de 1970, epoch válido), extrayendo el parseo a una función pura si hoy no lo es.
- [x] 4.5 Compilar la motherBoard y verificar en banco que `CTRL,TIME` sale con el campo nuevo y la cadencia intacta. **Verificación manual**, no hay entorno de test para `tasks/`.

## 5. Display_HMI: driver del PCF8563

- [x] 5.1 Implementar `Display_HMI/src/drivers/rtc_pcf8563.{h,cpp}`: lectura y escritura de los siete registros de tiempo en `0x51`, sobre el bus I2C ya inicializado. Sin alarma, sin temporizador, sin `CLKOUT`.
- [x] 5.2 Implementar la validación de lectura: flag `VL` del registro de segundos, nibbles BCD en rango, ventana `[2021, 2100)`, y relectura cuando el registro de segundos cambie entre dos muestreos.
- [x] 5.3 Implementar la convención de siglo: bit de siglo siempre a `0`, año leído como `2000 + YY`, rechazo de 2100 o posterior antes de escribir.
- [x] 5.4 Separar la conversión BCD ↔ epoch en una unidad pura, sin I2C, para que pueda entrar en los tests de host.
- [x] 5.5 Verificar en banco lectura y escritura reales contra el chip, y el caso de pila retirada dando `VL=1`. **Verificación manual.**

## 6. Display_HMI: persistencia, decisión de escritura y siembra

- [x] 6.1 Añadir a `include/config/EEPROM_defines.h` la clave NVS de la terna (`tzq`, `tzsrc`, `src`) empaquetada en un único `uint32_t`, con su comentario de por qué va en una sola clave.
- [x] 6.2 Implementar la decisión de escritura como función pura: escribe si el rango recibido mejora al guardado, o si iguala y la deriva supera 2 s. Nunca con `epoch=0` ni con rango peor.
- [x] 6.3 Parsear el campo `src` en el `CTRL,TIME` de `src/tasks/CommTask.cpp`, tolerando su ausencia como rango `NONE`, igual que ya se tolera la de `tzq`/`tzsrc`.
- [x] 6.4 Enviar `HMI,RTC_TIME` al arrancar y, después, solo mientras llegue `CTRL,TIME` con `epoch=0`, como mucho uno por cada `CTRL,TIME` recibido. Callar en cuanto la motherBoard anuncie hora.
- [x] 6.5 Corregir el comentario de `include/tasks/CommTask.h:249` que afirma que el HMI no tiene RTC.

## 7. Cobertura de host para la lógica pura del HMI

- [x] 7.1 Añadir a `tools/host_tests/CMakeLists.txt` una lista `HMI_SOURCES` con las unidades puras del HMI (conversión BCD ↔ epoch, decisión de escritura) y sus rutas de include, sin arrastrar I2C ni LVGL.
- [x] 7.2 Escribir las suites Unity de esas dos unidades con los escenarios de `specs/hmi-rtc-clock/`: `VL=1`, ventana inválida, años de la convención de siglo, mejora de rango, deriva por debajo y por encima del umbral, `epoch=0`.
- [x] 7.3 Confirmar en la tabla SUMMARY de ctest que las suites nuevas pasan, suite a suite y no solo el total (`.claude/rules/tooling.md`).

## 8. Verificación en banco y documentación

- [ ] 8.1 Ciclo de alimentación con red disponible: el equipo arranca con fecha correcta sin esperar a la red. **Manual.**
- [ ] 8.2 Ciclo de alimentación sin red: la fecha sobrevive y el huso también, la interfaz pinta hora local y no UTC. **Manual.**
- [ ] 8.3 Unidad con GPRS y sin WiFi: comprobar que acaba con rango NTP y no NITZ ni RTC, que es el fallo que introduciría una separación incorrecta en la fase 3.2. **Manual.**
- [ ] 8.4 Ajuste manual desde la pantalla táctil sobre un reloj ya sembrado: la hora tecleada manda y ninguna fuente automática la desplaza hasta el reinicio. **Manual.**
- [ ] 8.5 Cruce de versiones de firmware, las cuatro combinaciones de placa nueva y vieja. Ninguna debe empeorar respecto a hoy. **Manual.**
- [ ] 8.6 Medir el tráfico del enlace durante un arranque sin red y confirmar que `HMI,RTC_TIME` no supera un mensaje por `CTRL,TIME` recibido. **Manual.**
- [x] 8.7 Actualizar `.claude/rules/testing.md`, `embedded-motherboard.md` y `tooling.md`, que siguen documentando `pio test -e native` y un `platformio.ini` que ya no existe.
- [ ] 8.8 Anotar en `docs/` la deriva real observada del PCF8563 a temperatura de incubadora y confirmar o ajustar el umbral de 2 s.
