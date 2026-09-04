## Why

Afecta a **Display_HMI**. No toca `shared/` ni `motherBoard`: el modo
formación se implementa en el lado HMI del protocolo (qué envía y qué aplica
`CommTask`), sin cambiar ninguna trama.

El menú de ayuda (`hmi-help-center`, archivado 2026-09-04) tiene un tutorial
guiado pasivo: resalta y explica, pero no deja tocar. El personal de los
hospitales de destino aprende a usar la incubadora con formación presencial
que no siempre llega, y hoy no hay forma de acreditar que alguien sabe
encender el control de temperatura, atender una alarma o dar de alta a un
bebé. El usuario quiere convertir ese tutorial en un **cursillo que el
personal tiene que superar**, con dos itinerarios (enfermería y técnico),
interactivo de verdad (el operador pulsa los controles reales) y con
seguridad clínica garantizada.

Dos hechos del código actual obligan a diseñarlo con cuidado
(`openspec/changes/hmi-cursos-formacion/design.md`, mapa del 2026-09-04):

- **La placa manda.** Todo cambio local (consigna, toggle, modo) pone
  `hmi_msg.shouldSendData` y viaja a la motherBoard, que actúa sobre
  calefactor, humidificador y fototerapia y devuelve su estado en
  `CTRL,STATE` (~1 Hz); `Display_ApplyCtrlState()` lo aplica a la UI
  (`CommTask.cpp:925+`). No existe ningún modo en el que la UI cambie sin que
  la incubadora lo haga.
- **El alta de un bebé crea el registro en la placa a mitad del asistente**
  (`Communication_SendProfileNew` en `BabyWizard.cpp:461`, al confirmar las
  semanas de gestación), no al final. Un tutorial que deje avanzar el
  asistente real crea un paciente real, con su telemetría en ThingsBoard.

## What Changes

- **Modo formación en la HMI** (`training_mode`): mientras está activo,
  `CommTask` **no envía** a la placa ningún cambio de estado ni de perfil
  (sigue mandando el keepalive con la instantánea tomada al entrar, para no
  perder el enlace) y **no aplica** a la UI el `CTRL,STATE` que llega (sigue
  actualizándolo en `ctrl_state_msg` para alarmas y detección de enlace).
  Los asistentes que esperan respuesta de la placa (`BabyWizard`,
  `BabyExitDialog`, `TimeDialog`) reciben respuestas simuladas localmente.
  Resultado: la UI se comporta con normalidad y la incubadora no cambia nada.
  Al salir se descarta el estado local y se vuelve a sincronizar con el
  siguiente `CTRL,STATE`.
- **Cursos**: el botón TUTORIAL GUIADO del menú de ayuda abre un selector con
  dos cursos, **Enfermería** y **Técnico**, cada uno con su lista de
  lecciones y el progreso (hecha / pendiente). La intro a la UI actual es la
  lección 0 de ambos.
- **Lecciones interactivas**: pasos de tres tipos. **Explicar** (resaltar y
  leer, como el tutorial actual). **Hacer**: el hueco del recuadro deja pasar
  el toque solo al control del paso, y el paso se da por hecho cuando se
  cumple un **objetivo comprobado por estado** (la consigna subió, el diálogo
  se abrió, el modo cambió), no solo por el clic. **Pregunta**: tres opciones
  en el bocadillo; si se falla se explica la respuesta y se repite. Cuando el
  paso abre un asistente completo, el tutorial pasa a **modo libre**: sin
  sombras, bocadillo plegado en una esquina con la instrucción, esperando el
  objetivo.
- **Seguridad clínica**: una lección interactiva solo arranca **sin terapia
  activa, sin alarma, con enlace y sin apagado en curso**; con un bebé en
  tratamiento se ofrece en modo demostración (pasos "hacer" convertidos en
  "explicar"). Rótulo fijo **FORMACION** durante toda la lección. Cualquier
  alarma o pérdida de enlace aborta y restaura. Al salir (también a medias)
  se sale del modo formación y la UI vuelve al estado real de la placa.
