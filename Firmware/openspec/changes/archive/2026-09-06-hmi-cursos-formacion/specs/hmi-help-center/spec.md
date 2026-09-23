## MODIFIED Requirements

### Requirement: El heading de la pantalla principal ofrece un botón de ayuda

`ui_ScreenMain` SHALL mostrar un botón de ayuda (`?`) en el heading, a la
izquierda del reloj, visible en todo momento mientras la pantalla principal
esté cargada. El resto de widgets del heading (reloj, conectividad, candado,
Bebés, alarmas, ajustes) SHALL conservar su función y su zona táctil.

La réplica del heading en `ui_ScreenLock` SHALL usar las mismas posiciones
de reloj y conectividad que `ui_ScreenMain` y NO SHALL mostrar el botón de
ayuda.

#### Scenario: El botón está en el heading y abre el menú
- **WHEN** el operador toca el `?` del heading en `ui_ScreenMain`
- **THEN** se abre el menú de ayuda como diálogo modal sobre la pantalla
  principal, con tres opciones: tutorial guiado (que abre el selector de
  cursos de `hmi-training-courses`), vídeo tutorial y contacto
- *(Verificación manual en banco.)*

#### Scenario: El heading redistribuido no rompe las zonas táctiles
- **WHEN** el operador toca el borde izquierdo del reloj o cualquier punto
  de la zona táctil ampliada del candado
- **THEN** responde el reloj o el candado, no el botón de ayuda
- *(Verificación manual en banco.)*

#### Scenario: La pantalla de bloqueo no cambia de reparto
- **WHEN** la pantalla pasa a `ui_ScreenLock`
- **THEN** reloj y conectividad aparecen en la misma posición horizontal
  que tenían en `ui_ScreenMain`
- **AND** no hay botón de ayuda
- *(Verificación manual en banco.)*
