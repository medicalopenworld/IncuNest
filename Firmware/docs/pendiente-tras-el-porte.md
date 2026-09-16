# Trabajo y pruebas pendientes tras el porte a ESP-IDF

Estado a **2026-09-16**, con el porte ya mergeado en `dev` y validado en banco
sobre la unidad 353.

Este documento existe porque durante el porte se acumularon decisiones
aplazadas y verificaciones a medias en sitios distintos —comentarios de código,
mensajes de commit, propuestas de OpenSpec, notas de banco— y sin una lista
única es imposible saber qué falta de verdad.

**Cómo leerlo:** el orden es por riesgo, no por esfuerzo ni por área. Cada
entrada dice *qué falta*, *por qué importa* y *cómo se comprueba*. Lo que no
tiene una forma concreta de comprobarse no debería estar aquí.

---

## 1. Seguridad — lo único urgente

### 1.1 Las unidades en campo aceptan la contraseña filtrada

**Qué pasa.** El 2026-09-15 se rotaron `WEB_SERVER_PASSWORD` y el par de
provisioning de ThingsBoard, verificado en las dos placas del banco (credencial
nueva → 200, filtrada → 401). Pero **una unidad desplegada sigue aceptando la
vieja hasta que reciba firmware nuevo**. La rotación cierra lo que se fabrique o
actualice desde ahora; no lo que ya está fuera.

**Por qué importa.** Esa contraseña estuvo publicada en un repositorio público,
en texto plano y dentro de código fuente — no sólo en binarios. Con ella se
entra a `/config`, `/debug/*` y `/update`, que es **OTA**: se le puede meter
firmware a una incubadora. La guarda de placa impide el binario *equivocado*, no
el *malicioso*.

**Cómo se cierra.** Actualizar cada unidad. El flasher ya tiene el camino:
`INCUNEST_WEB_USER_LEGACY` / `INCUNEST_WEB_PASS_LEGACY` permiten alcanzar las
que aún llevan la credencial antigua.

**Comprobación:** por cada unidad, que la credencial nueva dé 200 y la vieja
401 contra `/debug/state`.

### 1.2 Los valores viejos siguen siendo recuperables

No se pueden borrar: están en el historial de git, en los objetos **Git LFS** y
en dos releases públicas. Por eso el remedio fue rotar. Se deja constancia para
que nadie intente "limpiarlo" y lo dé por resuelto.

> **Trampa al auditar esto:** los `.bin` del historial son **punteros LFS de
> ~132 bytes**, no binarios. Un `grep` sobre ellos no encuentra nada y parece
> que no hay fuga. El contenido real está en `.git/lfs/objects/`.

---

## 2. Validación de hardware que sigue abierta

### 2.1 `CONFIG_SPI_FLASH_AUTO_SUSPEND` en una segunda unidad

**Qué falta.** Está activado en las dos placas y validado **en una sola unidad**
de cada una (NVS íntegra tras reinicio + OTA completa de 1,94 MB).

**Por qué importa más de lo que parece.** Al comprobarlo se descubrió que **las
dos placas llevan flash de fabricantes distintos**: la motherBoard una XMC
`0x20 4017` y el display una GigaDevice `0xC8 4018`. Si varía entre placas del
mismo proyecto, puede variar entre unidades de producción — y esta opción
depende del chip, no del SoC.

**Cómo se comprueba, y es trivial:** mirar el arranque. IDF escribe siempre
`Flash suspend feature is enabled`; si el chip **no** lo soporta añade
`Suspend and resume may not supported for this flash model yet.`. Si aparece esa
segunda línea, esa unidad **no tiene la protección** aunque el firmware la pida.

Para saberlo antes de flashear: `esptool flash_id`, y para las XMC
`esptool read-flash-sfdp 0x32 1` — el bit 3 puesto significa serie D, que sí lo
soporta sin el `FORCE` que IDF marca como *"big risk"*.

### 2.2 Umbral de obstrucción del ventilador, calibrado sobre una sola placa

**Qué hay.** El umbral pasó de 190/175 a **220/200** con datos medidos: 97
muestras con el ventilador a 4006 rpm, calefactor a 255 y fototerapia al 82 %
dan un duty de trabajo de **187** (min 186, máx 188). Con 190 quedaban **dos
cuentas** de margen, y la retirada en 175 caía **por debajo** del punto de
trabajo — o sea que una vez declarada, la alarma **no podía retirarse** y dejaba
el calefactor cortado hasta reiniciar.

**Qué falta.** La dispersión entre unidades. Otro ventilador, otro conducto y
otra fototerapia darán otro duty.

**Red ya puesta:** un `static_assert` en `security.cpp` rompe la compilación si
alguien vuelve a dejar la retirada por debajo de `FAN_DUTY_NORMAL_MAX_OBSERVED`.

