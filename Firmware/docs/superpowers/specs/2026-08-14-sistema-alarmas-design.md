# Rediseño del sistema de alarmas — diseño

- **Fecha**: 2026-08-14 (revisado 2026-08-15 tras el análisis normativo)
- **Alcance**: `motherBoard`, `Display_HMI`, `shared/`, `PROTOCOL.md`
- **Estado**: diseño aprobado, pendiente de plan de implementación
- **Documentos hermanos**: `docs/alarms_normative_analysis.md` (análisis contra
  IEC 60601-1-8 y 60601-2-19), `docs/alarms.md` (comportamiento actual)

## 1. Objetivo y alcance

Rediseñar el ciclo de vida y la presentación de las alarmas: prioridades
derivadas de la norma, presentación diferenciada, aviso emergente en cada
alarma anunciable, e historial persistente con marca de tiempo.

**Fuera de alcance**: alarmas derivadas de SpO₂, frecuencia cardiaca o índice
de perfusión. El AFE4490 queda en stand by por decisión de producto.

**Fuera del alcance del firmware**, identificado durante el análisis normativo
y listado aquí para que no se pierda: el canal de corte térmico independiente
(§8.1), la reserva de energía para la alarma de corte de red de 10 min (§8.1),
la detección de cortocircuito en la sonda de piel (§8.1) y los objetivos
acústicos (§8.1). Ninguno se cierra en firmware.

## 2. Decisiones de arquitectura

### 2.1 La motherBoard es la única dueña de la lógica de alarmas

La MB detecta, decide prioridad, mantiene estado, genera el texto, gobierna el
audio y persiste el historial. El HMI presenta y envía `ack` / `silence`.

**Motivo**: quien controla los actuadores decide sobre las alarmas. El HMI es
un display no crítico que puede reiniciarse o perder el enlace sin que eso
afecte a la seguridad del paciente.

**Consecuencia aceptada**: el texto viaja ya traducido por el UART, lo que
mantiene vivo el modo de fallo de `known_issues.md` #3. Se mitiga en §6.1 con
un eco explícito del idioma.

### 2.2 La prioridad se compila en `shared/` y viaja en el cable

Tabla en `shared/alarm_ids.h`, compilada en ambas placas y evaluada por la MB;
el valor viaja en `CTRL,ALM`. El HMI **no la recalcula**.

**Motivo**: evita duplicar lógica de seguridad en el display y permite cubrir
la tabla con tests en host.

### 2.3 Tres prioridades, dos tratamientos visuales

Se conservan las tres prioridades de IEC 60601-1-8 en el análisis de riesgo y
en el manual, que es donde la norma comprueba conformidad, pero la UI se
simplifica a **una card parametrizada y un modal**.

**Motivo**: 6.3.2.2.2 permite indicar la prioridad en la señal de 1 m con uno,
dos o tres elementos (`!`, `!!`, `!!!`), de modo que no hacen falta tres
diseños gráficos. Colapsar a dos prioridades reales obligaría a subir siete
condiciones MEDIA a ALTA — desescalarlas no está permitido — lo que produce
14 ALTA de 16 y crea fatiga de alarma, que la norma define como peligro
(3.39) y señala como degradante de la efectividad del sistema.

### 2.4 El motor de estados va en `modules/control/alarm_machine`

Se amplía `motherBoard/src/modules/control/alarm_machine.{h,cpp}` hasta
contener la máquina de estados completa.

**Motivo**: ya está en el `build_src_filter` de `[env:native]`, así que ciclo
de vida, temporizadores y ring buffer quedan cubiertos por tests Unity en
host. Es la única parte del cambio que admite TDD estricto.

## 3. Línea base verificada

`docs/alarms.md` ha sido corregido en este cambio; contenía ocho afirmaciones
que el código no sostiene (umbral de corte de aire, `HUMIDITY_ERROR`, gracia
de spin-up, detección de sobretensión inexistente, cortes térmicos ajustables
en runtime, gateo del humidificador, número de puertas de trip-off y el
re-armado del mute).

