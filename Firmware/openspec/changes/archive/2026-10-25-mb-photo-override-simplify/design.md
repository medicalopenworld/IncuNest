# Diseño

**D1 — Quitar la raíz.** Todo el problema nacía de emitir el ON forzado: el display lo adopta y lo devuelve. Mientras dura la prueba, `photo_override_state_field()` devuelve `ordered` (la última orden del display). Sin 1 rancio no hay nada que neutralizar, y por tanto ni ventana, ni confirmación, ni plazos.

**D2 — `ordered` registra al operador.** `photo_override_effective()` guarda cada orden del display durante la prueba; `photo_override_note_order(false)` registra la caducidad del temporizador. Al soltar, `photo_override_take_release()` entrega `ordered` una sola vez.

**D3 — Aplicación en el tick.** `Communication_Receiver` aplica el valor forzado (ON) y, tras soltar, el valor entregado, por `applyPhotoState()` (el camino de una orden). No persiste: la persistencia sigue siendo solo del camino de la orden y con `debug_photo_may_persist()`.

**D4 — Spinlock y no mutex para el PWM.** La sección crítica es una escritura de registro: microsegundos. La tarea de seguridad no puede bloquearse ni sufrir inversión de prioridad. El duty se calcula dentro de la sección crítica a partir de `in3.phototherapy`, así que la última escritura serializada siempre ve el estado más reciente (equivale a recomprobar dentro de la sección crítica). Alternativa descartada: recomprobar sin cerrojo, que deja una ventana.

**D5 — Qué se ve en pantalla.** Durante la prueba el display no muestra el ON; la prueba se sigue en `/debug/state` y en el log. Es el coste aceptado de D1.
