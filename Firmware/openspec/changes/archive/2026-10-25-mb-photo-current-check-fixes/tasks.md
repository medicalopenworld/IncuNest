## 1. shared

- [x] 1.1 Prioridad MEDIA provisional en `alarm_policy.cpp` y reparto 7/10/3 en `test_alarm_policy`
- [x] 1.2 Texto de las cuatro lenguas que cubre «sin corriente o medida»
- [x] 1.3 `static_assert(ALARM_COUNT <= 32)` en `alarm_ids.h`

## 2. motherBoard (TDD, host)

- [x] 2.1 Tests en rojo de `photo_fault` (ceguera) y `photo_override` (confirmación)
- [x] 2.2 Implementar ambos módulos puros
- [x] 2.3 Cablear: condición del lazo de intensidad, clasificación del ciclo, log del plazo vencido, contabilidad por bebé, validación de `photo`
- [x] 2.4 `photoMin` inicializado, `lastPhotoFaultCheck` a `uint32_t`

## 3. Verificación

- [x] 3.1 `pio test -e native` en motherBoard y Display_HMI
- [x] 3.2 `pio run -e IncuNest_V18` y `pio run` en Display_HMI
- [ ] 3.3 **Manual (banco)**: INA principal desconectado con la lámpara ON: alarma a ~8 s y PWM quieto
- [ ] 3.4 **Manual (banco)**: `PHOTO,1` / `PHOTO,0` con el display lento: la lámpara se apaga y no se guarda

## 4. Documentación

- [x] 4.1 `PROTOCOL.md`, `known_issues.md` §20, `alarms.md`, `alarm_popup_image_prompts.md`, nota de despliegue (HMI antes que MB)
