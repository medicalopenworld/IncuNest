## ADDED Requirements

### Requirement: El tutorial guiado ofrece dos cursos con lecciones y progreso

La opción TUTORIAL GUIADO del menú de ayuda SHALL abrir un selector con dos
cursos, **Enfermería** y **Técnico**. Cada curso SHALL listar sus lecciones
con su estado (superada / pendiente / demostración) y el nombre del alumno
en curso si lo hay. La lección 0 de ambos cursos SHALL ser la introducción a
la interfaz (el recorrido pasivo existente).

#### Scenario: Elegir curso y alumno
- **WHEN** el operador entra en TUTORIAL GUIADO y elige Enfermería
- **AND** no hay progreso guardado para ese curso
- **THEN** se le pide nombre o iniciales con el teclado en pantalla y se
  muestra la lista de lecciones, todas pendientes
- *(Verificación manual en banco; Display_HMI no tiene entorno de test.)*

#### Scenario: Continuar un curso empezado
- **WHEN** hay progreso guardado para Enfermería a nombre de "ANA"
- **THEN** el selector ofrece "Continuar como ANA (n/12)" y "Nuevo alumno"
- **AND** "Nuevo alumno" borra el progreso de ese curso tras confirmar
- *(Verificación manual en banco.)*

### Requirement: Modo formación — la incubadora actúa de verdad, el bebé es virtual

Mientras una lección interactiva esté en curso, la HMI SHALL estar en modo
formación. La **actuación SHALL ser real**: consignas, toggles y fototerapia
se envían a la motherBoard como en operación normal y el `CTRL,STATE` se
aplica a la UI (calefactor y lámpara se encienden de verdad, con la cabina
vacía por el gate clínico). El **bebé SHALL ser virtual**: la lista de
perfiles del asistente SHALL contener un único bebé de prácticas, ZOE
(`seq 0xFFFF`), el asistente SHALL rechazar BEBE NUEVO y SALTAR con un
aviso, y selección, peso y edad SHALL contestarse en local con los mismos
flags que pone el parser. `CommTask` NO SHALL enviar a la motherBoard
ninguna trama de perfil (nuevo, selección, peso, edad, alta, canguro) ni de
hora ni de credenciales WiFi. Las órdenes al sistema de alarmas
(`ALM_SILENCE`, `ALM_TEST`) SHALL seguir saliendo. Los botones de conexión
WiFi SHALL rechazarse con un aviso. Nada cambiado durante la lección SHALL
persistirse en NVS. Al salir, la HMI SHALL restaurar el estado local previo
y enviarlo de inmediato a la placa, que SHALL volver al estado que tenía
(todo apagado si así estaba); ZOE SHALL desaparecer.

#### Scenario: La consigna llega a la placa y el calefactor actúa
- **WHEN** el alumno sube la consigna de aire dos pasos en la lección de
  temperatura, con el control encendido
- **THEN** la pantalla muestra la consigna nueva
- **AND** la motherBoard recibe la consigna nueva (monitor serie:
  `desiredAirTemperature` cambia) y el calefactor arranca sobre la cabina
  vacía
- *(Verificación manual en banco con la placa conectada, sin bebé.)*

#### Scenario: La fototerapia se enciende de verdad
- **WHEN** el alumno confirma la protección ocular en la lección de
  fototerapia
- **THEN** la lámpara se enciende y la cuenta atrás corre en pantalla
- **AND** al terminar la lección la lámpara se apaga sola
- *(Verificación manual en banco.)*

#### Scenario: Solo se puede practicar con ZOE
- **WHEN** el asistente del bebé se abre durante una lección
- **THEN** la lista muestra únicamente "ZOE - EG 32 sem - 1500 g"
- **AND** BEBE NUEVO y SALTAR responden con el aviso "En formacion,
  selecciona a ZOE" sin avanzar
- **AND** seleccionar ZOE lleva al peso, la edad, el rango propuesto y
  APLICAR como con un bebé real
- *(Verificación manual en banco.)*