### 3.1 Defectos preexistentes que este cambio arregla

**Desbordamiento de pila al parsear `CTRL,ALM`** — `CommTask.cpp:320-327`:
`char type[ALARM_TYPE_LEN]` son 30 bytes y `sscanf` usa `%31[^,]`, que escribe
hasta 32. Además `description` reserva 100 bytes pero se trunca a 31, así que
el `long_text` ya llega cortado hoy.

**El audio se apaga solo a los ~4 min** — `buzzerAlarmBeepCount` agota 500
toggles y nada lo re-arma. **IEC 60601-1-8, 6.10** solo admite que el audio
cese por acción del operador.

**El mute global sobrevive a alarmas nuevas** — `alarmsMuted` solo se limpia
cuando la lista queda vacía, así que silenciar humedad silencia un corte
térmico posterior. **6.8.1** exige que la inactivación de una alarma no afecte
a las señales de otras.

**El parámetro de frecuencia del buzzer es código muerto** — `buzzerTone()`
ignora `freq`; el tono lo fija `ledcSetup`.

**No existe ninguna alarma que nazca en silencio** — la rama `SILENCED_ALARM`
de `evaluateAlarm()` es inalcanzable (detalle en `docs/alarms.md` §2.2). El
estado `PENDING` de §5 hay que **construirlo**, no cablearlo.

## 4. Conjunto de alarmas y prioridades

Derivadas de la Tabla 1 de IEC 60601-1-8, evaluando el onset **con** el corte
automático de calefactor en su sitio, según la nota al pie de esa tabla.
Justificación completa en `docs/alarms_normative_analysis.md` §5.

| # | Condición | **Prioridad** | Corte de calefactor |
|---|---|---|---|
| 1 | Corte térmico de aire (> 38 °C en control por aire) | ALTA | sí — exigido |
| 2 | Corte térmico de piel (> 40 °C temperatura de incubadora) | ALTA | sí — exigido |
| 3 | Fallo del sensor de aire | ALTA | sí |
| 4 | Fallo de sonda de piel (desconexión, circuito abierto **o corto**) en modo PIEL | ALTA | sí — exigido |
| 5 | Fallo de sonda de piel en modo AIRE | BAJA | no |
| 6 | Fallo de giro del ventilador | ALTA | sí — exigido |
| 7 | Salida de aire obstruida | ALTA | **sí — exigido, hoy ausente** |
| 8 | Interrupción de la alimentación de red | ALTA | n/a — alarma de 10 min exigida |
| 9 | Desviación de temperatura de aire > +3 °C | MEDIA | **sí — exigido, hoy ausente** |
| 10 | Desviación de temperatura de aire > −3 °C | MEDIA | no — el calefactor debe seguir |
| 11 | Desviación de temperatura de piel > +1 °C | MEDIA | **sí — exigido, hoy ausente** |
| 12 | Desviación de temperatura de piel > −1 °C | MEDIA | no — el calefactor debe seguir |
| 13 | Fallo del calefactor | MEDIA | sí |
| 14 | Subtensión en el raíl de 12 V | MEDIA | no |
| 15 | Pérdida de enlace HMI ↔ motherBoard | MEDIA | no |
| 16 | Desviación de humedad | BAJA | no |

Reparto: **7 ALTA / 7 MEDIA / 2 BAJA**.

Cambios estructurales frente a los diez IDs actuales:

- **`TEMPERATURE_ALARM` se parte en cuatro** (9–12). La norma fija umbral
  distinto por modo (±3 °C aire, ±1 °C piel) y comportamiento de actuador
  distinto por dirección: el calefactor se corta solo por el lado caliente y
  **debe seguir encendido** por el frío. Un solo ID no puede expresarlo.
- **`SKIN_SENSOR_ISSUE` se parte por modo** (4 y 5) en vez de tener prioridad
  dinámica. La prioridad pasa a ser estática, que es mucho más fácil de
  declarar en el manual y de defender en el expediente. **Esto elimina el
  requisito de prioridad dinámica** que llevaba el diseño anterior.
