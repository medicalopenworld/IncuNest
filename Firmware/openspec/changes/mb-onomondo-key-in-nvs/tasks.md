**El orden entre fases 1-2 y 4 no es negociable:** el flasher tiene que poder escribir la clave *antes* de que el firmware deje de leer la macro. Al revés hay una ventana en la que ninguna placa se puede activar.

Fases 1 y 2 viven en `Firmware/flasher_tool/` (fuera de este root de OpenSpec, pero dependencia dura) y se prueban con `python -m pytest flasher/tests/` desde `flasher_tool/`. Fase 3 es TDD en `[env:native]` de la motherBoard. Lo demás es verificación manual y va marcado como tal.

Lanza `pio` desde PowerShell, nunca desde MSYS/Git Bash — un run bajo MSYS da falso verde. Al leer el resumen de `pio test -e native`, mira el SUMMARY de cada suite: `test_sensorboard_frame` da ERRORED con `0xC0000139` en esta máquina desde antes de este cambio.

## 1. Entradas de tipo cadena en el generador de NVS (flasher, red primero)

- [ ] 1.1 Escribir los tests que fallan en la suite del flasher: una cadena corta, una de la longitud real (~40 caracteres), una que agote el tamaño máximo, una vacía, y la convivencia con la entrada `serial` que ya existe en el mismo namespace.
- [ ] 1.2 Añadir el tipo de entrada cadena a `flasher/nvs_gen.py` (segunda entrada de 32 B con el span de datos más la carga de longitud variable), respetando la disciplina de CRC que el módulo ya documenta en su cabecera.
- [ ] 1.3 Test de ida y vuelta: parsear la imagen generada con el propio lector del test y comprobar que namespace, clave, tipo y valor salen exactamente como entraron.
- [ ] 1.4 Verificación manual **imprescindible antes de seguir**: flashear una placa real con una imagen que lleve serial y clave, volcar su NVS y comprobar los dos valores. Si el formato de la entrada cadena necesitara más del que se ha implementado, aquí es donde se ve — y entonces la decisión 5 del diseño se invierte y se delega en `nvs_partition_gen.py` de ESP-IDF.

## 2. El flasher aprovisiona la clave

- [ ] 2.1 Leer la clave de `flasher_config.json` vía el `load_config()` que ya existe, y documentar el campo en la documentación del flasher.
- [ ] 2.2 Pasarla al generador de NVS junto al número de serie. Sin clave configurada: imagen sin ella, una línea en el registro, y el flasheo sigue adelante.
- [ ] 2.3 Comprobar que la clave no se escribe en el registro, ni en la interfaz, ni en ningún fichero salvo la imagen NVS. Añadir un test que falle si el valor aparece en el texto del log.
- [ ] 2.4 Añadir el campo al `flasher_config.json` del PC del taller (fuera del repo) y confirmar que el fichero de ejemplo versionado, si existe, lleva un placeholder y nunca la clave real.

## 3. Lectura y validación en el firmware (TDD, rojo primero)

- [ ] 3.1 Escribir los tests que fallan para la validación de la clave: vacía, solo espacios, 96 caracteres (válida), 97 (ausente), byte de control, byte por encima de 0x7E, y el formato real intacto sin recortar ni transformar.
- [ ] 3.2 Implementar la validación como función pura, y extender `build_src_filter` de `[env:native]` en `motherBoard/platformio.ini` en el mismo commit si vive en un fichero nuevo.
- [ ] 3.3 Añadir `KEY_ONOMONDO_KEY` (`"onomondo_key"`, ≤ 15 caracteres) a `include/config/preferences_keys.h`, en el namespace `mb_cfg`.
- [ ] 3.4 Getter en `ftest_sim_activation.cpp` que lea la clave de NVS una vez, la valide y la guarde en un buffer estático. Clave ausente o no válida → `FTEST_SIM_UNREACHABLE` con detail `sin key`, sin abrir ninguna conexión.
- [ ] 3.5 `pio test -e native` en verde.

## 4. Retirar la macro y el desdoble de builds

- [ ] 4.1 Sustituir las dos referencias a `ONOMONDO_API_KEY` (la cabecera `authorization:` de `httpRequest()` y la comparación contra el dummy en `ftest_sim_activation_start()`) por el getter de 3.4.
- [ ] 4.2 Quitar `ONOMONDO_API_KEY` y `ONOMONDO_API_KEY_DUMMY` de `include/protocol/Credentials_public.h`, con sus comentarios.
- [ ] 4.3 Quitar el guard `FTEST_SIM_ACT_ENABLED` de `ftest_sim_activation.{h,cpp}` y el bloque stub del build de campo, que dejan de tener sentido con la clave fuera del binario.
- [ ] 4.4 Quitar los entornos `IncuNest_V18_factory` e `IncuNest_V17_factory` de `platformio.ini`. **Dejar** `pick_pio_env_dir()` y su exclusión en el flasher, con sus 7 tests: es una guarda barata contra que el patrón vuelva.
- [ ] 4.5 Compilar `IncuNest_V18` e `IncuNest_V17` y **comprobar sobre el binario** que el prefijo de la clave da cero coincidencias en los dos. Es la prueba que de verdad cierra este cambio.
- [ ] 4.6 `pio test -e native` y la suite del flasher, las dos en verde.

## 5. Verificación manual en banco

- [ ] 5.1 Placa aprovisionada, SIM Onomondo real y WiFi: `sim_act` da PASS, y el operario no ha hecho ningún paso extra respecto a hoy. Registrar qué se flasheó y qué se observó.
- [ ] 5.2 La misma placa reiniciada: `sim_act` vuelve a autenticarse, o sea que la clave se lee de NVS y no había quedado en RAM.
- [ ] 5.3 Placa sin aprovisionar, con SIM y WiFi: WARN `sin key`, sin tráfico hacia `api.onomondo.com` (comprobable porque la API no registra ninguna petición de esa unidad).
- [ ] 5.4 Revisar el log de la unidad y el `detail` de `CTRL,FTEST`: ICCID, resultado y marca de tiempo, y ni rastro de la clave.

## 6. Documentación

- [ ] 6.1 `Firmware/PROTOCOL.md`, fila 28: quitar la nota de los entornos `*_factory` y poner que la clave sale de NVS y que sin ella el test da WARN `sin key`.
- [ ] 6.2 `Firmware/docs/thingsboard_dashboards.md` §9: reescribir la sección de la clave alrededor del aprovisionamiento por NVS, dejando dicho que se mantiene el flujo de un solo paso del operario y que el riesgo que queda es volcar el flash de una placa.
- [ ] 6.3 Documentar en el flasher qué escribe en NVS (serial y clave), y que el fichero de configuración con la clave no se versiona.

## 7. Operativa, fuera del código

- [ ] 7.1 Preguntar a Onomondo si emiten claves **restringidas** (solo activar, o acotadas a un rango de SIMs). Reduce el impacto de una filtración bajo cualquier diseño, y conviene saberlo antes de que 200 unidades lleven una.
- [ ] 7.2 Rotar la clave actual una vez esto esté en su sitio: ha estado dentro de binarios que circulan por USB, aunque no se haya publicado ninguna release con ella.
- [ ] 7.3 Decidir las dos preguntas abiertas del diseño: si la clave se borra de NVS al pasar `sim_act`, y si los secretos aprovisionados deberían vivir en su propio namespace (`mb_prov`) para que un "resetear configuración" no se los lleve.
