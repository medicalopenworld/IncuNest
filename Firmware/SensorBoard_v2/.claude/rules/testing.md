---
paths:
  - "test_apps/**"
  - "components/**/*.c"
  - "components/**/*.h"
---

# Reglas de testing

- TDD: el test se escribe antes que la implementación y debe fallar primero por la razón correcta (compilación rota o assert que falla, no un test vacío).
- Único framework de test: **Unity**, el que trae ESP-IDF de serie (`idf_component_register` sobre el componente `unity`). No hay framework de test on-host/nativo — todo test corre on-target, sobre el ESP32-S3 real.
- Los tests viven en `test_apps/<componente>_test/main/test_main.c`, agrupados **por componente**, no un archivo de test por cada `.c` fuente. Ej.: los tests de `components/usb_comm/` viven en `test_apps/comm_test/main/test_main.c`, cubriendo CRC16, framing, protocolo, etc. en el mismo archivo.
- Estructura de cada test: `TEST_CASE("descripción del comportamiento", "[tag]")`. La descripción es la especificación en prosa; el tag agrupa por área (`[crc16]`, `[frame]`, `[sensor]`, ...) y permite filtrar con `idf.py -p COMx flash monitor -T <tag>` si hace falta.
- Nombres de test descriptivos del comportamiento esperado ("CRC16 known vector: '123456789' == 0x29B1"), no de la implementación interna.
- Ciclo real de ejecución:
  1. `idf.py build` — compila. Este paso sí es automatizable y es lo que ejecuta el hook de verificación (`run-affected-tests.sh`) al cerrar un turno con `.c`/`.h` sin commitear.
  2. `idf.py -p COMx flash monitor` — flashea el `test_apps/<componente>_test` al ESP32-S3 físico y ejecuta los `TEST_CASE` on-target vía el runner de Unity. Este paso es **siempre manual**: requiere hardware conectado a un puerto COM concreto y nunca debe automatizarse ni lanzarse desde un hook (riesgo de flashear una placa equivocada sin supervisión).
- Cada escenario Given/When/Then de una spec de OpenSpec debe mapear a al menos un `TEST_CASE`. Si un escenario no tiene test asociado, la implementación no está verificada.
- Casos límite obligatorios para código que toca el framing/protocolo: longitud de payload en el límite (`SB_PROTO_MAX_JSON_PAYLOAD`), CRC inválido, magic bytes corruptos, payload vacío.
- No bajes la cobertura de escenarios para "pasar" el stage verify; si falta un caso, añade el `TEST_CASE` en vez de omitirlo.