- **Nueva: pérdida de enlace HMI ↔ MB** (15), elegida por el fabricante.
- **El fin del temporizador de fototerapia deja de ser alarma** y pasa a
  *information signal*, permitido por 6.1.2. IEC 60601-2-50 no añade alarmas.
- **`AIR_BLOCKED` sube a ALTA con corte de calefactor obligatorio** (7). Esto
  **revierte** la decisión previa de dejarla notify-only: **201.12.3.101** lo
  exige y no es una preferencia de diseño. Su detección, hoy compilada fuera
  por falta de calibración de `FAN_DUTY_BLOCKED_THRESHOLD`, pasa al camino de
  certificación: una alarma exigida no puede enviarse deshabilitada.

### 4.1 Decisiones de producto registradas

- **La alarma 14 (subtensión) no corta el calefactor.** Decisión del
  responsable de producto. Atenuante verificado: solo detecta subtensión
  (`0 V < system_voltage < 8 V`), que entrega *menos* potencia al calefactor,
  así que el escenario retirado no era de sobrecalentamiento. Es una alarma
  distinta de la 8, que sí es exigida por 201.12.3.103.
- **El humidificador sigue sin gatearse por alarmas.** Decisión del
  responsable de producto. Atenuante verificado: tiene protección propia
  independiente — en `HW_NUM >= 16`, `checkUsbFault()` lo apaga ante
  cortocircuito o sobrecarga sin levantar ninguna alarma. El comentario
  incorrecto de `security.cpp:399` se corrige.

## 5. Máquina de estados

```
INACTIVE ──condición──> PENDING ──expira retardo──> ACTIVE ⇄ SILENCED
                           │                          │  │
                           └──condición desaparece────┘  └──> ACKED
                                        │                       │
                                        └───────────────────────┴──> INACTIVE
```

- **`PENDING`** — condición presente dentro de la ventana de estabilización.
  Cuenta en bitmask y badge, **sin audio y sin modal**, pero **con card
  visible**: 201.12.3.104 exige que una alarma deliberadamente silenciada
  mantenga indicación visual.

  **201.12.3.104 legitima esta ventana explícitamente**: el AUDIO PAUSED de
  una incubadora calentando desde frío "puede ser de hasta 30 min". La ventana
  de 30 minutos que ya existe es conforme.

  **Los cortes térmicos 1 y 2 no pasan nunca por `PENDING`.** Hoy están
  armados con audio desde el primer instante por diseño deliberado, y es la
  conducta correcta.

- **`PENDING` → `ACTIVE`** — al expirar el retardo con la condición presente,
  la MB **reenvía** `CTRL,ALM` con `silent=0`, lo que dispara modal y audio.
  201.12.3.104 lo exige: los silenciados "deben reanudar automáticamente su
  función normal dentro de un tiempo especificado por el fabricante".

- **`SILENCED`** — audio inactivo durante un intervalo declarado en el manual.
  **No existe el tope de 2 minutos** que se suponía: 6.8.5 solo obliga a
  declarar la duración, y a restringir el ajuste del máximo a la organización
  responsable. Card y parpadeo continúan.

- **`ACKED`** — inactiva el audio de esa alarma. **Card y señal de 1 m
  permanecen**: 6.8.1 dice que AUDIO PAUSED o AUDIO OFF "no deben inactivar
  las señales visuales de 1 m", aunque sí pueden inactivar las de 4 m o
  degradar la prioridad. Una alarma nueva re-arma el audio.

- **Alarmas *latching*** — los cortes térmicos 1 y 2 **deben persistir hasta
  reset manual** aunque la condición desaparezca (201.15.4.2.1 aa/bb). El
  resto son *non-latching* y se limpian solas. **La elección latching /
  non-latching no puede ser un ajuste de usuario** (6.10): queda fija en
  firmware.

- **Condiciones de corta duración** — 6.10: si la condición se va enseguida,
  una MEDIA debe completar **al menos una ráfaga entera** y una ALTA **media
  ráfaga**, salvo inactivación por el operador.