- **Identidad y acreditación**: al empezar un curso se piden nombre o
  iniciales con el teclado en pantalla. Cada lección superada se guarda en
  NVS (curso, lección, intentos de las preguntas). Al superar el curso se
  guarda el registro (nombre, curso, fecha, intentos) y se muestra un
  **certificado QR `mailto:`** al responsable de formación con esos datos y
  el número de serie del equipo, misma vía que el contacto con soporte.
- **Lecciones** (detalle y objetivos en `design.md`):
  - Enfermería: 0 intro; 1 temperatura por aire; 2 piel y sonda; 3 humedad;
    4 fototerapia segura; 5 atender una alarma; 6 alta y seguimiento del
    bebé; 7 salida del bebé; 8 bloqueo; 9 tendencia; 10 hora; 11 soporte.
  - Técnico: 0 intro; 1 información y versiones; 2 WiFi y servidor; 3 idioma
    y modos; 4 hora; 5 alarmas técnicas y qué revisar; 6 actualización por
    el servidor web local; 7 informe de soporte; 8 apagado seguro.
- **Entrega por fases**, cada una mergeable: (1) motor + modo formación +
  lecciones 5 y 1 de enfermería para validar en banco; (2) resto del curso
  de enfermería; (3) curso técnico.

## Capabilities

### New Capabilities

- `hmi-training-courses`: selector de cursos, motor de lecciones
  interactivas, modo formación, progreso, certificado.

### Modified Capabilities

- `hmi-help-center`: la opción TUTORIAL GUIADO pasa de arrancar el recorrido
  pasivo a abrir el selector de cursos. El recorrido pasivo sobrevive como
  lección 0. El resto del menú (vídeo, contacto) no cambia.

## Impact

- **Código Display_HMI**: `src/ui/HelpTour.cpp` evoluciona a motor de
  lecciones (`src/ui/training/`: runner, selector, tablas de lecciones por
  curso, progreso, certificado); `src/tasks/CommTask.cpp` gana el gate de
  modo formación en el envío y en `Display_ApplyCtrlState()`; `BabyWizard`,
  `BabyExitDialog`, `TimeDialog`, `BabyHistory`, `TelemetryHistory` exponen
  `_IsOpen()`/`_Cancel()` y un punto de respuesta simulada;
  `UITask.cpp` añade la exención de auto-bloqueo y el rótulo FORMACION.
- **NVS**: nuevo namespace `hmi_train` (progreso por curso, registro de
  superados). Escritura desde la tarea UI fuera de `LVGL_Lock()`, con el
  patrón `eepromDirty` existente.
- **Protocolo serie**: sin cambios. `Firmware/PROTOCOL.md` intacto. La
  motherBoard no sabe que hay una formación en curso: simplemente no recibe
  órdenes distintas de las que ya tenía.
- **Flash**: cada lección son ~10 pasos × 3 idiomas ≈ 4-6 KB de texto; 21
  lecciones ≈ 100-120 KB más el motor. Hoy quedan ~630 KB.
- **Idiomas**: la rama `feat/hmi-i18n-catalogo` (portugués, catálogo único)
  no incluye la ayuda. Los textos del curso se escriben con el mismo
  `TXT(es,en,fr)` de la ayuda y se migran al catálogo cuando se integren las
  dos ramas; se anota en `tasks.md`.
- **known_issues.md**: #1 alarmas fantasma y #2 inundación UART: el modo
  formación reduce tráfico (no manda cambios), no lo aumenta; #3 desync de
  idioma: cambiar idioma en la lección técnica sí se envía (es la única
  excepción del gate, ver design). Ninguno se reintroduce.
- **Verificación**: manual en el CrowPanel con motherBoard conectada, sin
  bebé. Display_HMI no tiene entorno de test; se documenta qué se probó.
