## 1. motherBoard (TDD, host)

- [x] 1.1 Tests en rojo de `photo_override` con la API nueva
- [x] 1.2 Implementar `photo_override` mínimo y quitar ventana, confirmación y respaldo
- [x] 1.3 Campo 8 con `debug_photo_state_field()`, caducidad del temporizador como orden, tick de aplicación y liberación
- [x] 1.4 `photoPwmRefresh()` bajo spinlock en el lazo de intensidad y en `applyPhotoState()`

## 2. shared

- [x] 2.1 `_Static_assert` en C junto al `static_assert`

## 3. Verificación

- [x] 3.1 `pio test -e native` (motherBoard) y `pio run -e IncuNest_V18`
- [x] 3.2 `pio run -e main` en Display_HMI
- [ ] 3.3 **Manual (banco)**: `PHOTO,1` — la lámpara enciende y la pantalla NO cambia; `PHOTO,0` con el display desconectado — la lámpara se apaga en el acto
- [ ] 3.4 **Manual (banco)**: temporizador vencido durante la prueba — al soltar queda OFF y no se reanuda tras reiniciar
- [ ] 3.5 **Manual (banco)**: verificar el falso positivo de `ALARM_PHOTO_LAMP_FAULT` en HW17 sin shunt antes de fabricar

## 4. Documentación

- [x] 4.1 `PROTOCOL.md` y `docs/known_issues.md` §20