- **El silenciado nunca es global.** 6.8.1: la inactivación de una alarma o
  grupo no debe afectar a las señales de otras. Se sustituye el bit `mute`
  global actual por silenciado por ID.

## 6. Protocolo

Cambio *breaking* en ambos sentidos: las dos placas se flashean juntas, como
en la v2.0.0. Sin shim de compatibilidad.

### 6.1 `CTRL,ALM` extendido

```
CTRL,ALM,id,short_text,long_text,active,priority,silent,latched,epoch,lang
```

- `priority`: `0`=BAJA, `1`=MEDIA, `2`=ALTA, ya evaluada por la MB.
- `silent`: `1` en `PENDING`.
- `latched`: `1` si la condición ya desapareció pero la alarma sigue viva a la
  espera de reset manual.
- `epoch`: hora de activación, o `0` si no hay NTP (precedente:
  `dischargeEpoch=0`).
- `lang`: idioma con el que la MB generó el texto. Si no coincide con `g_lang`,
  el HMI reenvía su `lang` y pide re-anuncio. Mitiga `known_issues.md` #3.

Parseo con **conteo tolerante de campos**, como ya hace `CTRL,STATE`; el
parser actual exige `result == 4` exacto. Anchos de `sscanf` derivados de las
constantes. Línea malformada → descarte silencioso con log.

### 6.2 Comandos nuevos del HMI

```
HMI,ALM_ACK,id
HMI,ALM_SILENCE,id,ms
HMI,ALM_RESET,id          ← reset manual de una alarma latching
HMI,ALM_TEST              ← test de alarmas (201.12.3.105)
HMI,ALM_HISTORY_REQ
```

El bit `mute` global de `HMI,...` **se retira**: incumple 6.8.1.

### 6.3 Respuesta de historial

```
CTRL,ALM_HISTORY,n{,id,priority,raisedEpoch,clearedEpoch,limit,value,short_text}×n
```