#### Scenario: ZOE no queda en ningún registro
- **WHEN** el alumno completa el asistente con ZOE y termina la lección
- **THEN** la motherBoard no recibe `HMI,PROFILE_*` (monitor serie), Bebés
  no muestra a ZOE y en ThingsBoard no aparece ningún `baby_seq` nuevo
- *(Verificación manual en banco con la consola de ThingsBoard.)*

#### Scenario: Al salir todo vuelve a como estaba
- **WHEN** el alumno sale de una lección a mitad, con el control encendido,
  la consigna cambiada y el asistente abierto
- **THEN** el asistente se cierra, consigna y toggles vuelven a su estado
  previo, la placa recibe ese estado en menos de 1 s y apaga lo que la
  lección encendió, y ningún valor cambiado queda en NVS
- *(Verificación manual en banco.)*

### Requirement: Gate clínico y aborto

Una lección interactiva SHALL arrancar solo si no hay terapia activa
(`UI_AnyControlActive()` falso), no hay alarma activa, el enlace con la
placa está vivo, no hay perfil de bebé activo y no hay apagado en curso. Si
alguna condición falla, la lección SHALL ofrecerse en modo demostración
(pasos "hacer" mostrados como "explicar", sin modo formación) y NO SHALL
contar como superada. Durante una lección interactiva, cualquier alarma,
pérdida de enlace o apagado SHALL abortarla y restaurar el estado. Una
franja fija "MODO FORMACION" SHALL ser visible mientras dure.

#### Scenario: Con un bebé en tratamiento solo hay demostración
- **WHEN** el control de temperatura está activo y el operador abre la
  lección de humedad
- **THEN** se avisa de que la lección se hará en demostración, los pasos
  "hacer" no dejan tocar, y al terminar la lección sigue marcada pendiente
- *(Verificación manual en banco.)*

#### Scenario: Una alarma aborta la lección
- **WHEN** en mitad de una lección interactiva la motherBoard anuncia una
  alarma de cualquier prioridad
- **THEN** la lección se cierra, el estado local se restaura, se sale del
  modo formación y la pantalla vuelve a `ui_ScreenMain` con la alarma
  visible
- **AND** el selector de cursos NO se reabre: no queda ningún overlay de
  formación sobre la pantalla clínica
- *(Verificación manual en banco con la prueba de alarmas.)*

#### Scenario: El selector también cede
- **WHEN** el selector de cursos está abierto y llega una alarma, se pierde
  el enlace o pasan 3 min sin tocar
- **THEN** el selector se cierra solo y la pantalla vuelve a ser la principal
- *(Verificación manual en banco.)*

### Requirement: Pasos "hacer" con toque real y objetivo por estado

En un paso "hacer", solo el control del paso SHALL recibir toques; el resto
de la pantalla SHALL quedar bloqueado y atenuado. El paso SHALL darse por
hecho cuando se cumpla su objetivo comprobado por estado en la siguiente
pasada del bucle de UI, no por el evento de clic. Un objetivo ya cumplido al
entrar en el paso SHALL saltarlo. En un paso "libre" (asistentes completos)
las sombras SHALL retirarse y el bocadillo SHALL plegarse a una franja con
la instrucción y SALIR.

#### Scenario: Solo el control del paso responde
- **WHEN** el paso pide tocar AIRE y el alumno toca PIEL, el candado o
  Ajustes
- **THEN** no pasa nada
- **AND** al tocar AIRE el panel se selecciona y el tutorial avanza
- *(Verificación manual en banco.)*

#### Scenario: Objetivo por estado
- **WHEN** el paso pide subir la consigna 0,4 °C y el alumno pulsa la
  flecha dos veces
- **THEN** el paso avanza tras la segunda pulsación, no tras la primera
- *(Verificación manual en banco.)*

### Requirement: Preguntas con reintento

Un paso "pregunta" SHALL mostrar tres opciones. Acertar SHALL mostrar un
refuerzo breve y permitir SIGUIENTE. Fallar SHALL mostrar la explicación,
contar un intento y repetir la pregunta. Los intentos SHALL acumularse por
lección y por curso.

