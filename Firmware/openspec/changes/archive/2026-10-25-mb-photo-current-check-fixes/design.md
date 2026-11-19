# Diseño

**D1 — Dos «no evaluable» distintos.** Apagada, test de fábrica o asentamiento: no hay nada que afirmar, se retira. Sin medida con la lámpara ON: se mantiene el estado y se cuentan ciclos a ciegas (`PHOTO_FAULT_BLIND_CYCLES` = 5); al llegar se declara la misma condición, y el texto de las cuatro lenguas cubre «sin corriente o medida». Una lectura fallida aislada no retira nada (sin parpadeo).

**D2 — Muestra nueva.** El criterio es el mismo que el del calefactor: `systemCurrentSampleSeq` avanzó. El lazo de intensidad lo consume en su propia condición (solo se toca la condición del `if`, no su cuerpo); la alarma usa un marcador propio porque corre a otra cadencia.

**D3 — Liberación confirmada.** Tras soltar se fuerza `real_before` hasta un keepalive con ese valor, nunca antes de `PHOTO_OVERRIDE_CONFIRM_MIN_MS` (2 s: una trama en vuelo anterior a la adopción también lleva el valor real). A los 60 s se registra un error una vez, sin adoptar nada. Residuo: la lógica solo corre al llegar una orden del display.

**D4 — MEDIA provisional.** Tabla 1 de IEC 60601-1-8: no dar una fototerapia indicada puede acabar en kernicterus (irreversible, diferido) → MEDIA. Provisional hasta el expediente de riesgos.
