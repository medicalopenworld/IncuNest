---
name: tdd-cycle
description: El ciclo TDD red-green-refactor con Unity/idf.py en el firmware SensorBoard. Úsalo al implementar cualquier feature o bugfix, antes de escribir código de producción, y cuando haya dudas sobre dónde colocar tests, qué comando usar o cómo verificar en compile-only vs. hardware real.
---

# Ciclo TDD en SensorBoard (Unity + ESP-IDF)

## Red → Green → Refactor

1. **Red**: escribe un `TEST_CASE` en `test_apps/<component>_test/main/test_main.c` que describa el
   comportamiento deseado y falle — o bien porque no compila (símbolo/función que aún no existe) o
   bien porque la aserción falla en ejecución. Confirma que falla por la razón correcta antes de
   tocar producción.
2. **Green**: el mínimo código en el componente (`components/<component>/`) para pasar el test.
   Nada de adelantar funcionalidad sin test. Para código de lógica pura (cálculo de CRC16-CCITT,
   máquina de estados del framing, parseo) `idf.py build` ya detecta errores de compilación antes
   de necesitar hardware — úsalo como primer chequeo rápido.
3. **Refactor**: con los tests en verde, mejora nombres/duplicación/límites de componente (ver
   skill `arch-embedded-layering`) y vuelve a verificar con otro flasheo/monitor.

## Dónde y con qué

| Tipo de código                                            | Ubicación del test                                            | Cómo se verifica                                                                                     |
| ---------------------------------------------------------- | ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| Lógica pura (CRC16-CCITT, framing, parseo, cálculos)       | `TEST_CASE` en `test_apps/<component>_test/main/test_main.c`   | `idf.py build` primero (compile-only, automatizable); luego `idf.py -p COMx flash monitor` para ver el `TEST_CASE` pasar en el target |
| Drivers de sensor / hardware (I2C, I2S, GPIO+ISR, cámara)  | mismo patrón, en `test_apps/<component>_test/`                 | requiere el ESP32-S3 físico conectado — solo `flash monitor` manual, nunca automatizado                |
| Integración `usb_comm` (framing end-to-end)                | `test_apps/usb_comm_test/main/test_main.c`                     | `idf.py -p COMx flash monitor`, verificando además que las tareas RX/TX previas siguen en verde        |

> No hay Vitest/Jest ni entorno "node" de test: todo Unity corre embebido en el target (o, si el
> componente lo permite, en el host vía `idf.py --preview qemu` — excepción, no la norma en este
> proyecto). El compile-only (`idf.py build`) es lo único automatizable sin hardware; la ejecución
> real siempre es manual.

## Comandos

```bash
idf.py build                                                  # compila todo — gate automatizable, detecta errores de tipos/enlazado
idf.py -p COMx flash monitor                                  # flashea y muestra el output de Unity en el target (manual, requiere placa)
idf.py -C test_apps/<component>_test build                    # compila solo el test_app de un componente
idf.py -C test_apps/<component>_test -p COMx flash monitor    # flashea y corre ese test_app en concreto
```

## Convenciones

- Tests Unity agrupados por componente en `test_apps/<component>_test/main/test_main.c`, no
  colocados 1:1 junto a cada `.c` (a diferencia de un monorepo TS) — es el patrón estándar de
  ESP-IDF (componentes con su propio `test_apps/`).
- Nombres de `TEST_CASE` descriptivos del comportamiento.
- Ningún `TEST_CASE` de hardware real se ejecuta en un hook automatizado (ver `run-affected-tests.sh`
  y la decisión de diseño "verificación compile-only, nunca auto-flash"): el desarrollador corre
  `flash monitor` él mismo cuando el cambio lo requiere.
- No hay umbral numérico de cobertura automatizado en este stack: la evidencia de "suficientemente
  testeado" es que cada escenario de la spec (ver `spec-driven-development`) tiene un `TEST_CASE`
  correspondiente y que este pasa en el target, no un porcentaje de líneas.