#### Scenario: Fallo y reintento
- **WHEN** el alumno elige una opción incorrecta
- **THEN** ve por qué es incorrecta y la pregunta se repite; el contador de
  intentos de la lección sube en uno
- *(Verificación manual en banco.)*

### Requirement: Progreso y certificado

El progreso (alumno, lecciones superadas, intentos) SHALL guardarse en NVS,
namespace `hmi_train`, desde la tarea UI fuera de `LVGL_Lock()`. Un curso
SHALL considerarse superado cuando todas sus lecciones estén superadas en
modo interactivo. Al superarlo SHALL guardarse un registro (nombre, curso,
fecha, intentos) en un anillo de 16 y mostrarse un certificado con QR
`mailto:` a `TRAINING_EMAIL` (por defecto `SUPPORT_EMAIL`) con asunto
`IncuNest SN <serie> - Certificado <curso> - <nombre>` y cuerpo con fecha,
lecciones, intentos y versión de firmware. Los certificados SHALL poder
consultarse desde el selector.

#### Scenario: Superar el curso
- **WHEN** el alumno supera la última lección pendiente de Enfermería
- **THEN** aparece la pantalla de certificado con el QR y el resumen
- **AND** tras reiniciar el equipo el certificado sigue en la lista
- *(Verificación manual en banco: escanear el QR con un móvil.)*

#### Scenario: El progreso sobrevive al reinicio
- **WHEN** el alumno supera dos lecciones y el equipo se apaga y encienda
- **THEN** el selector muestra las dos lecciones como superadas a su nombre
- *(Verificación manual en banco.)*

### Requirement: Lecciones del curso de Enfermería

El curso de Enfermería SHALL contener, en este orden: 0 introducción a la
interfaz; 1 temperatura por aire; 2 control por piel y sonda; 3 humedad; 4
fototerapia segura; 5 atender una alarma; 6 alta y seguimiento del bebé; 7
salida del bebé; 8 bloqueo de pantalla; 9 tendencia; 10 ajustar la hora;
11 contactar con soporte. Las lecciones 1 a 7 y 10 SHALL ser interactivas
(modo formación); 0, 8, 9 y 11 no cambian estado y no lo requieren.

#### Scenario: Lección 1, temperatura por aire (fase 1)
- **WHEN** el alumno sigue la lección
- **THEN** los pasos son: activar con el toggle (aparece el asistente del
  bebé en paso libre; SALTAR o completarlo lo cierra y activa); leer que el
  control ha quedado en AIRE (explicar); subir la consigna dos pasos con la
  flecha; leer medida frente a consigna (explicar); apagar con el toggle;
  pregunta sobre qué significa la cifra grande
- *(Verificación manual en banco.)*

#### Scenario: Lección 5, atender una alarma (fase 1)
- **WHEN** el alumno sigue la lección
- **THEN** los pasos son: abrir el centro de alarmas desde el icono; leer
  título y acción recomendada (explicar); localizar el botón de pausa de
  audio (explicar, en formación no se envía); cerrar; abrir el registro
  desde el check de "todo OK"; pregunta sobre la duración de la pausa
- *(Verificación manual en banco.)*

### Requirement: Lecciones del curso Técnico

El curso Técnico SHALL contener, en este orden: 0 introducción; 1
información y versiones; 2 WiFi y servidor; 3 idioma y modos; 4 ajustar la
hora; 5 alarmas técnicas y qué revisar; 6 actualización de firmware por el
servidor web local; 7 informe de soporte; 8 apagado seguro. En formación, la
lección 2 NO SHALL aplicar credenciales WiFi reales; la lección 3 SHALL
cambiar el idioma de verdad y restaurarlo al salir.

#### Scenario: Lección 3 restaura el idioma
- **WHEN** el alumno cambia el idioma a inglés durante la lección y sale
- **THEN** la interfaz vuelve al idioma que tenía antes de la lección
- *(Verificación manual en banco.)*