**Cómo se comprueba:** en cada unidad nueva, leer `fan.pid_out` de
`/debug/state` con el calefactor a tope y comprobar que queda holgadamente por
debajo de 200.

### 2.3 Rampa térmica con consigna 39 °C

Se hizo una y sirvió para encontrar tres cosas, pero **no se ha repetido** desde
que se ensanchó la histéresis del corte a 0,5 °C ni desde la recalibración del
ventilador. Quedan por observar el sobreimpulso con el umbral de desviación a
±1 °C y si el lado frío salta al abrir la puerta.

### 2.4 Periféricos sin batería de pruebas propia

Declarados como funcionando el 2026-09-16, pero **sin prueba automática**:
GPRS/SIM, táctil GT911, audio, humidificador, fototerapia y SensorBoard. La
batería de banco (17 pruebas) no los toca.

---

## 3. Defectos conocidos y acotados

### 3.1 El panel del HMI tiembla con cualquier transmisión WiFi

**Estado: acotado, no eliminado.** El *desplazamiento permanente* sí está
resuelto (`CONFIG_SPI_FLASH_AUTO_SUSPEND`). El temblor transitorio es inherente
al diseño —framebuffer en PSRAM y bounce buffer que se rellena desde ahí,
compitiendo con WiFi por ancho de banda— y **lo provoca cualquier transmisión,
incluida una petición a la página del webserver**.

Eso significa que ocurre en uso normal de mantenimiento, no sólo en condiciones
anómalas. **Merece entrada propia en `known_issues.md`** con ese disparador; hoy
consta como resuelto, que es engañoso.

Los fps **no** lo detectan: `LCD_DIAG` da 48 fps estables con la pantalla
temblando. Hay que mirar la pantalla.

### 3.2 La cadencia del PPG se degrada bajo carga

Medido con la instrumentación de la propia librería (`INCUNEST_PPG_TIMING=1`).
El ciclo es *trabajo por muestra*, no periodo; presupuesto a 500 Hz: 2000 µs.

| Escenario | Ciclo medio | Peor máximo | Cadencia |
| --- | --- | --- | --- |
| Sin sonda, red quieta | 687 µs (34 %) | 11 772 µs | 485,8 Hz |
| Sin sonda, 500 peticiones HTTP | 911 µs (46 %) | 25 658 µs | 448,9 Hz |
| **Con sonda**, red quieta | **1 574 µs (79 %)** | **32 961 µs** | **417,8 Hz** |

Con sonda el ciclo consume el 79 % del presupuesto y los cómputos asíncronos de
HR tardan **131–241 ms de media, hasta 832 ms de pico**. Cero `DRDY timeout`.

**Pendiente:** decidir si 500 Hz es sostenible o hay que bajar la tasa. Es
decisión del mantenedor de la librería, que tiene los datos en el PR #32.

> **Medir el PPG sin sonda no vale**: sin señal, `hr2_compute` y `hr3_compute`
> salen a 0 y no se mide la carga que importa.

---

## 4. Fase 1 del blindaje de la capa de plataforma

Decidido en `shared-retirar-shim-arduino`: **separar y blindar, no retirar**.
La separación está hecha (cuatro componentes). Faltan dos tests de contrato.

| Capa | Estado | Qué fija |
| --- | --- | --- |
| `plat_fs` | **hecho** | `name()` pelado, prefijo de montaje, `remove()` sobre ruta compuesta |
| `plat_nvs` | **hecho** | formato **BLOB** de `putFloat`/`putDouble`, verificado con `getBytesLength()` |
| `plat_i2c` | pendiente | que el bloqueo abarque la transacción con repeated-start y que dos tareas no se entrelacen |
| `plat_net_client` | pendiente | que `available()` refleje los bytes realmente pendientes y que el `timeout` se aplique |

**La regla que hace útiles estos tests:** cada uno debe **fallar** si se le
reintroduce el defecto histórico. Los dos hechos están verificados así — con el
defecto inyectado dan 2/5 y 3/7 en rojo. Un test que pasa con el bug puesto no
sirve.

Y el matiz que justifica el esfuerzo: **no basta con probar ida y vuelta**. Un
cambio de formato hecho de forma *consistente* en `put` y `get` pasaría
cualquier round-trip y aun así dejaría sin datos a las unidades en campo. Sólo
la comprobación del formato físico lo caza.

### 4.1 Prueba de NVS virgen — destructiva, necesita una placa de sacrificio

Falta cubrir el camino de **unidad nueva / NVS corrupta**: partición virgen →
`nvs_flash_init()` devuelve `NO_FREE_PAGES` → `erase` → `init` → `initEEPROM`
carga defaults → el flasher repone el serial.

**No se puede hacer en la unidad de referencia.** Un `nvs_flash_erase()` borra
la partición entera, y con ella:

- **el número de serie, irrecuperable** — el rescate de `initEEPROM()` lee
  `KEY_SERIAL` antes de `resetFlash()`, pero eso sólo salva una limpieza por
  API, no un borrado de partición. Lo escribe el flasher;