Solo bajo demanda, nunca en la telemetría periódica (`known_issues.md` #2).

## 7. Presentación en el HMI

### 7.1 Un único overlay en `lv_layer_top()`

Card y modal son variantes de un overlay en `lv_layer_top()`, no hijo de una
pantalla. Un overlay parentado a una pantalla desaparece al cambiar de
pantalla — `BabyExitDialog.cpp` documenta ese problema — y las señales de
alarma deben existir mientras exista la condición.

LVGL en este proyecto es **8.3.11**, no v9.

### 7.2 La card es persistente en todas las pantallas

**Esto revierte la decisión previa de mostrarla solo en la pantalla de
bloqueo.** 6.3.2.2.2 exige al menos una señal visual que identifique la
**alarma concreta y su prioridad**, legible a 1 m. Fuera del bloqueo, una
MEDIA quedaba representada solo por un número en el badge, que no identifica
ni la condición ni la prioridad. 201.12.3.104 tiene el mismo problema con la
indicación mantenida de las alarmas silenciadas.

Una sola card parametrizada:

| Prioridad | Marcador | Color | Banda de 4 m |
|---|---|---|---|
| ALTA | `!!!` | rojo | roja, 2,0 Hz |
| MEDIA | `!!` | amarillo | amarilla, 0,66 Hz |
| BAJA | `!` | cian | cian, fija |

Muestra la alarma activa de mayor prioridad, su texto corto y la hora. Con
`epoch=0` muestra `—`. Mostrar solo la de mayor prioridad es conforme por 6.2
(*intelligent alarm system*: "no necesita generar señales simultáneamente para
todas las condiciones activas"), siempre que el criterio de orden se declare
en el manual.

### 7.3 Banda de 4 m

Elemento estrecho en el borde de la pantalla con el color y la frecuencia de
la Tabla 2, indicando la prioridad más alta activa. Requisito de 6.3.2.2.1
para sistemas destinados a estar cerca de otros sistemas de alarma, que es el
caso de una UCI neonatal con varias incubadoras.

Valores de la Tabla 2, ya verificados: ALTA rojo 1,4–2,8 Hz; MEDIA amarillo
0,4–0,8 Hz; BAJA cian o amarillo constante; duty 20–60 % (100 % en BAJA).

### 7.4 Badge de recuento

`ui_AlarmLockCont` / `ui_AlarmLockNumLabel` se mantienen. Siguen navegando a
`ui_ScreenAlarms` sin exigir desbloqueo, comportamiento ya existente que
además congela el autolock.

### 7.5 Modal bloqueante — prioridad ALTA, cualquier pantalla

Overlay al 70 % con `SILENCIAR`, `ACEPTAR` y, en alarmas *latching* cuya
condición ya desapareció, `RESET`. Solo para ALTA en `ACTIVE`. **No se
apilan**: uno solo con indicador "1 de 3" y navegación — conforme por
6.3.2.2.2, que admite que las condiciones simultáneas se indiquen "por acción
del operador".

### 7.6 Indicación de silenciado

Símbolo persistente mientras haya alguna alarma silenciada o aceptada, legible
a 1 m (6.8.5 y 201.12.3.104). Se reutiliza el asset `ui_img_mute_icon_png`.

### 7.7 Test de alarmas

Acción en Ajustes que dispara las señales audibles y visuales para que el
operador verifique su funcionamiento (201.12.3.105). Debe describirse en el
manual.

### 7.8 Idioma

Solo se traduce el cromo de la UI, con el patrón `const char*[3]` de
`UI_ApplyLanguage()`. Los textos de alarma llegan traducidos de la MB.

**No se generan fuentes nuevas ni se introducen acentos.** Las fuentes son
Montserrat built-in de LVGL, que solo cubren ASCII y símbolos; por eso todo el
texto actual está sin acentos. Es un cambio independiente. El selector de
idioma ya existe y ya persiste en NVS.

## 8. Audio (motherBoard)

- Se elimina el corte por tiempo: el patrón se re-arma mientras haya alguna
  alarma en `ACTIVE` no silenciada (6.10).
- Patrones de la Tabla 3, verificados: **ALTA** 10 pulsos, intervalo entre
  ráfagas 2,5–15 s; **MEDIA** 3 pulsos, 2,5–30 s; **BAJA** 1 o 2 pulsos,
  intervalo > 15 s o sin repetición. Diferencia de amplitud entre pulsos
  ≤ 10 dB.
- Tonos diferenciados por frecuencia quedan **fuera de esta iteración**:
  exigen reconfigurar `ledcSetup` por evento. La discriminación queda en el
  número de pulsos y el periodo, que ya distingue los tres niveles de oído.

### 8.1 Requisitos que el firmware no puede cerrar

Listados para que entren en el plan de hardware, no en este cambio:

- **Corte térmico independiente del termostato** (201.15.4.2.1 aa). Hoy el
  corte lee el mismo sensor que el PID: un fallo de sensor se lleva el control
  y su propia protección. Necesita un segundo canal de temperatura.
- **Umbral de corte acotado**: `airTemperatureSetMax` se puede escribir sin
  límite por USB y por `/config`. Debe clamparse a 38 °C en control por aire y
  40 °C en control por piel. El override hasta 39 °C que contempla la norma
  exige un **segundo** corte a 40 °C, que no existe.
- **Alarma de corte de red durante 10 min** (201.12.3.103): reserva de energía
  dedicada.
- **Detección de cortocircuito en la sonda de piel** (201.12.3.102): el
  timeout de 20 s no ve un corto.
- **Objetivos acústicos**: ≥ 65 dB(A) **a 3 m** (201.9.6.2.1.102) y ≤ 80 dB(A)
  dentro del compartimento (201.9.6.2.1.103). Tiran en sentidos opuestos y
  condicionan colocación del zumbador y acústica del habitáculo.

## 9. Historial (motherBoard)

- **Ring buffer de 10 entradas en un blob NVS único**, no 10 claves: escritura
  atómica y menos desgaste.
- Entrada: `{uint8 id, uint8 priority, uint32 raisedEpoch, uint32
  clearedEpoch, int16 limit, int16 value}` = 14 bytes → 140 bytes.
- Los campos `limit` y `value` los pide **6.12.2 a)**: el log debería incluir
  los límites de alarma cuando son ajustables por el operador —los cortes
  térmicos lo son— y el dato que causó la condición.
- **Una entrada por activación**; `clearedEpoch` se rellena en sitio en vez de
  gastar hueco. Con 10 huecos, registrar activación y resolución por separado
  hace que una alarma que rebota borre todo lo anterior.
- **El texto no se persiste**: se regenera al responder, en el idioma vigente.
  Evita que el historial quede congelado en el idioma de cuando saltó.
- 6.12.2 b) exige registrar **todas** las ALTA y MEDIA. Con 14 condiciones en
  esos dos niveles, 10 huecos es poco: conviene revisar el tamaño contra el
  espacio NVS disponible antes de fijarlo.

