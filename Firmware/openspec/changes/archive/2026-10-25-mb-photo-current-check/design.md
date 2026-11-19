# Diseño

## Contexto

`CTRL,STATE` es la única fuente de la que el display ADOPTA el estado de la placa (`Display_ApplyCtrlState`, a 1 Hz). El display protege su orden local con una guarda de confirmación (`localCmdHoldsLocal`, 10 s): mantiene lo que el operador acaba de pedir hasta que la placa lo devuelve igual. Eso es lo que hace seguro emitir el valor real: entre que el operador toca el switch y la placa aplica la orden pasan decenas de milisegundos, y la guarda cubre esa ventana.

`photo_override.h` ya asume que la placa emite el valor real: el display «adopta» lo que emite la placa y lo devuelve en su keepalive. Con el eco actual (`g_last_cmd.phototherapyMode`, que durante un override de depuración vale lo que dijo el display, no lo aplicado) el display nunca llegaba a enterarse del encendido forzado.

## Decisiones

**D1 — Campo 8 = `in3.phototherapy`.** Es el valor que `main.cpp` calcula con `debug_photo_effective()` y es lo que gobierna el PWM. Un restore tras caída (`EEPROM.cpp` carga `in3.phototherapy` de NVS) también pasa a ser visible para un display que arranca, que es la intención del known issue nº 4 («un reinicio del HMI hereda el estado real»).

*Comprobación del override de depuración con el estado real emitido* (máquina de `photo_override`):

1. `start`: `effective()` fuerza ON → `in3.phototherapy=1` → la placa emite 1 → el display lo adopta y devuelve 1 en su keepalive. `may_persist()` es falso: nada de eso llega a NVS.
2. `release`: abre la ventana de gracia; `effective()` devuelve `real_before` (0) e ignora el 1 rancio del display → `in3.phototherapy=0` → la placa emite 0 → el display adopta 0 y deja de devolver 1.
3. Pasada la gracia (5 s, mayor que la vuelta emisión → adopción → keepalive de 1-2 s) manda otra vez el display, que ya dice 0.

Con el eco antiguo el paso 2 era el único que protegía; ahora además el display converge solo. La máquina no necesita cambios y `test_photo_override` sigue valiendo tal cual: es lógica pura, no depende de qué emite `CommTask`. La parte de «qué se emite» no es testeable en host (`CommTask.cpp` toca UART/FreeRTOS): **verificación manual**.

**D2 — Los demás ecos no se corrigen.**
- `actuation` y `controlMode`: `in3.actuation` / `in3.controlMode` se asignan literalmente desde el comando (`main.cpp`), así que su valor «real» es idéntico al eco en operación normal. Cambiarlos sería ruido sin efecto, y la ventana tras un restore de NVS (donde sí podrían diferir) es la misma discusión del known issue nº 4, con su propia guarda en el display.
- `desiredAirTemperature` / `desiredSkinTemperature`: la placa guarda **una sola** consigna (`in3.desiredControlTemperature`, según el modo). Emitir ahí «lo real» obligaría a inventar el valor de la consigna del modo inactivo. Semántica dudosa: se deja y se anota.
- `desiredHumidity`: `in3.desiredControlHumidity` solo se actualiza con la humedad activa; mismo problema.
- `skinModeEnabled`: no existe equivalente en `in3`.

**D3 — Umbral `PHOTO_FAULT_MIN_A` = 0,05 A.** El objetivo del lazo es 0,45 A (`PHOTO_TARGET_CURRENT`) y la semilla más baja posible, `PHOTO_MIN_PWM` = 10, ya da corriente medible: en banco (2026-09-23) una lectura al 10 % de PWM fue 0,10 A. 0,05 A queda por debajo de cualquier punto de trabajo legítimo —incluida la mínima intensidad que el lazo puede pedir— y por encima del ruido/offset de la medida de un canal sin carga. Lo que se busca no es detectar una lámpara degradada, sino una que **no responde**.

**D4 — Histéresis `PHOTO_FAULT_CLEAR_A` = 0,08 A.** Declarar a < 0,05 y retirar a ≥ 0,08. Sin ella, una lámpara con corriente alrededor del umbral haría parpadear la alarma (known issue nº 1).

**D5 — `PHOTO_FAULT_CYCLES` = 5 ciclos del lazo de 1 s (5 s).** Cubre un transitorio de arranque o un único muestreo malo del INA sin retrasar el aviso a más de unos segundos tras `PHOTO_SETTLE_MS`. Un ciclo bueno (corriente ≥ umbral) reinicia la cuenta: tienen que ser consecutivos.

**D6 — Cuándo se evalúa.** El llamador (`sensors_module.cpp`) calcula `evaluable = !g_factoryTestActive && in3.phototherapy && (millis() - in3.photoTurnOnTime > PHOTO_SETTLE_MS) && sensor MAIN presente y respondiendo`. Si no es evaluable, el módulo **reinicia el contador y retira la condición**: no se puede afirmar que la lámpara falle si no se la ordenó, si el banco de fábrica la tiene en estado seguro, o si el sensor no contesta (esto último es un fallo del sensor, y ya hay una condición de sensor de corriente para el calefactor; para el canal MAIN se deja fuera de alcance).

**D7 — Prioridad BAJA, no latching, sin corte.** Una lámpara sin corriente no daña al neonato: es una terapia que no se está administrando. Pero el operador tiene que saberlo; por eso es alarma y no log. La sensibilidad clínica (que BAJA sea suficiente) queda como duda registrada para el análisis de riesgos: `alarms_normative_analysis.md` no deriva esta condición.

**D8 — `ALARM_PHOTO_LAMP_FAULT` = 20, al final del enum.** El valor es el índice de bit del bitmask del protocolo (`alarmBitmask` es de 32 bits: el bit 20 cabe). Insertar en medio rompería la compatibilidad con un display sin actualizar. Ninguna constante del HMI está dimensionada por un `ALARM_COUNT` fijo (`MAX_ALARMS = ALARM_COUNT`), pero hay que **compilar** el HMI para comprobarlo.

**D9 — Módulo puro `photo_fault`.** La firma es una struct de estado y una función de actualización por ciclo; el reloj y la lectura son del llamador. Así el contador, el umbral y la histéresis se prueban en host (`test_photo_fault`) y lo único sin cobertura es el cableado de dos líneas.

## Riesgos

- **Carrera al encender**: entre la orden del display y la aplicación, un `CTRL,STATE` sale con 0. La guarda local del display (10 s) lo cubre; si un display antiguo no la tuviera, vería un parpadeo del switch. Verificación manual en banco.
- **Lámpara apagada por el temporizador**: `getRemainingPhotoTime()` pone `in3.phototherapy=false` y `g_last_cmd.phototherapyMode=0` a la vez; con el estado real la emisión es coherente en ambos casos.
- **Efecto secundario del override**: el display adopta el ON emitido y lo devuelve como orden suya, así que `CommTask` (contabilidad de exposición por bebé, `babyStore_setActiveSeq`) ve la sesión de depuración como una orden real. Sin bebé activo en el asistente (banco) no acredita nada; con él, sí. No se mitiga aquí (fuera de alcance, tocaría el camino de órdenes del display); queda anotado en `known_issues.md` §20.
