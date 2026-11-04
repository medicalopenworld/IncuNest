# Brief para la sesión de `incunest_afe4490` — detectar y compensar muestras perdidas (opción A)

Origen: sesión IncuNest "Bug HR 152 vs 135 (simulador)", 2026-11-02 a 2026-11-04. Este fichero es para pegarlo en la sesión de la librería; no se versiona en IncuNest.

## Qué se ha encontrado (medido, no supuesto)

- Placa IncuNest V18 (ESP32-S3, Arduino-ESP32 2.0.14, FreeRTOS), librería v0.113, simulador de paciente a 135 lpm. IncuNest mostraba **152 lpm** (HR1, HR2 y HR3, las tres); SpO2 y PI correctos. Dos PulseNest con la misma librería (v0.115) marcaban 135.
- Causa: IncuNest escribía en flash (LittleFS) un CSV de 500 muestras/s mientras medía. Cada programación de página / borrado de sector de la flash deshabilita la caché de la CPU; la tarea `_task_body()` de la librería, que vive en flash, se para. Como `_drdy_isr()` hace `xSemaphoreGiveFromISR(_drdy_sem)` sobre un **semáforo binario** y el AFE4490 **no tiene FIFO**, los DRDY que llegan durante la parada se funden en uno: la muestra se pierde **sin dejar rastro**.
- Medido con un contador en el consumidor (`getData()` en IncuNest), ventanas de 10 s:
  - 18.55.0 con el CSV: 445–451 muestras/s de 500 (0,89) → HR 152. Sin sonda (sin CSV): 0,99.
  - `-O2` en vez de `-Og`: 0,92. + tarea de la librería a prioridad 11 (por defecto 5): 0,953.
  - Sin el CSV: **500,0 (1,000) → HR 135,0**.
- Por qué infla la HR: HR1/HR2/HR3 convierten "muestras entre latidos" a lpm con la frecuencia **nominal** (`_afe_sample_rate_hz`). Con un 11 % menos de muestras por latido, HR ×1,126. SpO2 y PI son cocientes entre dos señales que pierden las mismas muestras: no cambian.
- IncuNest ya ha quitado el CSV (18.56.0) y ha subido la prioridad de la tarea. Pero en la placa siguen escribiendo en flash unos veinte módulos (NVS cada 60 s del temporizador de fototerapia, perfiles de bebé, informes de crash, OTA de 1,5 MB…). Cada uno puede tirar una ráfaga de muestras. **Ninguna aplicación puede garantizar que no se escriba en flash mientras se mide**: la defensa tiene que estar en la librería.

## Lo que se pide a la librería (A)

1. **Contar los DRDY en la ISR.** En `_drdy_isr()` (ya `IRAM_ATTR`, sigue corriendo con la caché deshabilitada): `_drdy_count++` (uint32, volátil o atómico) y opcionalmente `_drdy_last_us = esp_timer_get_time()` (también en IRAM). Coste: dos instrucciones.
2. **Detectar huecos en la tarea.** En `_task_body()`, tras tomar el semáforo, comparar `_drdy_count` con un `_processed_count`. `missed = drdy_count - processed_count - 1`. Si `missed > 0`, se ha perdido ese número de muestras **exacto** (no estimado).
3. **Exponer en `AFE4490Data`** (campos nuevos, sin romper el contrato existente):
   - `uint32_t sample_index` — índice real según DRDY (no el de muestras procesadas), para que cada medida lleve su número y un CSV/registro pueda mostrar los huecos.
   - `uint32_t samples_missed` — acumulado desde `begin()`.
   - `uint16_t gap_samples` — longitud del último hueco (0 si esta muestra no venía precedida de hueco).
   - Opcional: `int64_t t_us` del DRDY, para cuadrar con otros relojes.
4. **Compensar la base de tiempos.** Ante un hueco de *n* muestras, antes de procesar la nueva, alimentar *n* muestras de relleno a `_process_sample()` con el mecanismo que ya existe para `_should_freeze_input()` (últimos valores válidos, "constante benigna" que los filtros IIR/BPF absorben). Así HR1/HR2/HR3 y los bloques de SpO2/PI siguen contando el tiempo correcto. Para huecos grandes (p. ej. > 50 muestras = 100 ms) valorar marcar calidad: un bit RSQM nuevo o un `rsqi = 0` en la ventana, como ya se hace con `RSQM_DIAG_SWITCHED_RC_SETTLING`.
5. **Log y test.** `ESP_LOGW` con tasa limitada cuando `missed > 0`. Test de host: inyectar DRDY sin consumir y comprobar `samples_missed`, `gap_samples` y que la HR de una senoide a 2,25 Hz sigue siendo 135 con un 11 % de muestras perdidas.

## Cosas a tener en cuenta

- `_data_queue` (10 ítems) y `getData()` ya absorben retrasos del **consumidor**; el problema está **antes**, entre el chip y `_task_body()`. El contador ISR es el único sitio que ve todos los DRDY.
- `xSemaphoreTake(_drdy_sem, 100 ms)` → "DRDY timeout" solo detecta paradas > 100 ms; los huecos de 2–40 ms que causan esto son invisibles hoy.
- A medio plazo (A+): leer los 6 registros del AFE **desde la ISR** por SPI en IRAM a un buffer circular en RAM interna (512 muestras ≈ 12 KB cubren paradas de 1 s), de modo que no se pierda nada escriba quien escriba. Exige que el camino SPI entero esté en IRAM (en Arduino, `SPIClass` no lo está; en ESP-IDF, `CONFIG_SPI_MASTER_ISR_IN_IRAM` + transacciones por sondeo, o registros directos). Encaja con el porte a ESP-IDF puro. `samples_missed` de A sirve para verificar que A+ deja el contador a cero.
- En IncuNest, cuando la librería exponga los campos: publicar `samples_missed` y la tasa medida en `/debug/state` y en telemetría (`telemetry_keys.h`), evento de diagnóstico si > 1 % en 10 s, e índice de muestra en cualquier captura futura.
