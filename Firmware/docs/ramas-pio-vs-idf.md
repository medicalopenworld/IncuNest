# Dos líneas de desarrollo: PlatformIO e IDF

Desde el 2026-09-16 el repositorio lleva **dos líneas vivas en paralelo**. Saber
en cuál estás es lo primero que hay que mirar antes de tocar nada.

| línea | rama de integración | build | estado |
|---|---|---|---|
| **PlatformIO** (producción) | **`dev-pio`** | `pio run` | la que se fabrica y se mantiene |
| ESP-IDF (port) | `dev` | `idf.py build` | congelado, sin terminar |

`dev-pio` sale de **`627affc`** (2026-09-13), el último commit de `dev` anterior
a ESP-IDF, y de ahí en adelante es la línea que continúa.

## Cómo saber en cuál estás

```
ls Firmware/motherBoard/platformio.ini      # existe -> PlatformIO
ls Firmware/motherBoard/sdkconfig.defaults  # existe -> port IDF
```

Cosas que **no** sirven para distinguirlas, porque han engañado ya:

- **`platformio.ini` a secas**: la rama del port lo conservó en su punta hasta
  `c5e7b40`. Tener el fichero no significa estar en la línea PlatformIO.
- **Buscar el commit filtrando asuntos por "port"**: casa con "so**port**e".
- **`git log --first-parent`**: el trabajo PlatformIO del 13-sep entró en `dev`
  como **segundo padre** (la rama del port absorbió `dev` con `3ef8327` y luego
  se mergeó de vuelta con `65a79be` como primer padre). No aparece en la línea
  principal aunque sea ancestro de `dev`.
- **`Firmware/SensorBoard_v2/`**: es ESP-IDF nativo **desde siempre**, en las
  dos líneas. No indica nada sobre motherBoard/Display_HMI.
- **`Display_HMI/idf_component.yml`** y **`Display_HMI/CMakeLists.txt`**: el
  primero es del gestor de componentes (octubre de 2025, la plataforma
  `pioarduino` compila Arduino como componente de IDF); el segundo es un resto
  de SquareLine Studio (`add_library(ui …)`). Ninguno es el port.

## Flujo de trabajo en la línea PlatformIO

El gitflow del repo, con `dev-pio` en el papel de `dev`:

```
git checkout dev-pio
git checkout -b feat/<slug>
# ... commits atómicos por stage ...
git checkout dev-pio
git merge --no-ff feat/<slug> -m "merge: feat/<slug> -> dev-pio"
```

Entornos de compilación:

| placa | entorno | notas |
|---|---|---|
| motherBoard | `IncuNest_V18` | firmware que se distribuye |
| motherBoard | `IncuNest_V18_factory` | `+ -DFTEST_SIM_ACT_ENABLED=1`, activa SIM en el test de fábrica |
| motherBoard | `IncuNest_V17` / `_factory` | para HW17 |
| Display_HMI | `main` | |

## Qué hay de `dev` en esta línea, y qué no

El 2026-09-20 se portaron a `dev-pio` (rama `feat/portar-desde-dev`) **26 de
los 27 cambios de producto** que `dev` acumuló desde la base `627affc`,
cherry-pick a cherry-pick con `-x` (cada commit conserva su
`cherry picked from commit <sha>` y, cuando hubo que adaptar algo a Arduino, lo
dice en su propio mensaje). Entre ellos, los seis de seguridad clínica
(umbrales de alarma, histéresis, los tres defectos de alarmas de banco, la
recuperación del estado de control tras reinicio y la retirada de
`wipeBabies`), el modo depuración, el reloj RTC PCF8563 con su árbitro de
fuentes de hora, y el contador de bebés.

**No se portó**, a sabiendas:

- `e2e3ede` (retirar el encoder rotativo): refactor sin valor de producto cuyos
  tres conflictos eran fontanería del port, no el encoder.
- Lo que solo tiene sentido en IDF: `CONFIG_SPI_FLASH_AUTO_SUSPEND`, los dos
  parches al SDK de ThingsBoard vendorizado (`b2845f7`, `0549cef`), GPRS/OTA
  sobre `esp_modem` (`1a2eca7`, `2940b53`), el fix de caché/coredump del
  componente GT911 (`7a8463a`) y el knob del AFE4490 (`c442951`).

Adaptaciones que conviene conocer porque se notan al leer el código:

- La tabla de tareas del `/debug/state` de la motherBoard está detrás de
  `configUSE_TRACE_FACILITY`, que el core Arduino 2.0.14 no trae: el JSON lo
  dice en vez de omitirlo.
- Los contadores `gt911_*` del táctil viven en `lib/TAMC_GT911_Fixed`.
- `civil_time` pasó a `shared/` (como en el port); la motherBoard ya no lo
  lista en su `[env:native]` porque `pre_native.py` compila `shared/src`.
- El HMI tiene por primera vez un `[env:native]` (`pio test -e native` en
  `Display_HMI/`) con las tres suites del RTC; para ello la sección común de su
  `platformio.ini` pasó de `[env]` a `[common]`.

## Si se retoma el port

Lo que se arregle aquí **no viaja solo** a `dev`. Hoy están pendientes de
portar en sentido contrario los tres cambios propios de esta línea: botón TREND
retirado, mantenimiento desactivado por defecto y el cierre de la carrera de la
guarda de eco (`known_issues.md` #11).

Los issues abiertos que congelaron el port están documentados en la otra línea,
en `Firmware/docs/port-idf-issues-abiertos-2026-09-16.md` (rama
`feat/mb-heap-diag`), con la evidencia de banco fuera del repo en
`Documents/IncuNest_dev/bench-logs-2026-09-16/`.
