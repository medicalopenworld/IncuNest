## Context

Display_HMI, LVGL 8.3. Hechos del código que fijan el diseño (dev `2a5fd92`,
2026-09-10):

- **Bebes** (`src/ui/BabyHistory.cpp`): overlay hijo de `ui_ScreenMain`
  (tarjeta 660×430), máquina de estados `HistStep` (Closed / LoadingActive /
  LoadingArchived / Showing / DischargeDialog / WaitingDischargeAck /
  LoadingChart / ShowingChart), `BabyHistory_Poll()` consume `g_pending*` con
  timeout de 2 s y un reintento, y cierra ante alarma crítica. Expone
  `_IsOpen()` / `_Close()` para el motor de lecciones.
- **Asistente** (`src/ui/BabyWizard.cpp`): crea el perfil con
  `Communication_SendProfileNew()` al confirmar las semanas (antes del peso)
  porque la placa necesita el perfil para calcular el rango. Sus teclados son
  `lv_btnmatrix` por instancia, no `lv_keyboard` (cuyo mapa es global en LVGL
  8.3 y pisaría el teclado de credenciales WiFi); el mapa de letras no tiene
  coma (protocolo CSV) y `onNameChanged` la filtra por si acaso.
- **Placa**: `babyStore_createProfile()` persiste el slot y `nextSeq` en NVS
  en el propio `PROFILE_NEW`, sella `admissionEpoch = now` y marca
  atributos/telemetría sucios. `CommTask.cpp` de la motherBoard guarda
  `s_wizardSeq` en `PROFILE_NEW` / `PROFILE_SELECT` / `PROFILE_WEIGHT` y lo
  sella como `activeSeq` en el flanco "ninguna terapia → alguna". Con tres
  slots llenos desaloja por FIFO el que no sea `activeSeq`
  (`baby_pick_eviction_slot`); el desalojo archiva con resultado Desconocido.
- **Formación** (`src/state/training_mode.cpp`, ADR-0002): la lista se
  contesta en local con un único bebé ZOE (`0xFFFF`); `Training_SimProfileNew`
  existe pero es código muerto porque el asistente rechaza BEBE NUEVO
  (`trainingRefuse`). Cada lección entra y sale del modo (`Training_Enter` /
  `Training_Exit` resetean el estado simulado). El motor borra el perfil
  recordado al salir solo si es ZOE (`training_engine.cpp:291`); la lección 1
  comprueba `BabyWizard_GetActiveSeq() == TRAINING_BABY_SEQ`.
- **Pasos libres** del motor: sombras fuera, instrucción en la franja
  inferior (32 px, translúcida y no clicable); el SALIR de la franja se
  esconde mientras haya un diálogo de `ui_ScreenMain` abierto porque el
  teclado del asistente llega hasta y=456 (`training_engine.cpp:697+`). Ese
  criterio hoy no incluye a `BabyHistory`.

## Goals / Non-Goals

**Goals**

- Registrar un bebé (nombre, semanas, peso opcional) desde Bebes, sin
  encender ninguna terapia, con un único punto de confirmación.
- Que el asistente y Bebes compartan el mismo teclado y la misma validación,
  sin duplicarlos.
- Que la lección 1 de Enfermería enseñe ese flujo de verdad, en modo
  formación, sin crear ningún registro real.

**Non-Goals**

- Editar o borrar un registro existente (nombre, semanas). No se pide.
- Cambiar el protocolo o la motherBoard (sello de `activeSeq`, FIFO).
- Registrar el peso de un bebé ya activo desde Bebes: eso sigue haciéndose
  desde el asistente, que es donde la placa lo necesita para el rango.

## Decisions

### 1. El flujo confirma al final, no a mitad

El asistente manda `PROFILE_NEW` al confirmar las semanas porque después
necesita el `seq` para pedir el rango con el peso. Bebes no necesita rango:
recoge nombre → semanas → peso (o SIN PESO) y envía **todo al pulsar
REGISTRAR** en la última pantalla. Así la X en cualquier pantalla anterior
no deja rastro (ni en la placa ni en ThingsBoard), y el operador entiende
que hay un único botón que registra. Secuencia:

```
REGISTRAR ──► HMI,PROFILE_NEW,name,gest ──► CTRL,PROFILE_ACK,seq
   seq == 0 ─► toast "Registro rechazado por la placa" ─► lista
   sin peso ─► toast "Bebe registrado" ─► recarga lista
   con peso ─► HMI,PROFILE_WEIGHT,seq,grams ─► CTRL,PROFILE_RANGE (se consume
              y se ignora) ─► toast "Bebe registrado" ─► recarga lista
```