- **las credenciales WiFi** (`mb_wifi`), o sea que la placa no vuelve a la red.

Tampoco se puede aislar en una partición aparte: la tabla de 8 MB **no tiene un
solo byte libre**.

**Su sitio:** una placa que vaya a pasar por el flasher de todas formas.

---

## 5. Herramientas y marco de trabajo

### 5.1 `Firmware/.claude/` da instrucciones que fallan

**12 ficheros** siguen mandando `pio run` / `pio test` — incluido el hook
`run-affected-tests.sh` y cinco prompts de agentes. PlatformIO ya no existe en
el repositorio: esos comandos fallan. Cualquier sesión que siga esas reglas
tropieza.

Está en `.gitignore`, así que hay que tocarlo a mano y no entra en ningún
commit.

### 5.2 Los guards no actúan en modo bypass

`guard-merge` no saltó al mergear y `guard-push` no saltó al empujar a `dev`.
Existen pero en ese modo no se ejecutan. Es una decisión a tomar, no un parche:
o se asume que son orientativos, o se buscan gates que no dependan del modo
(por ejemplo, protección de rama en el servidor).

### 5.3 Un worktree por sesión

Acordado el 2026-09-16 después de que dos sesiones sobre el mismo worktree
hicieran que unos commits cayeran en la rama de feature de otra sesión **sin
ningún error visible** — `git push origin dev` respondía *"Everything
up-to-date"*.

`dev` queda **sin checkout permanente**: es la rama de integración y se usa con
un worktree temporal.

> **Al renombrar un worktree de ESP-IDF:** `git worktree move` conserva los
> ficheros pero **CMake graba rutas absolutas**, así que la caché de build queda
> invalidada — y `idf.py fullclean` tampoco funciona en ese estado. Hay que
> borrar los `build/` a mano y reconstruir (~10 min las dos placas).

---

## 6. Decisiones aplazadas, con su motivo

| Cambio OpenSpec | Estado |
| --- | --- |
| `shared-retirar-shim-arduino` | Fase 0 **decidida**; quedan dos tests de la Fase 1 |
| `mb-air-overtemp-override` | Aplazado: **exige un segundo corte a 40 °C que este hardware no tiene** |
| `shared-cascade-ota-distribution` | Aplazado: retiraría el cliente de ThingsBoard del display |
| `shared-ota-imagen-ajena-recordada` | Aplazado: bucle de re-descarga de OTA |
| `mb-onomondo-key-in-nvs` | Sin implementar |
| `mb-sim-auto-deactivation` | Sin implementar |
| `shared-baby-profile-nte-wizard` | Sin implementar (además **no valida**: le faltan deltas en `specs/`) |
| `hmi-thingsboard-sdk-bump` / `mb-thingsboard-sdk-bump` | Sin implementar |

### La desviación normativa que hay que tener presente

`ALARM_AIR_CUTOUT_MAX_C` está en **40 °C** y 201.15.4.2.1 aa) fija **38 °C**.
Es una decisión de producto consciente del 2026-09-14, tomada para poder subir
la consigna de aire a 39 °C. **No se puede reclamar conformidad** mientras esto
esté puesto.

El camino conforme está analizado y a medio hacer: exige un **override** con
gesto deliberado e indicación permanente, y un **segundo corte a 40 °C en canal
independiente del termostato**.

> **Dato útil y poco conocido:** ese canal independiente **existe a medias**. El
> SHTC3 es un chip distinto del STS35 que alimenta el PID, ya se lee y se criba
> en `sensors_module.cpp`, pero sólo se publica como telemetría
> `Air_temp_redundant` y **no gobierna nada**. Engancharle el corte del override
> es el siguiente paso real. Dos límites: en unidades con SensorBoard no se
> rellena (allí se funden tres lecturas con mediana), y si el STS35 falla el
> SHTC3 pasa a primario y la redundancia desaparece.

---

## 7. Lo que sí está cerrado

Para no volver sobre ello:

- Porte de las dos placas a ESP-IDF 6, mergeado en `dev`.
- **Cuatro regresiones del porte** que fallaban en silencio: el mutex de
  `TwoWire`, `available()` sin `LWIP_SO_RCVBUF`, la doble excepción del volcador
  de coredump y `FsFile::name()` devolviendo la ruta.
- Consigna de aire a 39 °C y corte a 40 °C, con consigna y corte **desacoplados**
  (eran la misma constante).
- El OTA del display, que preguntaba una vez y se quedaba enganchado.
- Dos caminos que tumbaban la placa con la sonda puesta: una línea de log que
  agotaba el heap y `DriveUpload` llenando el sistema de ficheros.
- Los logs apagados dejan de construir su cadena: **−52 KB de binario**.
- Rotación de credenciales, verificada en las dos placas.
- Limpieza de PlatformIO del repositorio.
