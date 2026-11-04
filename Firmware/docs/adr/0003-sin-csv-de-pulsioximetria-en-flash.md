# ADR-0003: Se elimina el CSV de pulsioximetría en flash; la captura en crudo del AFE4490 saldrá por red desde RAM

- **Estado**: aceptado
- **Fecha**: 2026-11-04
- **Placas afectadas**: motherBoard
- **Cambio OpenSpec**: ninguno (corrección de defecto; la spec de
  pulsioximetría está pendiente de escribir)
- **Commits**: `6522041d` (eliminación del escritor CSV, motherBoard 18.56.0),
  `3d821c9e` (`-O2` real y prioridad de la tarea del AFE)

## Contexto

Con un simulador de paciente a **135 lpm**, la V18 mostraba **152 lpm**
(HR1, HR2 y HR3 por igual) mientras SpO2 y PI coincidían con dos PulseNest
que llevan la misma librería `incunest_afe4490`. Medido en banco el
2026-11-02 con un contador temporal de muestras en `SPO2_Task`
(`src/drivers/SPO2.cpp`, consumidor de `afe.getData()`), ventanas de 10 s:

| Build (18.55.0) | Muestras/s de 500 | HR mostrada |
|---|---|---|
| Sonda retirada | 494,6 (0,989) | — |
| Sonda aplicada | 444–451 (**0,89**) | **152** |
| + `-O2` | 458–460 (0,92) | 147 |
| + tarea AFE con prioridad 11 | 476,7 (0,953) | 141,6 |
| + sin CSV a LittleFS | **500,0 (1,000)** | **135,0** |
| Solo sin CSV | 494–495 (0,989) | 136,4 |

Mecanismo. `DriveUpload` (`src/tasks/DriveUpload.cpp`) escribía, con la
sonda aplicada, un CSV de 500 muestras/s a LittleFS (`/pox_<ms>.csv`,
ventanas de 60 s, ~0,9 MB) para subirlo a Google Drive. Cada programación de
página o borrado de sector de la flash deshabilita la caché de instrucciones
de la CPU: el código que vive en flash se detiene, y la tarea de la
librería que lee el AFE por SPI está en flash. La librería espera DRDY con
un **semáforo binario** desde una ISR en IRAM y el AFE4490 **no tiene
FIFO**: los DRDY que llegan durante la parada se funden en uno y la muestra
se pierde sin dejar rastro (no hay contador ni marca de tiempo por muestra
en la v0.113). Los tres algoritmos de HR convierten "muestras entre
latidos" a lpm con la frecuencia nominal de 500 Hz, así que un 11 % de
muestras perdidas infla la HR ×1,126. SpO2 y PI son cocientes entre dos
señales que pierden las mismas muestras y no se ven afectados.

Hallazgo colateral: `platformio.ini` pedía `-O0` en `build_flags`, pero con
`build_type = debug` PlatformIO añade después `debug_build_flags` (por
defecto `-Og`), de modo que el `-O0` nunca se aplicó y el nivel real era
`-Og` para todo, librería incluida.

Por qué no vale bajar la frecuencia del CSV: la pérdida la causa **cada
escritura**, no el volumen. A 50 Hz la escritura es menos frecuente pero
sigue tirando una ráfaga de muestras cada vez, y el CSV grabado sigue
teniendo huecos. Por qué no vale un buffer grande en RAM antes de la flash:
la V18 (ESP32-S3 sin PSRAM, ~107 KB de heap libre en marcha) no puede
guardar 500 Hz × 60 s (~360 KB); eso solo cabe en la V19 con PSRAM.

## Opciones consideradas

