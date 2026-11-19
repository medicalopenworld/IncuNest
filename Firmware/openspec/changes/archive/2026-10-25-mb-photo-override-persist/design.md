# Diseño

**D1 — Persistir la orden del display siempre.** Como el campo 8 emite la orden del display durante la prueba, lo que el display devuelve en su keepalive es una orden real del operador y nunca está contaminado. No guardarla perdería un OFF/ON del operador si la placa cayera a mitad de prueba. El ON forzado vive solo en `in3.phototherapy`, no en `hmi_cmd_msg`, así que nunca llega a NVS.

**D2 — Persistir el valor aplicado al soltar.** Con el display colgado no llega ningún keepalive. Si venció el temporizador durante la prueba, el valor aplicado es OFF aunque el último keepalive guardado dijera ON.

**D3 — Rearme.** `photo_override_start()` solo captura la orden si no hay prueba activa ni liberación pendiente: en ambos casos `ordered` ya lleva las órdenes del operador y el `in3.phototherapy` que llega está contaminado.

**D4 — Cerrojo de caducidad.** Lógica pura en `photo_expiry_latch`: tras vencer, `photo=1` sin minutos se ignora hasta ver `photo=0`; con minutos se acepta. Es una carrera preexistente fuera de la prueba, y se cierra en el mismo sitio.

**D5 — Sin comprobación de fade en el arranque.** Instalar y desinstalar el servicio de fade reservaría memoria, registraría su ISR de forma global y rompería a otro usuario legítimo. Basta la prohibición escrita junto a `PHOTOTHERAPY_PWM_CHANNEL` y `photoPwmRefresh()`.
