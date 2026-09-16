# incunest_afe4490: de copia vendorizada a submódulo en v0.92

- **Fecha:** 2026-09-16
- **Rama destino:** `refactor/idf-native-port`
- **Ámbito:** solo el port de ESP-IDF (`Firmware/.worktrees/idf-native-port/`)
- **Estado:** diseño aprobado, pendiente de plan de implementación

---

## 1. Qué problema resuelve

Al migrar de PlatformIO/Arduino a ESP-IDF se copió la librería `incunest_afe4490`
dentro del port, en `Firmware/components/incunest_afe4490/`, pineada al commit
`4e0dd91` (tag `v0.81`) y **parcheada a mano**: ~28 puntos de llamada marcados
con `PARCHE INCUNEST` que sustituyen `<Arduino.h>` y `<SPI.h>` por la capa
`incunest_platform`, más un `CMakeLists.txt` propio con siete `-Wno-error=`.

Esa copia tiene tres costes:

1. **Es un fork silencioso.** La librería vive en otro repositorio con otro
   responsable. Cada vez que upstream se mueva hay que rehacer los parches a
   mano, y no hay forma barata de ver qué cambió.
2. **Los parches ya no hacen falta.** Upstream `v0.91` introdujo un HAL
   (`incunest_afe4490_hal_idf.cpp`) que hace exactamente lo mismo que los
   parches, y `v0.92` trae un `CMakeLists.txt` en la raíz cuya cabecera dice
   literalmente *"add it as a git submodule under components/"*. La librería ya
   está diseñada para este caso de uso.
3. **El port se quedó en v0.81.** Hay 23 commits upstream sin incorporar, entre
   ellos correcciones a los algoritmos de HR y de detección de presencia.

## 2. Qué se decide aquí

- Sustituir la copia por un **submódulo de git** en `Firmware/components/incunest_afe4490`,
  pineado en `74f8070a4665b26c89585e8287928ecc2b24c8f1` (= `v0.92^{}`).
- Hacer el **salto de versión y el reempaquetado en el mismo cambio**
  (decisión explícita del responsable; ver §8 "Alternativas descartadas").
- **No tocar** `Display_HMI` ni el `motherBoard` legacy de la rama `dev`.

## 3. Alternativa descartada: el Component Manager de IDF

ESP-IDF permite declarar la dependencia en un `idf_component.yml` con una fuente
`git:` pineada al tag, sin `.gitmodules`.

Se descarta porque deja el código **fuera del árbol**, en `managed_components/`,
que va a `.gitignore`. El motivo entero por el que esta librería está pineada
—escrito en `platformio.ini`— es que *"vive en otro repositorio con otro
responsable"*, y lo que se necesita es poder **leer y diffear** qué cambió
cuando upstream se mueva. El Component Manager optimiza justo lo contrario.

La distinción importa porque el port **ya usa** el Component Manager para
`espressif__mdns`, `espressif__esp_modem`, `espressif__mqtt` y
`joltwallet__littlefs`. Esas son dependencias de terceros estables que nadie
diffea nunca: el Component Manager es la herramienta correcta para ellas. El
AFE4490 es el caso opuesto —código propio de la organización, en evolución
activa, del que este firmware es consumidor crítico— y por eso va al árbol.
No es una incoherencia, es la línea entre los dos tipos de dependencia.

Detalle del pin: con submódulo el pin real es el **SHA** que queda registrado en
el árbol, no el tag. `v0.92` es solo la etiqueta con la que lo nombramos; un tag
se puede mover, un gitlink no. Eso es más fuerte que el `#4e0dd91` de PlatformIO,
no menos.

## 4. Diseño

### 4.1 La sustitución en el build

Se borra `Firmware/components/incunest_afe4490/` entero (`CMakeLists.txt`,
`library.json`, `README.md`, `src/*`) y el submódulo ocupa **esa misma ruta**.

Como la raíz del repo upstream *es* el componente IDF, esto no propaga cambios:

