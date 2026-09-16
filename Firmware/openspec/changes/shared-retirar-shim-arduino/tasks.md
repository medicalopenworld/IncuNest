# Tareas

## Fase 0 — DECIDIDA (2026-09-16): separar y blindar, **no retirar**

- [x] **Decidido: NO se retira.** Los cuatro defectos ya están arreglados y el shim de hoy funciona. Migrarlo sería reescribir código que ya va, escribiendo código nuevo contra IDF que puede divergir en silencio exactamente igual: **mueve el riesgo, no lo elimina**. El problema nunca fue que el shim tuviera *forma* de Arduino, fue que estaba *mal*. Son cosas distintas y sólo una ha costado días.
- [x] **Decidido: se separa por capas.** Hecho en `d56c987`. Riesgo cero —sólo `CMakeLists`, ni una línea de lógica—, resuelve un problema real (un test de ficheros compilaba mDNS, WiFi, HTTP y mbedTLS) y es requisito previo de cualquier retirada futura.
- [x] **Decidido: `String` no se toca** salvo oportunistamente en caminos calientes, como ya se hizo en `SPO2.cpp` (`25d745e`). Y el **grupo A queda fuera**: renombrar envoltorios que ya son `inline` sobre IDF no elimina riesgo y mueve 157 inclusiones.
- [x] **Contrastado con lo abierto.** Sigue sin validarse `SPI_FLASH_AUTO_SUSPEND` en una segunda unidad —con dos fabricantes de flash distintos en dos placas—, el umbral del ventilador sale de una sola placa, el temblor del panel está acotado pero no eliminado, y 12 ficheros de `.claude/` dan instrucciones que fallan. **Eso es riesgo vivo; el shim es riesgo mitigado cuatro veces y con red puesta.**

### Cuándo revisar esta decisión

No es para siempre. Retirar el shim pasaría a tener sentido si:

- **entra gente que conoce ESP-IDF y no Arduino** — ahí el coste de traducción mental se paga cada día;
- **el shim bloquea una función de IDF** que su forma Arduino no deja exponer;
- **la capa deja de cambiar**: si en seis meses nadie la toca, se cae el argumento de mantenerla, pero también el de que da problemas.

## Fase 1 — red de seguridad (vale igual si se decide blindar)

> **Precedente ya montado.** La primera está hecha y sirve de plantilla para
> las otras tres: convención de `test_apps/` de IDF (la misma de
> `SensorBoard_v2`), `WHOLE_ARCHIVE` para que Unity registre los casos,
> `EXTRA_COMPONENT_DIRS` apuntando al **componente concreto** —sobre
> `components/` entero arrastra `thingsboard` y con él `mqtt`— y la tabla de
> particiones de la placa por ruta relativa, sin la cual no hay partición
> `spiffs` donde montar.
>
> **Y dejó un dato para la Fase 0**: `incunest_platform` es un solo componente
> con **21 REQUIRES**, así que un test que sólo quiere el sistema de ficheros
> compila mDNS, WiFi, HTTP y mbedTLS. Separar el shim por capas mejoraría la
> testabilidad de inmediato, y es más barato que retirarlo.

- [x] **Test de contrato de `plat_fs` — HECHO** (`4334fd6`). `motherBoard/test_apps/plat_fs_test/`, Unity sobre la placa. 5 casos: `name()` pelado, `path()` completa, rutas sin prefijo de montaje, `totalBytes`/`usedBytes` coherentes y `remove()` sobre ruta compuesta a partir de `name()`. **Verificado que caza el fallo**: reintroducido el defecto de `d324f9e` da 2 de 5 en rojo; restaurado, 5/0.
- [ ] Test de contrato de `plat_i2c`: el bloqueo abarca la transacción con repeated-start; dos tareas concurrentes no se entrelazan.
- [ ] Test de contrato de `plat_net_client`: `available()` refleja los bytes pendientes de verdad; el `timeout` se aplica donde se dice.
- [ ] Test de contrato de `plat_nvs`: `putFloat`/`putDouble` siguen guardando **BLOB**, como hacía Arduino. Cambiarlo deja sin perfiles de bebé a las unidades en campo.
- [ ] Comprobar que cada test FALLA si se le reintroduce el defecto histórico. Un test que pasa con el bug puesto no sirve.

## Fase 2 — retirada del grupo B, una capa por commit

**NO SE HACE.** La Fase 0 decidió separar y blindar. Se deja escrito lo que
habría sido, por si alguna de las tres condiciones de arriba se cumple.

- [ ] `plat_fs` → llamadas POSIX directas. Verificar en banco: perfiles de bebé y pesos intactos tras reinicio y tras OTA.
- [ ] `plat_i2c` → `i2c_master_*` de IDF. Verificar en banco con los cuatro consumidores simultáneos (BQ25730, humidificador, INA3221, SensorBoard).
- [ ] `plat_net_client` → sockets de lwIP. Verificar reloj, geolocalización, Drive y SIM, que fue lo que rompió en silencio.
- [ ] `plat_nvs` → `nvs_*` de IDF. Verificar la migración de datos existentes, no sólo la escritura nueva.
- [ ] Batería de banco (17) y tests de host (26) verdes tras cada capa, con la placa flasheada de verdad.

## Fase 3 — `String`: NO SE HACE por barrido

- [ ] Inventariar los 388 usos por temperatura del camino: 500 Hz / ISR / control / arranque / configuración.
- [ ] Migrar primero los calientes, a buffer de pila con `snprintf`.
- [ ] Verificar que el binario no crece y que el heap libre en régimen no baja.
- [ ] Dejar los fríos para el final, o no tocarlos.

## Cierre

- [ ] Cerrar la sección 7 de `docs/porte-esp-idf-nativo.md` con lo que quede retirado y lo que se decida conservar, **con el porqué**.
- [ ] Actualizar `docs/architecture.md`.
- [ ] Archivar este cambio con `openspec archive`.
