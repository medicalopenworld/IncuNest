## ADDED Requirements

### Requirement: Ninguna imagen de firmware contiene la clave de Onomondo

`ONOMONDO_API_KEY` puede activar y desactivar todas las SIM de la organización, y los binarios de la motherBoard se publican como assets de GitHub Releases en un repositorio público. Ninguna imagen de firmware, en ningún entorno de compilación y en ninguna máquina, SHALL contener la clave. No SHALL existir ninguna vía de compilación que la introduzca: las macros `ONOMONDO_API_KEY` y `ONOMONDO_API_KEY_DUMMY` SHALL desaparecer de `Credentials_public.h` y el firmware no SHALL leerlas de `Credentials.h`.

#### Scenario: El binario distribuido no contiene la clave

- **WHEN** se compila cualquier entorno de la motherBoard en una máquina que sí tiene el `Credentials.h` real
- **THEN** buscar el prefijo de la clave en el `firmware.bin` resultante no da ninguna coincidencia

Verificación manual sobre el binario producido, en las dos revisiones de hardware.

#### Scenario: Una definición vieja en Credentials.h no reintroduce la clave

- **AND** ese `Credentials.h` todavía define `ONOMONDO_API_KEY` de una versión anterior
- **WHEN** se compila
- **THEN** la definición queda sin usar, el binario no la contiene y el build no falla

Verificación manual sobre el binario producido.

#### Scenario: Un único binario por revisión de hardware

- **WHEN** se listan los entornos de `motherBoard/platformio.ini`
- **THEN** no existe ningún entorno con sufijo de fábrica, y `sim_act` se comporta igual en todos

Verificación manual por inspección del cambio.

### Requirement: La clave llega a la placa por NVS, escrita al flashear

La clave SHALL viajar en la partición NVS que el flasher ya escribe durante el flasheo, en el namespace `mb_cfg` bajo la clave `onomondo_key`, como cadena. Esto SHALL preservar el flujo del operario: una sola acción de flasheo, y la placa activa su propia SIM con el ICCID que ya obtiene del módem. No SHALL añadirse ningún paso manual —ni reinicio, ni lectura del CCID, ni una segunda herramienta— porque ese es el motivo por el que la activación vive en la placa y no en el PC.

#### Scenario: Placa aprovisionada activa su SIM

- **WHEN** el flasher escribe la clave en NVS al flashear y la placa arranca con SIM Onomondo y WiFi
- **THEN** `sim_act` contacta con la API y da PASS, sin ninguna acción extra del operario

Verificación manual en banco con red y una SIM Onomondo real.

#### Scenario: La clave sobrevive al reinicio

- **WHEN** una placa aprovisionada se reinicia
- **THEN** `sim_act` sigue pudiendo autenticarse: la clave se lee de NVS, no de RAM

Verificación manual en banco.

### Requirement: Contrato del productor

El flasher SHALL leer la clave de `flasher_config.json` —que `load_config()` ya lee y que no está versionado— y escribirla en la imagen NVS junto al número de serie que ya escribe. Si no hay clave configurada SHALL escribir la imagen sin ella y dejarlo dicho en su registro, y NO SHALL abortar el flasheo: una placa sin clave es una placa cuya SIM se activará de otra forma, no una placa inservible. El flasher no SHALL escribir la clave en su registro, ni en la interfaz, ni en ningún fichero que no sea la imagen NVS.

#### Scenario: Sin clave configurada el flasheo sigue adelante

- **WHEN** `flasher_config.json` no trae clave y se flashea una placa
- **THEN** el flasheo termina correctamente, el registro dice que no se aprovisionó clave, y la placa da WARN en `sim_act`

Verificación manual con el flasher.

#### Scenario: La clave no aparece en el registro del flasher

- **WHEN** se flashea una placa con clave configurada y se revisa el registro y la interfaz
- **THEN** la clave no aparece en ninguno de los dos

Verificación manual con el flasher.

#### Scenario: La imagen NVS se lee de vuelta con el valor exacto

- **WHEN** se genera una imagen NVS con una clave de la longitud real y se vuelca la NVS de la placa flasheada
- **THEN** el valor leído coincide carácter por carácter con el configurado

Verificación manual contra una placa real; el formato de la entrada se cubre además en la suite pytest del flasher.

### Requirement: La clave leída de NVS se valida antes de usarse

El contenido de NVS no es una constante de compilación: puede venir truncado, a medias o mal configurado. La clave SHALL considerarse ausente si tras recortar espacios queda vacía, si excede 96 caracteres, o si contiene algún byte fuera del ASCII imprimible. Una clave que no valide SHALL tratarse exactamente como una clave ausente, y NO SHALL enviarse a `api.onomondo.com`: mandarla produciría un 401 confuso en vez de un motivo honesto.

#### Scenario: Clave vacía o solo espacios

- **WHEN** la entrada de NVS no existe, está vacía, o contiene solo espacios
- **THEN** se considera ausente

Cubierto por motherBoard `[env:native]`.

#### Scenario: Clave demasiado larga

- **WHEN** la entrada tiene 97 caracteres o más
- **THEN** se considera ausente

Cubierto por motherBoard `[env:native]`.

#### Scenario: Clave con bytes no imprimibles

- **WHEN** la entrada contiene un byte de control o un byte por encima de 0x7E
- **THEN** se considera ausente

Cubierto por motherBoard `[env:native]`.

#### Scenario: Clave del formato real

- **WHEN** la entrada tiene el formato observado del servicio, del orden de 40 caracteres imprimibles
- **THEN** se acepta tal cual, sin recortarla ni transformarla

Cubierto por motherBoard `[env:native]`.

### Requirement: La clave no se registra en ningún sitio

La clave SHALL viajar únicamente al socket TLS de `api.onomondo.com`, en la cabecera `authorization`. NO SHALL aparecer en ningún log, ni en el campo `detail` del test, ni en telemetría, ni en un volcado de crash. La traza por unidad que exige fábrica SHALL seguir llevando ICCID, resultado y marca de tiempo, y nunca la clave.

#### Scenario: La traza de fábrica no la contiene

- **WHEN** `sim_act` termina, con éxito o con error, y se revisa el log de la unidad
- **THEN** aparecen ICCID, resultado y marca de tiempo, y la clave no aparece

Verificación manual en banco.

#### Scenario: El detail del test no la contiene

- **WHEN** el resultado viaja en la línea `CTRL,FTEST`
- **THEN** el campo `detail` lleva un motivo corto y nunca la clave ni parte de ella

Verificación manual en banco.
