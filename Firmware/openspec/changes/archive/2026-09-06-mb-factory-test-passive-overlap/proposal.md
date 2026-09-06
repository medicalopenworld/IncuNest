## Why

Afecta a **motherBoard** (ejecutor) y, de forma menor, a **Display_HMI**
(qué fila sigue la página). La batería de fábrica es estrictamente secuencial:
en una línea de montaje sin cobertura móvil ni AP, los tests de conectividad
agotan sus plazos uno tras otro y la batería dura cuatro o cinco minutos, de
los que apenas 20 s son trabajo real. Un test que dura cinco minutos por
unidad acaba sin hacerse.

## What Changes

- **Solape cooperativo en la única tarea `FTEST`**, sin tareas nuevas: los
  tests **pasivos** (los que solo observan estado cacheado o una petición
  asíncrona: conectividad, charger, sensor ambiental, status y cámara de la
  SensorBoard, y todos los instantáneos) arrancan a la vez al inicio de la
  batería y se sondean cada 250 ms; los **activos** (standby, actuadores,
  RPM, zumbador, AFE, coherencia de SHT40 y luz con operador) siguen
  secuenciales y en el mismo orden, y entre sus pasos se sondean los pasivos.
- Peor caso pasa de ~4–5 min a ~45 s (el plazo pasivo más largo), y los
  tests GSM/WiFi tienen toda la batería para conectar, así que dan menos
  avisos por "aún no le había dado tiempo".
- El HMI puede ver varios RUNNING a la vez y resultados fuera de orden de id;
  la página sigue al **último test que emitió RUNNING** (el activo del
  momento). El watchdog por fila del display sube a 150 s.
- Sin cambio de protocolo de línea: mismos mensajes, distinto orden.

## Capabilities

### New Capabilities
<!-- Ninguna. -->

### Modified Capabilities
- `mb-factory-test`: orden de ejecución (pasivos en paralelo, activos
  secuenciales), cotas aplicables a cada clase.
- `hmi-factory-test`: fila seguida por la página y watchdog por fila.

## Impact

- `motherBoard/src/modules/factory_test/factory_test_task.cpp` (bucle),
  `factory_test_hw.cpp/.h` (firma de sondeo de los pasivos, flag en la
  tabla).
- `Display_HMI/src/ui/FactoryTest.cpp`.
- `PROTOCOL.md` (párrafo "Orden de ejecución"), `docs/hardware.md`.
