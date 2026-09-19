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

## Si se retoma el port

Lo que se arregle aquí **no viaja solo** a `dev`. Hoy están pendientes de
portar los tres cambios de esta rama: botón TREND retirado, mantenimiento
desactivado por defecto y el cierre de la carrera de la guarda de eco
(`known_issues.md` #11).

Los issues abiertos que congelaron el port están documentados en la otra línea,
en `Firmware/docs/port-idf-issues-abiertos-2026-09-16.md` (rama
`feat/mb-heap-diag`), con la evidencia de banco fuera del repo en
`Documents/IncuNest_dev/bench-logs-2026-09-16/`.