## 10. Verificación

### 10.1 Automatizable en host (`pio test -e native`)

Sobre `alarm_machine`:

- `PENDING` no genera audio ni modal, pero sí card.
- Al expirar el retardo con la condición viva se reanuncia con `silent=0`.
- Los cortes térmicos nunca entran en `PENDING`.
- `SILENCED` calla el audio y deja la señal visual; al expirar, el audio vuelve.
- `ACKED` calla el audio y conserva la señal de 1 m.
- Silenciar una alarma **no** afecta a las señales de las demás.
- Una alarma nueva re-arma el audio de las ya aceptadas.
- Las alarmas *latching* sobreviven a la desaparición de la condición y solo
  se limpian con `ALM_RESET`.
- Una condición de corta duración completa ráfaga entera (MEDIA) o media
  ráfaga (ALTA).
- El audio no cesa por tiempo con la condición activa.
- Ring buffer: `clearedEpoch` en sitio, envuelve a las 10, `epoch=0` se
  preserva, `limit` y `value` se guardan.

### 10.2 Verificación manual documentada

El HMI no tiene entorno de test. Se compila con `pio run -e main` y se prueba
en el CrowPanel, documentando en el commit: card en todas las pantallas, banda
de 4 m, modal, no apilamiento, badge, indicación de silenciado, test de
alarmas y los tres idiomas.

### 10.3 Fuera del alcance del agente

Las capturas en los tres idiomas y la grabación en hardware de la frecuencia
de parpadeo las produce el responsable del proyecto. No pueden ser criterio de
cierre por parte del agente, que no tiene acceso al hardware.

### 10.4 Verificación normativa pendiente

El análisis cubrió las cláusulas de alarmas, señales, cortes térmicos y
niveles sonoros de 60601-1-8 y 60601-2-19. **No se leyeron ambas normas
completas**, así que puede haber requisitos aplicables en secciones no
abiertas. Las mediciones acústicas y los ensayos de corte térmico descritos en
201.15.4.2.1 son ensayos de laboratorio, no de banco de desarrollo.

## 11. Plan de entrega

- **(A) Base** — fix del desbordamiento de `CTRL,ALM`, nuevo enum de 16
  condiciones en `shared/`, protocolo extendido, máquina de estados con sus
  tests en host, audio sin corte por tiempo, silenciado por ID.
- **(B) Conformidad de actuadores** — cortes de calefactor exigidos hoy
  ausentes (7, 9, 11), alarmas *latching* de los cortes térmicos con reset
  manual, clamp de los umbrales, corrección del comentario de
  `security.cpp:399`.
- **(C) Presentación** — card persistente, banda de 4 m, modal con cola,
  indicación de silenciado, test de alarmas.
- **(D) Historial** — ring buffer NVS, `CTRL,ALM_HISTORY`, pantalla de
  historial.

(B) es el bloque con impacto en seguridad del paciente y debería ir primero si
hay que priorizar. Requiere además actualizar `PROTOCOL.md`.
