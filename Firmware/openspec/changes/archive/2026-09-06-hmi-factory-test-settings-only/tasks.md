## 1. Display_HMI

Commit: `refactor(hmi): test de hardware solo desde Settings, sin boton en el splash`.

- [x] 1.1 Quitar el botón y su callback de `ui_ScreenIntro`; splash como antes.
- [x] 1.2 Quitar `g_factoryTestRequested`, la excepción de `intro_timer_cb()` y
      el parámetro de origen de `FactoryTest_Open()`.
- [x] 1.3 `pio run -e main` en verde.

## 2. Documentación

- [x] 2.1 `docs/hmi.md` §7 y `PROTOCOL.md`: una sola entrada, desde Settings.

## 3. Verificación manual (banco)

- [ ] 3.1 **Manual** — el splash no tiene botón y avanza a la pantalla
      principal como siempre; el test se abre desde Settings.
