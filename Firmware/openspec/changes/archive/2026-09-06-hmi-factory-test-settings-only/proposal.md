## Why

Afecta solo a **Display_HMI**. Con la fila "Test de hardware" ya disponible en
Settings, el botón del splash sobra: duplica la entrada, está a un toque de
cualquier persona que reinicie la incubadora y fue la causa del arranque
espontáneo del test en banco (toque fantasma del GT911 durante el init). La
zona técnica (candado con pulsación larga) es el único sitio donde debe estar.

## What Changes

- Se elimina el botón "HW test" de `ui_ScreenIntro`; el splash vuelve a ser
  solo logos y versión, con su temporizador de siempre.
- La única entrada al test es la fila de Settings, habilitada solo con
  control y fototerapia apagados, con el pop-up de "equipo vacío".
- Se retira el código que solo servía al splash: armado de 1,5 s,
  `g_factoryTestRequested`, la excepción en `intro_timer_cb()` y el parámetro
  de origen de `FactoryTest_Open()`.

## Capabilities

### New Capabilities
<!-- Ninguna. -->

### Modified Capabilities
- `hmi-factory-test`: entrada única desde Settings; se retira el requisito del
  botón del splash.

## Impact

- `Display_HMI/src/ui/ElementsCreation.cpp`, `src/tasks/UITask.cpp`,
  `src/ui/FactoryTest.cpp` y headers.
- `docs/hmi.md` §7, `PROTOCOL.md` (texto introductorio de `FTEST`).