| Opción | A favor | En contra |
|---|---|---|
| A. La librería cuenta los DRDY en la ISR y expone `sample_index` / `samples_missed`, rellenando los huecos para que la base de tiempos de HR sea exacta | Defensa frente a cualquier causa (flash, OTA, NVS, tareas futuras); detecta el problema sin simulador | Está en otro repo (`incunest_afe4490`); no evita la pérdida, la compensa |
| B. Desactivar `DriveUpload` por defecto y activarlo solo para investigación | Un `#define` | Durante una captura el fallo sigue y el CSV sigue con huecos; código muerto en producción |
| C. Enviar la captura por red sin pasar por flash (ráfagas cortas desde RAM, sin prisa) | Quita la causa de raíz; `PpgSnapshot` ya hace algo parecido (8 s a 50 Hz cada 15 min por ThingsBoard) | Límite de RAM en V18: 50 Hz × 60 s (~36 KB) sí; 500 Hz continuos no; cada subida HTTPS cuesta ~30–40 KB de heap en el handshake TLS |
| D. Ventana en PSRAM y escritura con la sonda retirada | Conserva el CSV a 500 Hz | Solo V19 |
| E. `-O2` real y prioridad de la tarea del AFE por encima de todas las de IncuNest | Dos líneas de `platformio.ini`; recupera el ~1 % de CPU | No arregla la flash; cambia el nivel de optimización de todo el firmware |
| **Eliminar el escritor CSV + E; A y C como continuación** | Resuelve el defecto medido (500,0 muestras/s, 135,0 lpm); sin código muerto; la subida de informes de crash a Drive se conserva | Se pierde la captura en crudo hasta que exista C; A depende de la librería |

## Decisión

Se elimina el escritor CSV de `DriveUpload` (tarea `DRV_WR`, cola de
muestras, `drivePushSample()` y la llamada desde `SPO2_Task`), se conserva
la subida de informes de crash a `<SN>/1-Logs/` y al arrancar se borran los
`/pox_*.csv` que dejara un firmware anterior (`DriveUpload.cpp:434`). La
decisión la toma Alex: `DriveUpload` se escribió a petición suya para
obtener señal real de bebés con la que mejorar la librería, y ese objetivo
se cubrirá con C (captura en RAM publicada por red), no con la flash.

Además (E): `debug_build_flags = -O2 -g2 -ggdb2` (`platformio.ini:113`),
que es el único sitio donde el nivel de optimización se aplica de verdad
con `build_type = debug`; y la tarea de lectura del AFE pasa a prioridad 11
(`-DINCUNEST_AFE4490_TASK_PRIORITY=11`, `platformio.ini:125`), por encima de
`GPRS_MONITOR` (10), `SECURITY` (9) y `SPO2` (8) de
`include/config/task_config.h`; las tareas HR23 y DIAG de la librería bajan
a 4 y 3. Con las dos cosas, medido: **500,0 muestras/s, HR1/2/3 = 135,0**.

Regla que queda establecida: **ninguna tarea de la motherBoard escribe en
flash a ritmo de muestra del AFE4490** (la sonda puede estar aplicada en
cualquier momento, así que no hay ventana "segura"). Las escrituras
esporádicas que siguen existiendo (NVS del temporizador de fototerapia cada
60 s, perfiles de bebé, informes de crash, OTA) no se tocan; cada una tira
una ráfaga de muestras y por eso A sigue siendo necesaria.

## Consecuencias

- HR correcta en la V18 (el error era del 12 % en un monitor neonatal).
- Se pierde la captura en crudo a 500 Hz. Sustituto: ráfagas desde RAM por
  ThingsBoard (C), pendiente de decidir la duración que hace falta; en V18
  cabe 50 Hz × 60 s, en V19 también 500 Hz.
- `-O2` cambia cómo se compila todo el firmware. Deuda: vigilarlo en banco
  y que Pablo lo revise; si aparece un comportamiento distinto al de `-Og`,
  este es el primer sospechoso.
- Los CSV ya subidos a Drive antes de 18.56.0 tienen huecos (saltos en
  `t_ms`): cualquier análisis hecho con ellos arrastra una base de tiempos
  errónea.
- **Deuda explícita en la librería (A)**: contador de DRDY en la ISR,
  campos `sample_index` / `samples_missed` / `gap_samples` en `AFE4490Data`
  y relleno de huecos. Brief en
  `docs/notes/2026-11-04-brief-libreria-muestras-perdidas.md`. Cuando
  exista, IncuNest publica `samples_missed` en `/debug/state` y en
  telemetría y registra un evento si supera el 1 % en 10 s.
- **Revisar este ADR si**: la librería lee el AFE desde la ISR en IRAM a un
  buffer en RAM (A+, encaja con el porte a ESP-IDF), porque entonces las
  escrituras en flash dejan de perder muestras y la regla de arriba deja
  de hacer falta; o si la V19 con PSRAM vuelve a necesitar captura continua a 500 Hz
  (opción D).
