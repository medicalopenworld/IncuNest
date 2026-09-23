> Propuesta **aplazada**: ninguna fase está planificada. La Fase 0 tiene que
> cerrarse antes de escribir código, y su pregunta 0.1 puede cancelar el cambio
> entero.
>
> Las fases están ordenadas para que cada una caiga como un commit atómico y
> ninguna mezcle dos placas, según `Firmware/.claude/rules/commits.md`.

## 0. Decisiones previas

- [ ] 0.1 Decidir si se implementa sin el segundo corte térmico a 40 °C que exige el camino de override (design, pregunta abierta 1). Consultar con los clínicos de Togo: un tope duro en 37 °C sin override no es aceptable si usan consignas más altas
- [ ] 0.2 Decidir si el override caduca, y si sobrevive al reinicio (preguntas 2 y 4)
- [ ] 0.3 Decidir el gesto de override en el HMI (pregunta 3)
- [ ] 0.4 Decidir el comportamiento en modo piel (pregunta 5)
- [ ] 0.5 Decidir cómo viaja el override a ThingsBoard (pregunta 6)

## 1. shared/ — la condición nueva

- [ ] 1.1 Añadir el `AlarmId` nuevo **al final** del enum, antes de `ALARM_COUNT` (`shared/include/alarm_ids.h`)
- [ ] 1.2 Prioridad MEDIA en `alarm_priority()`, y **fuera** de `alarm_cuts_heater()` y de `alarm_is_latching()` (`shared/src/alarm_policy.cpp`)
- [ ] 1.3 Título y descripción en ES/FR/PT/EN dentro de los límites de `ALARM_TITLE_MAX_CHARS` / `ALARM_DESC_MAX_CHARS` (`shared/src/alarm_text.cpp`)
- [ ] 1.4 Tests Unity en `[env:native]` para la prioridad, el no-corte y la longitud de los textos, siguiendo `test_alarm_policy` y `test_alarm_text`

## 2. motherBoard — detección

- [ ] 2.1 Extraer a `modules/control/` el umbral absoluto con histéresis y la regla de «no anunciar bajo override», como lógica pura testeable en host
- [ ] 2.2 Tests Unity en `[env:native]`: cruce del umbral, histéresis al bajar, silencio bajo override, y que quitar el override reevalúa la condición sin esperar al siguiente flanco
- [ ] 2.3 Declarar la condición en `checkAlarms()` (`src/system/security.cpp`), junto a las de desviación
- [ ] 2.4 Topar la consigna de aire en 37 °C sin override en todos los puntos de entrada: `CommTask.cpp`, `Wifi_OTA.cpp` y la carga de NVS de `EEPROM.cpp`
- [ ] 2.5 Clave de telemetría de la alarma nueva (`include/config/telemetry_keys.h`) y su publicación en `GPRS.cpp` y `Wifi_OTA.cpp`

## 3. Display_HMI — override e indicación

- [ ] 3.1 El gesto de override decidido en 0.3, en la pantalla de consigna
- [ ] 3.2 Indicación permanente mientras el override esté activo, visible sin navegar
- [ ] 3.3 **Manual**: verificar en banco que el override se ve al arrancar si sobrevive al reinicio (decisión 0.2)

## 4. Verificación

- [ ] 4.1 **Manual**: con una fuente de calor externa, consigna a 35 °C y aire llevado a 37.5 °C — la condición se anuncia sin que salte el corte térmico
- [ ] 4.2 **Manual**: consigna a 36.5 °C con el aire siguiéndola a 37.4 °C — la condición se anuncia aunque la desviación sea de solo +0.9 °C. Este es el caso que hoy queda mudo y la razón de ser del cambio
- [ ] 4.3 **Manual**: con override activo a 37.5 °C de consigna, la condición **no** se anuncia y la indicación de override sigue visible
- [ ] 4.4 **Manual**: comprobar que no hay parpadeo ni doble anuncio con la alarma de desviación en el mismo cruce (known issue nº 1)

## 5. Documentación

- [ ] 5.1 `Firmware/docs/alarms.md`: la condición nueva en la tabla y en §5
- [ ] 5.2 `Firmware/PROTOCOL.md` si el override viaja por el enlace
- [ ] 5.3 `Firmware/docs/alarm_popup_image_prompts.md`: prompt e imagen del pop-up
