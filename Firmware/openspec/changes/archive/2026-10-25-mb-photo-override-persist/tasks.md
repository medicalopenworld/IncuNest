## 1. motherBoard

- [x] 1.1 Tests en rojo del rearme y del cerrojo de caducidad
- [x] 1.2 `photo_override_start` conserva la orden con liberación pendiente; cerrojo `photo_expiry_latch` en el parser y en la caducidad
- [x] 1.3 Persistir la orden del display siempre y el valor aplicado al soltar; quitar `may_persist`
- [x] 1.4 `timeTrackHandler` excluye la prueba
- [x] 1.5 Comentarios que prohíben el fade LEDC (sin comprobación en tiempo de ejecución)

## 2. Verificación

- [x] 2.1 `pio test -e native` y `pio run -e IncuNest_V18`
- [ ] 2.2 **Manual (banco)**: `PHOTO,1` con el display desconectado y `PHOTO,0` — la lámpara se apaga y `photo_active` en NVS queda con el valor aplicado
- [ ] 2.3 **Manual (banco)**: temporizador vencido con el display enviando `photo=1,photoMin=0` — la lámpara no se reenciende

## 3. Documentación

- [x] 3.1 `PROTOCOL.md` y `docs/known_issues.md` §20