| Fichero | Cambio |
|---|---|
| `motherBoard/CMakeLists.txt` → `EXTRA_COMPONENT_DIRS` | **ninguno** (ya apunta al directorio) |
| `motherBoard/main/CMakeLists.txt` → `REQUIRES incunest_afe4490` | **ninguno** |
| `SPO2.h`, `DriveUpload.h`, `PpgSnapshot.h` → `#include "incunest_afe4490.h"` | **ninguno** (upstream declara `INCLUDE_DIRS "."`) |

Lo que sí cambia, dentro del componente y por tanto gratis:

- `REQUIRES`: `incunest_platform esp_timer` → `esp_driver_gpio esp_driver_spi esp_timer`.
  El AFE deja de depender del shim de Arduino.
- Se van los siete `-Wno-error=`. `v0.90b` dejó la librería limpia bajo
  `-Wall -Wextra -Werror` y su propio `CMakeLists.txt` impone esa política en
  `PRIVATE`.
- Desaparecen los 4 marcadores `PARCHE INCUNEST` y
  `incunest_afe4490_platform_stub.h`. El HAL se elige **por detección**
  (`ESP_PLATFORM && !ARDUINO`), sin macro que poner a mano.

### 4.2 El knob `INCUNEST_PPG_TIMING` — el punto delicado

Hoy lo implementa el `CMakeLists.txt` *local* del componente, con un `PUBLIC`
cuya justificación está escrita ahí y sigue siendo válida:

> `#if INCUNEST_TIMING_STATS` **añade miembros a la clase `INCUNEST_AFE4490`**, así
> que cambia su tamaño. La cabecera la incluyen también `SPO2.h`, `DriveUpload.h`
> y `PpgSnapshot.h`. Si el define llegase solo al componente, cada lado vería una
> clase distinta y el resultado sería corrupción de memoria silenciosa, no un
> error de enlazado.

Con submódulo ese fichero deja de ser nuestro. El knob se reubica a
`motherBoard/CMakeLists.txt`, **antes de `project()`**:

```cmake
if(DEFINED ENV{INCUNEST_PPG_TIMING})
  message(WARNING "INCUNEST_PPG_TIMING: instrumentacion de tiempos del AFE4490 "
                  "ACTIVADA. Binario de diagnostico, no de produccion.")
  idf_build_set_property(COMPILE_OPTIONS "-DINCUNEST_TIMING_STATS=1" APPEND)
endif()
```

`idf_build_set_property(... APPEND)` alcanza a **todos** los componentes del
build. Es estrictamente más seguro que el `PUBLIC` que sustituye: el define no
puede saltarse ninguna unidad de traducción.

Esto deja de ser un extra. `v0.92` añade `spi_mean,spi_max` a la trama `$TIMING`,
que es exactamente la medida que `components/incunest_platform/include/platform/plat_spi.h`
deja marcada como **pendiente de banco**. El knob pasa a ser la herramienta que
cierra esa duda abierta del port.

Efecto lateral a documentar: upstream saca `$TIMING` por `printf` crudo, no por
`ESP_LOGx` — su propio comentario llama a eso *"the mistake the vendored copy in
IncuNest made"*. Las tramas pierden el prefijo `I (ms) tag:`. Es una mejora: el
protocolo de línea `$...*XX\r\n` vuelve a estar limpio.

### 4.3 SPI: qué se queda y qué cambia de significado

`initSPO2()` en `SPO2.cpp:71` llama `SPI.begin(AFE_SCK, AFE_MISO, AFE_MOSI, -1)`,
que en `plat_spi.cpp:27` hace `spi_bus_initialize(SPI2_HOST, ..., SPI_DMA_DISABLED)`,
nueve líneas antes de `afe.begin(AFE44XX_CS, AFE_ADC_READY)`.

**Eso es justo lo que v0.92 exige y ya está en el orden correcto.** El HAL usa
`SPI2_HOST` por defecto (`INCUNEST_AFE4490_HAL_SPI_HOST`, sobrescribible por
`-D`) y solo hace `spi_bus_add_device` con `spics_io_num = -1`, porque la
librería sigue gobernando el CS por GPIO como siempre.

La llamada se queda, pero **su significado cambia y los comentarios mienten**:

