# Diseño

## Contexto

El equipo tiene hoy dos familias de vigilancia térmica, y las dos son relativas o de último recurso:

1. **Desviación respecto a la consigna** (`ALARM_AIR_TEMP_DEVIATION_HIGH/LOW`, ±1 °C tras `fix/mb-air-deviation-1c`). Responde a «¿me alejo de lo que me pidieron?».
2. **Corte térmico** (`ALARM_AIR_THERMAL_CUTOUT`, tope 38 °C). Responde a «¿me he ido del todo?». Es ALTA, latching, corta el calefactor y exige reset manual.

Falta la pregunta intermedia y absoluta: «¿está el aire demasiado caliente, me lo hayan pedido o no?».

## Decisiones propuestas

**D1 — Umbral en 37 °C.** Es el valor por debajo del cual la norma no exige override, y deja un grado entero de margen hasta el disyuntor de 38 °C. Un umbral más bajo chocaría con consignas legítimas de gran prematuro; uno más alto se solaparía con el corte.

**D2 — Prioridad MEDIA, no latching, sin corte de calefactor.** Es un aviso, no una protección: el corte de calefactor ya lo gobiernan la desviación (a +1 °C) y el corte térmico (a 38 °C), y a 37 °C con la consigna por debajo una de las dos ya habrá actuado. Añadir un tercer camino al actuador solo complica el razonamiento sobre quién corta.

**D3 — El override es del operador y es visible mientras dure.** No basta con permitir la consigna alta: 6.8.1 razona que el operador tiene que poder saber qué señales están inactivadas. La indicación permanente es lo que evita que un turno herede un equipo en override sin saberlo.

**D4 — El id nuevo va al final de `AlarmId`.** El valor numérico es el índice de bit del bitmask del protocolo; insertar en medio rompe la compatibilidad con cualquier HMI que no se actualice a la vez.

**D5 — La lógica pura vive en `modules/control/`.** Es la única parte de la motherBoard con entorno de test nativo. El umbral con histéresis y la regla de «no anunciar bajo override» son exactamente el tipo de lógica que ahí se puede verificar sin hardware.

## Preguntas abiertas (cerrar en la Fase 0)

1. **¿Se implementa sin el segundo corte a 40 °C?** La norma lo exige para el camino de override y este hardware no lo tiene. Opciones: (a) implementarlo igualmente como mejora real de seguridad, sin reclamar conformidad; (b) esperar a una revisión de hardware; (c) implementar solo el tope de 37 °C *sin* override, lo que prohíbe consignas altas legítimas y probablemente sea inaceptable clínicamente. Hay que preguntárselo a los clínicos de Togo antes de decidir.
2. **¿El override caduca?** Un override permanente se olvida; uno que caduca a mitad de la noche baja la consigna sin que nadie lo pida, lo cual es peor. Si caduca, ¿avisa antes?
3. **¿Qué gesto es el override?** Confirmación en un diálogo, pulsación larga, o código de servicio. Tiene que ser difícil de hacer sin querer y fácil de hacer a propósito con guantes.
4. **¿El override sobrevive al reinicio?** Si no sobrevive, un corte de red baja la consigna de un gran prematuro. Si sobrevive, hay que poder verlo al arrancar.
5. **¿Qué pasa en modo piel?** Ahí la consigna es de piel y la temperatura de aire es libre. La condición absoluta de aire ¿sigue viva? Probablemente sí, pero con qué umbral es otra conversación.
6. **¿Cómo se publica a la nube?** Clave propia de telemetría, y si el override en sí debe ser un atributo visible en ThingsBoard para que se vea desde fuera qué equipos están por encima de 37 °C.
