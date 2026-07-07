---
paths:
  - "components/**"
  - "main/**"
  - "test_apps/**"
---

# Reglas de seguridad (firmware / dispositivo médico)

- **Valida la longitud de la trama antes de confiar en ella.** El campo `Length` (4B LE) de una trama USB-CDC llega de un canal externo y puede estar corrupto o ser malicioso. Antes de usarlo para reservar, copiar o iterar, compruébalo contra el tamaño real del buffer de recepción (`SB_PROTO_MAX_JSON_PAYLOAD`/`SB_PROTO_MAX_JSON_FRAME` o el límite que aplique). Un `Length` mayor que el buffer nunca debe provocar un desbordamiento ni un bucle sin cota.
- **Verifica el CRC antes de actuar sobre el payload.** Ninguna trama se interpreta, parsea como JSON, ni dispara un comando hasta que el CRC16 calculado coincide con el recibido. Una trama con CRC inválido se descarta silenciosamente (o se loguea como error), nunca se procesa "por si acaso".
- **Cuidado con overflow/underflow en aritmética de tamaños.** Cálculos como `payload_len + overhead`, `buffer_size - header_len`, o índices derivados de `Length` deben revisarse por overflow (wrap de enteros sin signo) y underflow (resta que cruza cero en tipos sin signo). Usa comprobaciones explícitas antes de la operación, no confíes en que "nunca va a pasar".
- **Nunca copies con longitud no acotada.** Todo `memcpy`/`strcpy`/equivalente cuya longitud provenga de un campo de trama, de un sensor externo o de cualquier entrada no confiable debe acotarse contra el tamaño del buffer destino antes de ejecutarse. Prefiere `memcpy` con un `MIN(len, sizeof(buf))` explícito o un chequeo previo que aborte la operación.
- **Fail-safe, no fail-fast, en lecturas de sensor y control de actuadores.** Este firmware corre en una incubadora neonatal. Un fallo de lectura I2C/I2S/ADC (timeout, NACK, valor `NaN` o fuera de rango físico) debe reportarse como `sensors.<nombre>: false` y dejar la tarea viva — nunca debe crashear la tarea, colgarla, ni (más crítico) dejar pasar un valor inválido a lógica que controle un actuador. Una lectura `NaN`/fuera de rango nunca debe traducirse en una acción de control sobre el actuador; descarta el valor y mantén el último estado seguro conocido o un estado de fallback explícito.
- **Sin credenciales hardcodeadas**, incluso si en el futuro se añade WiFi/BLE (SSID, contraseñas, tokens de API). Usarían `menuconfig`/`sdkconfig` no versionado o NVS cifrado, nunca literales en el código fuente.
- **Respeta los límites de los arrays de tamaño fijo.** Todo acceso a buffers como `SB_PROTO_MAX_JSON_PAYLOAD` debe comprobar el índice/longitud antes de escribir o leer; un índice fuera de rango en C no falla de forma segura, corrompe memoria adyacente.
- No loguees payloads crudos no validados a nivel `info` en producción si pueden contener datos de más de un frame — evita que un log se convierta en vector de confusión; sí es correcto loguear el frame descartado a nivel de error con su motivo (CRC/longitud inválidos).
- **Hooks que emiten JSON de control** (decision/permissionDecision/systemMessage): construye SIEMPRE la salida con `jq -nc --arg` (escape correcto); **nunca** con `printf`/`echo` interpolando variables, ni el contenido de ficheros de estado (`.loop-mode`, `.unattended-state`, etc.). Un valor con comillas podría inyectar campos y secuestrar la decisión del hook. Valida además las entradas contra una allowlist cuando aplique.