- El docblock de `plat_spi.h` dice *"el único consumidor es la librería del
  AFE4490"*. Deja de ser cierto. En `motherBoard` ya no hay ningún consumidor
  SPI de `plat_spi` (`adafruit_busio` incluye la cabecera pero nadie la usa por
  SPI), así que `plat_spi` pasa a existir **solo para levantar el bus**.
- **No se borra en esta rama**: la retirada de `plat_spi` es una decisión de
  arquitectura aparte, fuera del alcance. Hoy el resto de la clase
  (beginTransaction / transfer / endTransaction) se queda sin ningún consumidor
  en todo el repo, y es estado que merece documentarse honestamente.

Consideraciones técnicas verificadas:

- Dos devices en SPI2 (el `dev_` perezoso de `plat_spi` y el del HAL) es normal y
  soportado por `spi_master`.
- `max_transfer_sz = 64` y `SPI_DMA_DISABLED` siguen valiendo: el HAL usa
  `SPI_TRANS_USE_TXDATA/RXDATA`, tramas de 4 bytes inline, sin DMA.
- DRDY: el HAL hace `gpio_install_isr_service(0)` tolerando
  `ESP_ERR_INVALID_STATE`. En el port no hay ningún otro
  `gpio_install_isr_service`, así que no hay conflicto.

### 4.4 Contrato de datos: lo que NO rompe

`AFE4490Data` es **idéntico campo a campo** entre la copia local y `v0.92`:
mismo orden, mismos tipos. El `static_assert` de `SPO2.h` sobre
`AFE4490Data::ppg_disp == float` sigue valiendo y debe quedarse.

`v0.90` renombró y partió `ProbeState`:

| Valor | v0.81 (copia local) | v0.92 |
|---|---|---|
| 0 | `PROBE_DISCONNECTED` | `PROBE_DISCONNECTED` |
| 1 | `PROBE_NOT_APPLIED` | `PROBE_OT_HIGH` |
| 2 | `PROBE_APPLIED` | `PROBE_APPLIED` |
| 3 | `PROBE_SATURATING` | `PROBE_AMB_SATURATING` |
| 4 | — | `PROBE_ONLY_LED_SATURATING` (nuevo) |

**El port no rompe a compilar**: solo usa `PROBE_APPLIED` y `PROBE_DISCONNECTED`
(`SPO2.cpp:20`, `CommTask.cpp:1186/1231/1265/1409`, `DriveUpload.cpp:572`,
`GPRS.cpp:179`, `factory_test_hw.cpp:648/650`). Ninguno de los nombres
renombrados aparece.

`v0.92` añade `isProbeAbsent(ProbeState)` como el único sitio que define "no hay
paciente en la sonda". El port no lo necesita hoy, pero es la API a usar si
alguna vez hay que distinguir "ausente" de "aplicado" en más de un sitio.

## 5. Deuda conocida que este cambio crea (y no resuelve)

**El valor 4 sale por el cable y el HMI lo pierde.** La motherBoard emite
`(int)probe_state` en las tramas. `Display_HMI/src/tasks/CommTask.cpp:701`
valida `state < SPO2_PROBE_DISCONNECTED || state > SPO2_PROBE_SATURATING`
(rango 0..3) y colapsa lo que sobra a `SPO2_PROBE_NOT_APPLIED`.

Un `4` (`PROBE_ONLY_LED_SATURATING`) degradaría a `NOT_APPLIED`. Como ambos son
estados "sonda ausente" según `isProbeAbsent()`, **no hay error clínico**, pero
sí pérdida de información diagnóstica. Además, los nombres del HMI
(`SPO2_PROBE_NOT_APPLIED`, `SPO2_PROBE_SATURATING` en `CommTask.h:145-151`)
quedan mintiendo respecto a la librería que fija el contrato numérico — cosa que
el propio `CommTask.h:140` documenta.

**Se deja fuera a propósito**: `Display_HMI` vive en `dev`, no en el worktree del
port. Merece su propia propuesta OpenSpec, alimentada por lo que se observe en el
punto 4 de la verificación de banco.