Con SIN PESO no se envía `PROFILE_WEIGHT,SKIP`: el perfil ya está persistido
en `PROFILE_NEW` y SKIP no cambia nada en la placa; ahorra una ida y vuelta.
El `CTRL,PROFILE_RANGE` de respuesta al peso se consume aquí
(`g_pendingProfileRange = false`) para no dejar una respuesta huérfana que
el asistente descartaría como obsoleta en su siguiente uso.

Timeouts: 3 s para el ACK (como `ACK_TIMEOUT_MS` del asistente) y para el
rango; sin reintento del `PROFILE_NEW` (reintentarlo crearía dos perfiles si
el primero llegó y el ACK se perdió). Al vencer: toast "Sin respuesta de la
placa" y recarga de la lista, que es la verdad.

### 2. Guarda de tres activos en la HMI

`s_active.count >= 3` → el botón avisa ("Ya hay 3 bebes activos: da de alta
a uno antes") y no abre el flujo. Alternativa descartada: dejar que la placa
desaloje por FIFO como hace con el asistente. En el asistente el desalojo es
el mal menor (hay una terapia que arrancar); en Bebes el alta con resultado
clínico está en la misma pantalla y perder un paciente activo con resultado
"Desconocido" sería un fallo de registro clínico, no una comodidad.

### 3. Teclado compartido: `ui/InputKeypad.{h,cpp}`

Sale de `BabyWizard.cpp`, sin cambiar su comportamiento: `KB_LETTERS_MAP` /
`KB_DIGITS_MAP` con sus `ctrl` maps, `onKeyPress`, el filtro de comas del
nombre y `readNumericInput`. API:

```c
lv_obj_t *InputKeypad_Create(lv_obj_t *parent, lv_obj_t *ta, bool digits);
void      InputKeypad_StripCommasCb(lv_event_t *e);   // LV_EVENT_VALUE_CHANGED del textarea de nombre
bool      InputKeypad_ReadNumber(lv_obj_t *ta, uint32_t lo, uint32_t hi, uint32_t *out);
```

Los comentarios que justifican `lv_btnmatrix` frente a `lv_keyboard` y la
ausencia de la tecla coma viajan con el código: son la razón de ser del
módulo. `buildInputStep()` (layout de una pantalla del asistente: título,
textarea, pista, ATRAS / SALTAR / CONTINUAR) **no** se comparte: Bebes no
tiene SALTAR y su tercera pantalla lleva SIN PESO / REGISTRAR; compartir el
layout obligaría a parametrizar tres botones para dos usos.

Alternativa descartada: un quinto `WizTarget::Register` en el asistente.
Reutilizaría todo, pero mezclaría "registrar sin terapia" en el módulo cuya
razón de ser es gatear la activación de una terapia, con ramas nuevas en
`cancelWizard`, el ACK (no debe fijar la sesión) y `finishWizard`. El
asistente es camino clínico; se toca solo para mover código, no para darle
un modo más.

### 4. Bebes se agranda como el asistente

Las tres pantallas de entrada usan la tarjeta grande del asistente (780×460)
para que las teclas tengan el mismo tamaño; la lista y la gráfica vuelven a
660×430. `setCardSize(big)` local, mismo patrón que `BabyWizard.cpp`.

### 5. Estado público para el motor de lecciones

```c
typedef enum { BH_CLOSED, BH_LIST, BH_NEW_NAME, BH_NEW_GEST, BH_NEW_WEIGHT,
               BH_NEW_WAITING, BH_CHART } BabyHistoryStep;
BabyHistoryStep BabyHistory_GetStep(void);
uint32_t        BabyHistory_LastRegisteredSeq(void);  // 0 hasta el primer ACK de esta apertura
```

Mismo criterio que `BabyWizard_GetStep()`: fases agrupadas, sin exponer el
enum interno. Los objetivos de la lección se evalúan por estado en
`Training_Poll()`; `LastRegisteredSeq` es el estado "ya registró uno".

### 6. Formación: un bebé de prácticas registrado por el alumno

`training_mode.cpp` guarda **un** bebé de prácticas adicional
(`TRAINING_NEW_BABY_SEQ = 0xFFFE`, nombre y semanas que tecleó el alumno,
peso si lo dio). `Training_SimProfileNew` deja de ignorar el nombre y lo
rellena; `SIM_LIST` devuelve ZOE y, si existe, ese bebé;
`Training_SimProfileSelect(seq)` contesta con el `seq` pedido si es uno de
los dos de prácticas (y carga sus semanas y peso para el rango), ZOE en
cualquier otro caso. `Training_Enter()` lo borra: vive lo que dura la
lección. `Training_IsPracticeSeq(seq)` sustituye a las comparaciones con
`TRAINING_BABY_SEQ` en el motor (borrado del perfil recordado al salir) y
en la lección 1 (objetivo "bebé admitido").

Un segundo registro en la misma lección sobrescribe al primero: la lista de
prácticas nunca pasa de dos, y la lección solo pide uno.

Alternativas descartadas: (a) interceptar REGISTRAR en formación con un
aviso "no se crea nada" — el alumno no ve el resultado del flujo, que es lo
que se enseña; (b) devolver ZOE como ACK del registro — el alumno teclea
ANA y ve ZOE.

El asistente sigue rechazando BEBE NUEVO y SALTAR en formación
(`trainingRefuse`): el registro se enseña desde Bebes y el asistente enseña
la selección. El aviso "En formacion, selecciona a ZOE" sigue siendo cierto
(ZOE siempre está en la lista).

### 7. Lección 1 de Enfermería

Pasos libres por pantalla, como hace la lección 2 con el asistente:

1. Explicar (Bebes): qué es el registro; se crea al ingresar desde Bebes o,
   si no se hizo, desde el asistente al encender la primera terapia.
2. Hacer: tocar Bebes.
3. Libre (objetivo `BH_NEW_NAME`): tocar BEBE NUEVO.
4. Libre (`BH_NEW_GEST`): nombre o iniciales, CONTINUAR.
5. Libre (`BH_NEW_WEIGHT`): semanas de gestación y por qué importan.
6. Libre (`LastRegisteredSeq != 0` y `BH_LIST`): peso o SIN PESO, REGISTRAR;
   el bebé aparece en Activos.
7. Libre (Bebes cerrado): explorar y cerrar con la X.
8. Hacer: encender la temperatura → asistente.
9. Libre (bebé de prácticas admitido): seleccionar al recién registrado,
   peso, días de vida, APLICAR.
10. Explicar: cada peso nuevo se registra desde el asistente y dibuja la
    curva en Bebes.
11. Hacer: apagar la temperatura.
12. Pregunta (sin cambios).

El SALIR de la franja se esconde también con Bebes abierto en pantalla de
teclado (mismo motivo que con el asistente: la barra de espacio queda
debajo). `mainDialog` incorpora `BabyHistory_IsOpen()`.

### 8. La motherBoard no cambia

`PROFILE_NEW` desde Bebes deja `s_wizardSeq = nuevo` en la placa. Efecto:
si después alguien enciende una terapia y pulsa SALTAR en el asistente, la
placa sella como activo al último registrado. Hoy ya sella al último
creado/seleccionado, que puede ser un bebé dado de alta hace días (deuda
"`s_wizardSeq` no se limpia al dar de alta", ADR-0002). Con selección
explícita —el camino normal, que el asistente fuerza salvo SALTAR— el sello
es correcto. No se toca la placa en este cambio; la deuda sigue anotada con
la misma solución pendiente: limpiar `s_wizardSeq` al dar de alta y no
sellar con SALTAR.

## Risks / Trade-offs

- **ACK perdido tras un `PROFILE_NEW` que sí llegó** → el perfil existe y la
  HMI avisa "sin respuesta"; al recargar la lista aparece. Por eso no se
  reintenta el `PROFILE_NEW`: un reintento crearía un duplicado.
- **Peso duplicado el mismo día**: registrar con peso desde Bebes y volver a
  teclearlo en el asistente minutos después añade dos puntos iguales; la
  placa deduplica si el valor no cambió (`weightAppend`). Si cambió, son dos
  medidas reales.
- **Formación con paciente real registrado** (permitido por decisión del
  usuario): la lista de formación sigue ocultando a los reales; el alumno
  ve a ZOE y a su bebé de prácticas. El sello de minutos al paciente real
  depende del arreglo pendiente en la motherBoard, sin cambios aquí.
- **NVS en formación**: `Maintenance_Tick()` sigue `BabyWizard_GetActiveSeq()`
  y anota un cambio de paciente en NVS; con ZOE ya podía ocurrir y con el
  bebé de prácticas también. Se comprueba en la implementación si el tick
  corre en formación y, si lo hace, se salta con `Training_IsActive()`
  (regla de `embedded-display-hmi.md`: efectos fuera del protocolo).

## Migration Plan

Sin migración: ningún dato nuevo en NVS ni cambio de protocolo. Las dos
placas siguen siendo compatibles con cualquier versión ≥ v2.2.0 del
protocolo.

## Open Questions

- Ninguna bloqueante. Queda para el banco comprobar la ergonomía del teclado
  de letras dentro de la tarjeta de Bebes con la franja de formación debajo.
