## Why

Afecta solo a **Display_HMI**. Hasta ahora el test de hardware solo se podía
lanzar desde el splash, en los primeros segundos tras el arranque. En campo, un
técnico que ya tiene la incubadora encendida tendría que reiniciarla para
llegar al botón. Hace falta una segunda entrada desde la propia interfaz.

Se eligió **Settings** y no el menú de ayuda: la ayuda es para la enfermera y
está a un toque desde la pantalla principal, mientras que el test enciende el
calefactor al 100 % en lazo abierto con las alarmas inhibidas. Settings ya
exige mantener pulsado el candado y es donde viven calibración, WiFi e Info.
Además los overlays de ayuda ceden ante cualquier alarma y se cierran solos a
los 3 min; el test hace lo contrario (inhibe alarmas, tiene sus propias cotas),
así que meterlo ahí obligaría a excepciones en una regla uniforme.

## What Changes

- Fila **"Test de hardware"** al final de `ui_ScreenSettings`, debajo de
  Info. Habilitada solo con control y fototerapia apagados; si no, en gris con
  el subtexto "Apaga el control para testear".
- Al pulsar, el mismo aviso "equipo vacío, sin paciente" con Sí / No del
  splash y la misma pantalla de test. "No" vuelve a Settings; "Salir" tras
  una batería vuelve a la pantalla principal, como ahora.
- El botón del splash se mantiene (vía de fábrica). La motherBoard no cambia:
  el rechazo por control activo sigue siendo la barrera dura.

## Capabilities

### New Capabilities
<!-- Ninguna. -->

### Modified Capabilities
- `hmi-factory-test`: segunda entrada desde Settings con condición de
  habilitación visible.

## Impact

- `Display_HMI/src/ui/ElementsCreation.cpp` (fila en Settings),
  `Display_HMI/src/ui/FactoryTest.cpp` (origen de apertura, refresco de la
  fila), `Display_HMI/src/tasks/UITask.cpp` (idioma, refresco).
- `docs/hmi.md` §7.
- Riesgo residual ya anotado: sin PIN de servicio, la barrera en campo es
  candado con pulsación larga + control apagado + confirmación explícita.
