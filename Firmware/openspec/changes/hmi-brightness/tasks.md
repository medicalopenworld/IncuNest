Cada fase es un commit atómico. Todo el cambio es de `Display_HMI` (no toca
`shared/` ni la motherBoard). Comandos de verificación, desde
`Firmware/Display_HMI`:

```
pio test -e native
pio run -e main
```

## 1. Propuesta

- [x] 1.1 Explorar el código (backlight, bloqueo, alarmas, NVS, Ajustes).
- [x] 1.2 Proposal, design, tasks y delta spec; `openspec validate --all`.

## 2. Display_HMI: política pura (TDD, host)

- [x] 2.1 Escribir en rojo `test/test_brightness_policy/test_brightness_policy.cpp` con los escenarios de `specs/hmi-brightness/` (tabla de estados, saneado, conversión, modo sin niveles) y dar de alta `modules/brightness/brightness_policy.cpp` en el `build_src_filter` de `[env:native]`.
- [x] 2.2 Implementar `modules/brightness/brightness_policy.{h,cpp}` como lógica pura, sin Arduino ni LVGL. Verde en `pio test -e native`.

## 3. Display_HMI: driver y servicio

- [x] 3.1 `drivers/backlight.{h,cpp}`: `Backlight_SetPercent()`, `Backlight_Off()`, reintentos, `static_assert` de coherencia de bytes con `display_config.h`. Sustituir las escrituras en línea del arranque y de `pwroff_update()`.
- [x] 3.2 Constantes en `display_config.h` (`DISPLAY_BL_LEVELS_SUPPORTED`) y claves `HMI_KEY_BL_PCT`/`HMI_KEY_BL_NIGHT` en `EEPROM_defines.h`; carga en `EEPROM.cpp`.
- [x] 3.3 Servicio de brillo en `UI_Task`: objetivo calculado bajo `LVGL_Lock`, escritura I2C tras `LVGL_Unlock()`, reintento si falla.
- [x] 3.4 Filas de slider y switch en `ui_ModesConfigCont`, handlers, persistencia en el bloque diferido (no en modo formación), cadenas en cuatro idiomas, snapshot de formación si procede.

## 4. Verificación

- [x] 4.1 `pio test -e native` en verde.
- [x] 4.2 `pio run -e main` en verde; anotar RAM y Flash.
- [ ] 4.3 **Manual, en banco (obligatorio, no hay entorno de test)**: el STC8H1K28 acepta niveles intermedios. Barrer 20/40/60/80/100 %, confirmar monotonía y que ningún nivel apaga la pantalla ni suena el zumbador. Si no los acepta: `DISPLAY_BL_LEVELS_SUPPORTED=0` y reportar.
- [ ] 4.4 **Manual, en banco**: modo noche (atenúa a los 30 s en bloqueo, toca y vuelve), alarma durante la atenuación, cese de la alarma, pérdida de enlace, pwroff apaga, cambio de idioma, persistencia tras reinicio, formación no persiste.

## 5. Documentación y cierre

- [ ] 5.1 `Firmware/docs/hmi.md`: backlight, ajustes, claves NVS.
- [ ] 5.2 `openspec archive hmi-brightness`.