**Divergencia entre ramas.** El `motherBoard` legacy de `dev` sigue pineado a
`#4e0dd91` (v0.81) en `platformio.ini`. No se bumpea: la unidad de banco corre el
port, y meter 23 commits de algoritmo en el firmware legacy sería riesgo clínico
sin beneficio. Queda registrado aquí para que sea una decisión, no un olvido.

## 6. Clon limpio y CI

Sería **el primer submódulo del repo** (`git config -f .gitmodules --list`
devuelve vacío hoy). Un `git clone` sin `--recursive` deja el directorio vacío y
el build falla con un *"component not found"* que no explica nada.

- **Mitigación en el build**: guard en `motherBoard/CMakeLists.txt` que compruebe
  que el `CMakeLists.txt` del submódulo existe y, si no, falle con
  `FATAL_ERROR` diciendo el comando exacto que lo arregla.
- **CI**: `release.yml` solo hace `pio run -e IncuNest_V17` y `pio run -e main`,
  las dos apps de PlatformIO. **Nunca construye el port**, así que hoy no rompe
  nada. Cuando el port entre en CI hará falta `submodules: recursive` en el
  `actions/checkout@v4`. Se documenta, no se cambia ahora.
- **README del port**: `git submodule update --init --recursive`.

## 7. Verificación

Compilar es barato y aquí prueba poco. La condición de cierre es banco.

### 7.1 Build

`idf.py build` de `motherBoard` limpio, sin warnings nuevos. Verificar también
que `idf.py build` con `INCUNEST_PPG_TIMING=1` compila y emite el `message(WARNING)`.

### 7.2 Banco (condición de cierre, no opcional)

1. **Tiempos.** Con `INCUNEST_PPG_TIMING=1`, leer la trama `$TIMING`: el campo
   CICLO medio debe rondar los **2000 µs a 500 Hz**. Y leer `spi_mean/spi_max`,
   nuevos en v0.92 — esto cierra la duda que `plat_spi.h` dejó abierta sobre el
   coste por byte de `spi_device_polling_transmit()`.
2. **Señal viva.** Traza PPG no plana (`ppg_disp` sigue siendo `float`) y
   `probe_state` transitando `DISCONNECTED → APPLIED` al aplicar la sonda.
3. **HR y SpO2 contra la unidad legacy.** *Aquí es donde asomarían los 23 commits
   de algoritmo*, y es el riesgo real de este cambio:
   - `v0.86b`: HR1 media móvil 64 → 160 taps, con un clamp que antes truncaba en
     silencio.
   - `v0.89`: presencia por OT pasa de OR a AND en los dos canales.
   - `v0.88`: la puerta SQI *"reverses both HR1 conclusions"*.
   - `v0.85`/`v0.87`: catálogo cerrado de PRF, parámetros temporales en segundos.
4. **El valor 4.** Observar si `probe_state` llega a valer `4` alguna vez en uso
   real y qué hace el HMI con él. Alimenta la propuesta de §5.

## 8. Alternativas descartadas

- **Component Manager de IDF** en vez de submódulo → §3.
- **Dos pasos (v0.91 y luego v0.92).** `v0.91` ya trae el HAL completo, así que
  el paso 1 habría sido reempaquetado puro con el mismo código de señal que corre
  hoy, comparable byte a byte, y el paso 2 solo el bump de algoritmos. Descartado
  por el responsable a favor de un único cambio; el coste aceptado es que si el
  banco da lecturas raras no se distingue si viene del HAL o de los algoritmos.
  **Si eso pasa, el fallback es exactamente esta separación**: repinear el
  submódulo en `v0.91` y volver a medir.
- **Submódulo pineado en v0.81.** Inviable: los parches son ediciones al código
  de la librería y un submódulo es de solo lectura desde aquí. Exigiría forkear.

## 9. Notas de ejecución

El worktree `refactor/idf-native-port` tenía 11 ficheros modificados sin
commitear (el trabajo en curso del 2G: `thingsboard/`, `GPRS.cpp`,
`Wifi_OTA.cpp`) cuando se escribió esta spec. Antes de crear rama hay que
confirmar que ese trabajo está commiteado o guardado, o un `checkout -b` lo
arrastrará.
