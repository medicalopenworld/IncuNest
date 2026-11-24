## 1. motherBoard (TDD en `[env:native]`)

- [x] 1.1 Tests en rojo: parse de `PROFILE_SEX` (válidos y malformados), `PROFILE_NEW` sigue en 4 campos, `baby_sex` en atributos/alta/vacío, layout del blob (64 B, `sex` en el offset 33), blob antiguo = 0, auditoría sin sexo.
- [x] 1.2 `BabySex`, `BabyProfile.sex` en el relleno + `static_assert`, `baby_sex_sanitize()`, parse, `babyStore_setSex()`, `CommTask`, `baby_sex` en el payload.
- [x] 1.3 `pio test -e native` en verde y `pio run -e IncuNest_V19` compila.

## 2. Display_HMI

- [x] 2.1 `ui/SexPicker` compartido, paso de sexo en `BabyWizard` y en el registro de `BabyHistory`, `Communication_SendProfileSex()`.
- [x] 2.2 Textos en los 4 idiomas (ASCII, sin Ñ por las fuentes) y lecciones de formación (1/4..4/4).
- [x] 2.3 `pio run -e main` compila.
- [ ] 2.4 **Manual en hardware**: registrar un bebé de cada sexo y uno NO CONSTA por las dos vías; ver `baby_sex` en ThingsBoard; actualizar una placa con bebés activos y comprobar que siguen; lección de registro completa en formación.

## 3. Documentación

- [x] 3.1 `PROTOCOL.md` v2.4.0 y `docs/thingsboard_dashboards.md`.
