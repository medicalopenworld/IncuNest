# Nota para Pablo — HR inflada un 12 % por muestras perdidas del AFE4490 (V18, 18.55.0)

Fecha: 2026-11-02 (actualizada 2026-11-04) · Autor: Alex (medido en banco con Claude Code) · Rama: `feat/hmi-pulse-oximetry`

> **Actualización 2026-11-04.** Decisión tomada por Alex (DriveUpload se hizo a petición suya para capturar datos reales): el escritor CSV **se elimina** (commit 6522041d, motherBoard 18.56.0; la subida de informes de crash a `1-Logs` se conserva) y se aplica E (commit 3d821c9e: `-O2` real vía `debug_build_flags` y tarea del AFE a prioridad 11). Medido con los dos: **500,0 muestras/s y HR 135,0**. A (contador DRDY + relleno) va a la librería; brief en `notes/2026-11-04-brief-libreria-muestras-perdidas.md`. La captura en crudo se sustituirá por ráfagas en RAM publicadas por red (C'), pendiente de decidir la duración necesaria. Pablo: por favor, revisa sobre todo el paso a `-O2`, que cambia cómo se compila todo tu código.

## Resumen

Con un simulador de paciente a **135 lpm**, IncuNest muestra **152 lpm**. SpO2 y PI salen bien (coinciden con dos PulseNest con la misma librería).

Causa medida: mientras la sonda está aplicada, `DriveUpload` escribe el CSV de captura (500 muestras/s) en LittleFS. Cada escritura/borrado de sector en la flash para la CPU (caché deshabilitada) más de un periodo de muestreo (2 ms). La librería espera DRDY con un semáforo binario y el AFE4490 no tiene FIFO, así que la muestra que llega mientras la flash está ocupada **se pierde sin dejar rastro**. Los algoritmos de HR convierten "muestras entre latidos" en lpm con la frecuencia nominal de 500 Hz: con ~11 % menos muestras, la HR sale ×1,126. SpO2 y PI no se ven afectados porque son cocientes entre señales que pierden las mismas muestras.

## Mediciones (V18, simulador a 135 lpm, ventana de 10 s)

Endpoint temporal `/debug/afe_rate` que cuenta las muestras entregadas por la librería:

| Build | Muestras/s (de 500) | HR mostrada |
|---|---|---|
| 18.55.0 tal cual, sonda retirada | 494,6 (0,989) | — |
| 18.55.0 tal cual, sonda aplicada | 444–451 (**0,89**) | **152** |
| + `-O2` | 458–460 (0,92) | 147 |
| + tarea AFE con prioridad 11 (por defecto 5) | 476,7 (0,953) | 141,6 |
| + `drivePushSample()` desactivado (sin CSV a LittleFS) | **500,0 (1,000)** | **135,0** |
| Solo sin CSV (sin `-O2` ni prioridad) | 494–495 (0,989) | 136,4 |

Conclusión: el CSV explica ~10 de los 11 puntos; el ~1 % restante es CPU (nivel de optimización y prioridad de la tarea del AFE).

## Hallazgos colaterales

1. **`-O0` no se aplica.** `platformio.ini` pide `-g3 -O0` en `[mb_hw18plus]`, pero `build_type = debug` hace que PlatformIO compile todo (también la librería) con `-Og`. Para cambiarlo de verdad hay que usar `debug_build_flags`.
2. **PANIC a 1,4 s del arranque** en 18.55.0: `/debug/crash` guarda 6 informes iguales, "Reboots: 19 desde el último arranque en frío". No investigado.
3. **`WIFITBProvision` reintenta cada ~200 ms** cuando no hay servidor ThingsBoard (banco con `server: myURL`). Llena el ring de log del CrashReporter.
4. **Los CSV que ya están en Drive tienen huecos** (las mismas muestras perdidas), así que cualquier análisis hecho con ellos arrastra la base de tiempos errónea.

## Opciones de arreglo

| | Opción | Dónde | Coste | Comentario |
|---|---|---|---|---|
| A | La librería cuenta los DRDY en la ISR (IRAM) y compara con las muestras procesadas: expone `sample_index` y `samples_missed`, y rellena los huecos para que la base de tiempos de HR1/2/3 sea exacta | `incunest_afe4490` | Medio | Protege frente a **cualquier** causa, presente o futura (flash, WiFi, OTA, tareas nuevas). Es la defensa de fondo. |
| B | `DriveUpload` apagado por defecto; se activa expresamente para investigación (`/debug/mode`, atributo de ThingsBoard…) | IncuNest | Bajo | Quita la causa en uso clínico. Durante una captura el fallo sigue y el CSV sigue con huecos. |
| C | Enviar la captura por la red sin pasar por flash | IncuNest | Alto | Rediseño de `DriveUpload`; exige red estable y TLS a ~15 KB/s. |
| D | Ventana en PSRAM, escritura solo con la sonda retirada | IncuNest | Medio | Solo V19 (2 MB PSRAM). La V18 no tiene. |
| E | `-O2` + prioridad alta de la tarea del AFE | IncuNest | Bajo | Recupera el ~1 % restante. Complemento, no solución. |

Propuesta de Alex: **A + B**, y E como extra. B la podemos hacer ya en IncuNest si te parece bien (es tu módulo). A va en la librería.

## Detección permanente, sin simulador

Hoy nada avisa de que se pierden muestras. Con A, IncuNest podría:
- publicar `samples_missed` y la frecuencia medida en `/debug/state` y en telemetría (clave nueva en `telemetry_keys.h`);
- registrar un evento de diagnóstico si se pierde más de un umbral (p. ej. > 1 % en 10 s);
- añadir `sample_index` a cada fila del CSV de `DriveUpload`, para que quien analice sepa dónde hay huecos.

## Estado del árbol

2026-11-04: diagnóstico retirado; `feat/hmi-pulse-oximetry` = dev + 5 commits (3 de HMI + 6522041d + 3d821c9e), sin push de los dos últimos. La V18 del banco lleva la 18.56.0 limpia.
