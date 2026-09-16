# Tareas

## Fase 0 — decidir, antes de escribir código

Esta fase no produce código. Sin ella, las demás no deben empezar.

- [ ] **Decidir entre retirar o blindar.** La alternativa barata al grupo B es dejarlo y ponerle tests de contrato: ataca el mismo riesgo (que se comporte distinto y calle) por una fracción del coste y es reversible. Migrar sólo gana si además se quiere quitar la dependencia conceptual de Arduino.
- [ ] **Si se retira: fijar hasta dónde.** Retirar sólo el grupo B es un estado final coherente. Incluir `String` es el 80 % del esfuerzo y el 100 % del riesgo nuevo.
- [ ] **Contrastar con lo que hay abierto.** Tras el porte siguen sin probarse GPRS/SIM, táctil, audio, humidificador, fototerapia y SensorBoard; el temblor del panel está acotado, no eliminado; el umbral del ventilador está calibrado sobre una sola unidad. Eso es riesgo medido y vivo. El shim es riesgo ya mitigado cuatro veces.

## Fase 1 — red de seguridad (vale igual si se decide blindar)

- [ ] Test de contrato de `plat_fs`: `name()` devuelve el nombre pelado; el prefijo de montaje se antepone por dentro; `openNextFile()` recorre lo que debe.
- [ ] Test de contrato de `plat_i2c`: el bloqueo abarca la transacción con repeated-start; dos tareas concurrentes no se entrelazan.
- [ ] Test de contrato de `plat_net_client`: `available()` refleja los bytes pendientes de verdad; el `timeout` se aplica donde se dice.
- [ ] Test de contrato de `plat_nvs`: `putFloat`/`putDouble` siguen guardando **BLOB**, como hacía Arduino. Cambiarlo deja sin perfiles de bebé a las unidades en campo.
- [ ] Comprobar que cada test FALLA si se le reintroduce el defecto histórico. Un test que pasa con el bug puesto no sirve.

## Fase 2 — retirada del grupo B, una capa por commit

Sólo si la Fase 0 decide retirar.

- [ ] `plat_fs` → llamadas POSIX directas. Verificar en banco: perfiles de bebé y pesos intactos tras reinicio y tras OTA.
- [ ] `plat_i2c` → `i2c_master_*` de IDF. Verificar en banco con los cuatro consumidores simultáneos (BQ25730, humidificador, INA3221, SensorBoard).
- [ ] `plat_net_client` → sockets de lwIP. Verificar reloj, geolocalización, Drive y SIM, que fue lo que rompió en silencio.
- [ ] `plat_nvs` → `nvs_*` de IDF. Verificar la migración de datos existentes, no sólo la escritura nueva.
- [ ] Batería de banco (17) y tests de host (26) verdes tras cada capa, con la placa flasheada de verdad.

## Fase 3 — `String`, por zonas y sólo si la Fase 0 lo incluye

- [ ] Inventariar los 388 usos por temperatura del camino: 500 Hz / ISR / control / arranque / configuración.
- [ ] Migrar primero los calientes, a buffer de pila con `snprintf`.
- [ ] Verificar que el binario no crece y que el heap libre en régimen no baja.
- [ ] Dejar los fríos para el final, o no tocarlos.

## Cierre

- [ ] Cerrar la sección 7 de `docs/porte-esp-idf-nativo.md` con lo que quede retirado y lo que se decida conservar, **con el porqué**.
- [ ] Actualizar `docs/architecture.md`.
- [ ] Archivar este cambio con `openspec archive`.
