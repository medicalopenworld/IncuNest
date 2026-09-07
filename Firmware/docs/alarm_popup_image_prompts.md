# Imágenes de pop-up de alarma — prompts para nano banana 2

Una imagen explicativa por condición de alarma, en el estilo y con la silueta
que fijan las dos imágenes base de `docs/alarm_images_refs/_comunes/`:
`IncuNest_alarm_base_FLAT.jpg` y
`IncuNest_alarm_base_with_baby_MIRRORED_FLAT.jpg`.

La segunda es el original **espejado**. El original tiene la cabeza del bebé a
la izquierda y la serie la quiere a la derecha; pedirlo por texto falló en 6 de
15 imágenes. Ver §3.0.

La lista de condiciones y sus textos salen de `shared/include/alarm_ids.h` y
`shared/src/alarm_text.cpp`. **Cada escena ilustra la ACCIÓN que pide
`alarm_action_text()`, no el fallo en abstracto**: al operador no le sirve ver
un ventilador roto, le sirve ver qué tiene que mirar.

---

## 1. Cómo usar esto

nano banana 2 (Gemini 3 Pro Image) acepta imágenes de referencia. El texto solo
**no** basta para clavar el estilo: la coherencia entre los assets actuales
viene de que comparten incubadora, bebé y trazo concretos, y eso se transfiere
mucho mejor por referencia que por descripción.

Flujo recomendado, por cada imagen:

1. Adjunta las dos imágenes base de `docs/alarm_images_refs/_comunes/`. Fijan a
   la vez trazo, paleta, silueta y cara del bebé (ver §3.0).
2. Pega el **bloque de estilo** (§2) + el **prompt de escena** (§4) de esa alarma.
3. Genera en 1:1, como las imágenes base.

---

## 2. Bloque de estilo (idéntico en las 20)

```
STYLE (match the attached reference image exactly):
Flat vector medical-infographic illustration, children's-book warmth, clean and
calm. Uniform medium-weight rounded outlines in dark slate blue (#2E4A5C) on
every object — no varying line weight, no sketchiness, no cross-hatching.
Palette, strictly: off-white cream background (#FCFCF5), pale blue (#BFD9E8)
and soft blue-grey (#A8C4D4) for equipment and fabric, teal (#3E8FA8) for the
frame ring, warm peach (#F5C9A0) for skin, pale sage green (#C8D9BE) for the
mattress base strip. The scene's alarm colour is used ONLY on the single
element the operator must look at.
ALARM COLOUR BY PRIORITY: each scene names its own alarm colour and uses ONLY
that one. Coral red (#E0553D) marks a HIGH-priority condition, warning amber
(#E8A33D) a MEDIUM one, and bright cyan (#12B5D6) a LOW one. Never more than one
of the three in the same image. This follows the alarm-priority colour coding of
IEC 60601-1-8, so it is not decoration: the three colours mean different things
to the operator.
The low-priority CYAN must be a bright, saturated, clearly BLUE cyan, plainly
different from the muted teal (#3E8FA8) of the frame ring — and it is always a
filled shape, never a thin outline, so it cannot be mistaken for the ring. Do not
use teal as an accent anywhere in any scene: that colour belongs to the ring.
ACCENT DISCIPLINE (critical): the scene's alarm colour must cover no more than
about 5% of the image, concentrated in ONE place. Everything the FOCUS line does
not name stays pale blue-grey, including cables, clamps, housings and buttons
near the focus point. If in doubt, use less of it.
The equipment's own pink parts (porthole latches, the connector panel) may be
drawn in a SOFT DESATURATED PINK (#E8C4CE) so the unit stays recognisable, but
that pink must always read as clearly paler and flatter than the alarm colour.
The alarm colour is the only saturated colour in the picture.
Soft flat fills with very subtle gradients; a light diagonal highlight on the
transparent dome; no drop shadows, no harsh contrast, no photorealism, no 3D
render, no textures.
COMPOSITION: the whole illustration sits inside a thin teal circle outline
(#3E8FA8, ~1% of canvas width) centred on a plain off-white square canvas, with
generous margin between the ring and the canvas edge. The subject is centred and
fully contained inside the ring.
CONTAINMENT (critical): every drawn element — the unit, the badge, cables — fits ENTIRELY inside the circle, with a clear band of
empty cream between the drawing and the ring itself. Nothing crosses the ring,
touches it, or is clipped by it. Scale the subject down as much as needed to
achieve this; a smaller drawing with air around it is always correct, a drawing
that spills past the circle is always wrong.
The inside of the circle is the SAME plain off-white cream as the rest of the
canvas. Never fill the circle with a coloured background. Never draw a room, a
floor, a wall, a shelf or an enclosing box around the subject — the incubator
itself floats on empty cream, with clear breathing space between it and the
ring, exactly as in the reference image. Interior details (heater, fan, wiring) are drawn ON the bare cream panel below
the mattress, never in a separate box of their own.
Always a FLAT FRONT ELEVATION, orthographic: the unit is seen straight on from
the front, symmetrical, with no perspective, no vanishing point, no depth and no
three-quarter views.
THE INCUBATOR — COPY THE ATTACHED BASE IMAGE EXACTLY. The first reference image
is the canonical drawing of this unit. Reproduce its silhouette part for part and
do not redesign it, rescale its proportions or rearrange its parts, no matter
what else the scene adds. From top to bottom it is:
 - a white arched shell with squared shoulders and a rounded top, carrying a
   white oval HANDLE across the top, with a small square latch block just below
   the handle;
 - a large transparent front panel filling the arch, tinted very pale blue with
   a soft diagonal highlight;
 - TWO large circular portholes side by side on that panel, equal in size, each
   a thick white ring; a pair of salmon-pink latches meets between the two
   portholes, and one more pink latch sits on the outer edge of each;
 - a plain SATURATED BLUE mattress pad, drawn as a simple slab, resting on a
   white tray that spans the full width;
 - two small white corner blocks under the tray, at the left and right ends;
 - below them a plain CREAM-BEIGE FRONT PANEL, drawn completely FLAT and EMPTY:
   no drawer outline, no recessed handle, no seams, no lines of any kind. It is a
   bare cream band, and it is where component badges go;
 - a SAGE GREEN base strip along the very bottom.
The badge or detail a scene adds is placed ON this unchanged unit — on the bare
cream panel, on the transparent panel, or in the air inside the hood. It never causes the unit
itself to be redrawn.
THE BABY (when present, keep identical across images): a simplified chubby
newborn lying on its back on the blue mattress, seen in profile, bald, two small
blush dots on the cheek, swaddled from the chest down in a pale grey-blue
blanket — as in the second reference image.
HEAD TO THE RIGHT, ALWAYS: the baby's head is at the RIGHT-HAND end of the
mattress and the swaddled body extends to the LEFT, exactly as in the attached
baby reference. Every image in the set puts the head on the right, with no
exceptions, so the operator's eye lands in the same place every time.
BOTH ARMS, ALWAYS: the baby has two arms and both are accounted for. The far
(right) arm is tucked INSIDE the blanket — its shape may show as a soft bulge
under the cloth, but it is never left out. Never draw a baby with one bare arm
and an empty, armless shoulder on the other side: that is an anatomy error, and
it has appeared repeatedly. The near (left) arm rests on the chest or is tucked
in too.
THE FACE — NEVER SMILING: these are ALARM images, and a smiling baby contradicts
the alarm. Eyes closed as two gentle curves, and a small mouth that is NEUTRAL or
slightly DOWNTURNED. No smile, no grin, no upturned mouth corners, no happy
expression of any kind. The face should read as asleep and a little unwell —
calm, but plainly not content.
THE PROHIBITION BADGE (recurring motif): a plain circular outline crossed by one
diagonal slash, with a simple flat symbol of the failed component drawn inside
it, drawn in the scene's alarm colour, for a component that has been CUT or has
FAILED. It always sits on or beside the part of the unit that holds that
component, never floating loose.
BADGE LEGIBILITY (critical): the badge must be BIG — roughly a third of the
width of the cream panel it sits on — and it sits on bare cream with nothing
behind it. The panel is empty by design (see above), so no line may ever cross
the badge or its symbol. The symbol inside is what the operator has to read.
THE HEATER SYMBOL (recurring): a flat horizontal zig-zag resistance ribbon —
the same shape every time, so it reads as "the heater" across images. It is NOT
a coil, NOT a spring, NOT a spiral.
NO HANDS, EVER: never draw a hand, a finger, an arm or a forearm anywhere in
the image, in any scene. The picture shows WHAT IS WRONG; the popup text
beside it tells the operator what to do, so no gesture is needed. Where an
action is implied, it is conveyed by the coral gap between the parts, not by
somebody reaching in.
THE BABY NEVER WEARS EYE COVERS, goggles or a phototherapy mask. That comes
from the attached style reference and does not belong in an alarm image.
HARD CONSTRAINTS: absolutely NO text, NO letters, NO numbers, NO logos, NO
watermarks, NO arrows with words anywhere in the image. Square 1:1.
```

**El color del acento codifica la prioridad** (decisión de 2026-09-06):
**coral `#E0553D` = ALTA**, **ámbar `#E8A33D` = MEDIA**, y nunca aparecen los dos
en la misma imagen. Coincide con el código de colores de prioridad de
IEC 60601-1-8, así que el color dice algo, no decora.

Esto sustituye a la regla anterior, que era «coral siempre, la prioridad la
comunica el marco del pop-up». Se cambió al pedir ámbar para la 12: dejarla como
única ámbar la habría convertido en la rara, y extender el criterio es lo que
hace que el color signifique algo.

> **Pendiente: las de prioridad BAJA (15, 16, 19) siguen en coral**, que es el
> color de ALTA. Eso ahora es una **contradicción**: antes el coral solo quería
> decir «mira aquí», pero desde que codifica prioridad, una alarma BAJA pintada
> de rojo dice lo contrario de lo que es. La Tabla 2 de IEC 60601-1-8 asigna
> **cian** a la prioridad baja; el problema es que el anillo del marco ya es
> teal y competirían. Hay que resolverlo antes de dar la serie por cerrada.

---

## 3. Nombres de fichero

Se propone indexar por `AlarmId` para que el código componga la ruta sin tabla
de traducción (`snprintf(path, "S:/alm_%02d.png", id)`):

La columna **Bebé** dice si el recién nacido aparece en la imagen y con qué
papel. Su criterio se explica en §3.1 — no es el mismo que la clasificación
normativa fisiológica/técnica, y esa diferencia es deliberada.

| Fichero | Alarma | Prio | Bebé |
|---|---|---|---|
| `alm_01_air_thermal_cutout.png` | `ALARM_AIR_THERMAL_CUTOUT` | ALTA | Contexto |
| `alm_02_skin_thermal_cutout.png` | `ALARM_SKIN_THERMAL_CUTOUT` | ALTA | **Protagonista** |
| `alm_03_air_sensor_fault.png` | `ALARM_AIR_SENSOR_FAULT` | ALTA | Contexto |
| `alm_04_skin_sensor_fault_skin_mode.png` | `ALARM_SKIN_SENSOR_FAULT_SKIN_MODE` | ALTA | **Protagonista** |
| `alm_05_fan_failure.png` | `ALARM_FAN_FAILURE` | ALTA | Contexto |
| `alm_06_air_outlet_blocked.png` | `ALARM_AIR_OUTLET_BLOCKED` | ALTA | Contexto |
| `alm_07_mains_interruption.png` | `ALARM_MAINS_INTERRUPTION` | ALTA | Contexto |
| `alm_08_air_temp_deviation_high.png` | `ALARM_AIR_TEMP_DEVIATION_HIGH` | MEDIA | Contexto |
| `alm_09_air_temp_deviation_low.png` | `ALARM_AIR_TEMP_DEVIATION_LOW` | MEDIA | Contexto |
| `alm_10_skin_temp_deviation_high.png` | `ALARM_SKIN_TEMP_DEVIATION_HIGH` | MEDIA | **Protagonista** |
| `alm_11_skin_temp_deviation_low.png` | `ALARM_SKIN_TEMP_DEVIATION_LOW` | MEDIA | **Protagonista** |
| `alm_12_heater_fault.png` | `ALARM_HEATER_FAULT` | MEDIA | Contexto |
| `alm_13_supply_undervoltage.png` | `ALARM_SUPPLY_UNDERVOLTAGE` | MEDIA | No |
| `alm_14_hmi_link_lost.png` | `ALARM_HMI_LINK_LOST` | MEDIA | Contexto |
| `alm_15_skin_sensor_fault_air_mode.png` | `ALARM_SKIN_SENSOR_FAULT_AIR_MODE` | BAJA | Contexto |
| `alm_16_humidity_deviation.png` | `ALARM_HUMIDITY_DEVIATION` | BAJA | No |
| `alm_17_heater_sensor_fault.png` | `ALARM_HEATER_SENSOR_FAULT` | MEDIA | No |
| `alm_18_sensorboard_link_lost.png` | `ALARM_SENSORBOARD_LINK_LOST` | MEDIA | Contexto |
| `alm_19_sensorboard_door_fault.png` | `ALARM_SENSORBOARD_DOOR_FAULT` | BAJA | No |
| `alm_display_link_lost.png` | enlace caído visto por el display | — | No |

Reparto: 4 protagonista, 11 contexto, 5 sin bebé.

### 3.0 La imagen base es la referencia canónica (2026-09-05)

`docs/alarm_images_refs/_comunes/IncuNest_alarm_base.jpg` y
`IncuNest_alarm_base_with_baby.jpg` son **la** referencia: fijan a la vez el
trazo, la paleta, la silueta de la incubadora y la cara del bebé. Si una imagen
generada no se parece a ellas, está mal.

Sustituyeron a `Baby_phototherapy_eyes_covering.png` y `Baby_place_sensor.png`,
que se venían usando como referencia de estilo. Dos motivos, y el segundo no era
evidente:

1. Dibujaban una incubadora genérica distinta de la IncuNest, y **la silueta se
   desestabilizaba en cuanto una escena añadía elementos** — al meter termómetro
   e insignia en la 1, el modelo recompuso la base entera como un pedestal
   estrecho; en la 12 movió los portillos encima del bebé y pintó un suelo verde.
2. La de fototerapia **contagiaba las gafas del bebé**, que llegaron a aparecer
   en la alarma de corte de red.

**Cuando el texto y la referencia discrepan, gana la referencia.** Es la lección
que ha salido dos veces:

- con la silueta, describir la IncuNest por texto mientras se adjuntaba una
  incubadora genérica producía un híbrido;
- con la orientación, pedir «cabeza a la derecha» mientras se adjuntaba una
  referencia con la cabeza a la izquierda falló en **6 de 15** imágenes.

Por eso la referencia del bebé que se adjunta es `_MIRRORED`, generada girando
el original: se arregla la referencia, no el prompt. La incubadora es simétrica,
así que espejarla no altera nada más. Si algún día cambia el original, hay que
regenerar el espejo — lo hace un `Image.transpose(FLIP_LEFT_RIGHT)`.

Consecuencia de forma que hay que respetar: la base es un **alzado FRONTAL**, no
lateral. Todas las escenas se reescribieron en consecuencia, y lo que antes era
«la bahía de servicio bajo la bandeja» es ahora **el frente del cajón crema**,
que es donde se apoyan las insignias.

### 3.2 Sin manos (decisión de 2026-09-04)

Ninguna escena dibuja manos. No es una preferencia estética: es la conclusión de
ocho intentos fallidos.

El modelo dibuja siempre un antebrazo que se come media composición y cruza el
anillo, y no lo corrige ninguna instrucción — se probó acotando el tamaño («del
tamaño de la cabeza del bebé»), prohibiendo el antebrazo («solo la mano,
recortada por la muñeca») y fijando un porcentaje del diámetro. Las tres
fallaron, y la del porcentaje además empeoró el resto de la imagen. La primera
imagen que salió limpia fue la primera sin mano.

La división que queda es coherente y además es la que ya tenía sentido: **la
imagen muestra QUÉ está mal; el texto del pop-up dice QUÉ HACER**. Donde había
una acción física (enchufar un conector, recolocar la sonda) la comunica el
hueco coral entre las dos piezas, que es más legible que unos dedos.

Vinculado a esto, el bloque de estilo prohíbe también las **gafas de
fototerapia**: se le pegaban de `Baby_phototherapy_eyes_covering.png`, que es
una de las referencias de estilo adjuntas, y aparecieron en la alarma de corte
de red.

### 3.3 Cuatro escenas aplazadas (2026-09-04)

No se generan todavía porque el componente que dibujan no existe hoy en el
equipo, o está sin definir:

| Escena | Motivo |
|---|---|
| 6 `AIR_OUTLET_BLOCKED` | no hay salida de aire como elemento físico |
| 13 `SUPPLY_UNDERVOLTAGE` | el conector de alimentación DC está sin definir |
| 16 `HUMIDITY_DEVIATION` | no hay depósito de agua todavía |
| 19 `SENSORBOARD_DOOR_FAULT` | aplazada por decisión de producto |

**Dos de ellas no son solo un problema de imagen.** La 6 y la 16 tienen textos
de acción que mandan al operador a un componente inexistente:

- 6: *«CALEFACTOR CORTADO - DESPEJAR LA SALIDA DE AIRE»* (`alarm_text.cpp`).
  La condición se detecta por duty del ventilador sostenido, así que lo que
  significa de verdad es «el aire no circula»; la instrucción concreta de
  despejar una salida presupone una rejilla que hoy no está.
- 16: *«REVISAR DEPOSITO DE AGUA»*, con la 16 activa solo si
  `in3.humidityControl` está encendido.

Mientras el hardware no las respalde, el texto pide algo que no se puede hacer.
Es una decisión de producto, no de ilustración: o el componente llega, o el
texto de acción tiene que cambiar. Anotado aquí para que no se pierda.

### 3.1 Por qué el criterio no es «alarma de paciente vs. alarma de equipo»

IEC 60601-1-8 distingue **condición de alarma fisiológica** (derivada del
paciente) de **técnica** (derivada del equipo), y es tentador mapear eso
directamente a «con bebé / sin bebé». No sirve, y el contraejemplo está en el
propio código: `ALARM_AIR_SENSOR_FAULT` es técnica pura — se ha muerto un
sensor — pero su texto de acción es *«CALEFACTOR CORTADO - SIN MEDIDA DE AIRE -
REVISAR AL BEBE»*. Clasificada por origen iría sin bebé, y sería la imagen
equivocada: lo primero que tiene que hacer el operador es mirar al niño.

El criterio que sí funciona es **la acción, no el origen**:

- **Protagonista** — el coral está sobre el bebé o sobre algo pegado a él (la
  sonda de piel). El operador tiene que mirar al bebé.
- **Contexto** — el coral está en el equipo, pero la acción incluye revisar al
  bebé, o la condición le afecta directamente (temperatura, circulación de
  aire). Se dibuja la incubadora entera, con el bebé dentro y tranquilo.
- **No** — la acción se agota en un componente y el bebé no interviene. Primer
  plano del componente, sin cúpula y sin bebé.

Dos consecuencias prácticas:

1. **Un bebé en la imagen implica que el bebé está implicado.** En
   `ALARM_HUMIDITY_DEVIATION` (revisar el depósito) dibujarlo desvía la
   atención hacia donde no hace falta, y en una pantalla de alarma eso cuesta
   segundos.
2. **«Sin bebé» obliga a primer plano.** Si la escena dibuja la incubadora
   entera de perfil, hay que poner al bebé: una incubadora vacía se lee como
   alarmante por sí sola. Por eso las cinco «No» son todas escenas de detalle.

---

## 4. Prompts de escena

### Prioridad ALTA

**1 — `ALARM_AIR_THERMAL_CUTOUT`** · *FALLO TERMICO AIRE* · «CALEFACTOR CORTADO - REVISAR AL BEBE - AVERIA: AVISO FIJO HASTA REINICIAR»

```
REFS: calefactor/*
SCENE: Front view of the incubator with the baby inside, asleep and not smiling.
TWO elements carry the message, and nothing else in the picture is coral.
(1) An AIR THERMOMETER: a simple upright thermometer — bulb at the bottom, thin
tube above — drawn INSIDE the hood, in the air space above the baby and clear of
the body, showing it measures the air. Its column is filled coral red right to
the top, plainly too hot, and three short coral heat marks radiate from the bulb.
(2) A PROHIBITION BADGE centred on the cream panel below the mattress: a coral
circle crossed by one diagonal slash, containing the flat zig-zag heater
resistance symbol — the heater has been cut.
Draw this badge LARGE, about a third of the panel's width, on bare cream with
nothing behind it. The resistance symbol is
drawn with few, wide, clearly separated peaks so it reads as a heater element at
a glance instead of a tight squiggle.
NO hand in this image.
FOCUS: the over-hot air thermometer inside the hood, with the crossed-out heater
badge below explaining why the heater is off.
```

**2 — `ALARM_SKIN_THERMAL_CUTOUT`** · *FALLO TERMICO PIEL* · misma acción

```
REFS: sonda_piel/*, calefactor/*
SCENE: Same framing and same two elements as image 1. The difference is WHAT the
thermometer is reading: here it reads the skin, in image 1 it reads the air.
(1) The same simple upright thermometer, drawn as a free-floating GAUGE in the
air inside the hood, above and clear of the baby — it is never placed on the body
and never touches it. Its column is filled coral red right to the top. A thin
plain coral leader line runs from the base of the gauge down to a small round
sensor disc lying ON THE BLANKET over the swaddled body, its thin cable curling
away to the side — the disc rests on the cloth, not on skin. The disc and its
cable are pale blue-grey. NOTHING on the baby itself is coloured or marked: no
glow, no flush, no marks on the skin. The baby is drawn plain and untouched,
exactly as in the reference.
(2) The same PROHIBITION BADGE as image 1, large and centred on the bare cream
panel below the tray with nothing behind it: a
coral circle crossed by one diagonal slash containing the flat zig-zag heater
resistance symbol, drawn a little smaller and more discreetly than in image 1.
The baby is asleep and plainly not smiling, head to the RIGHT.
NO hand in this image.
FOCUS: the over-hot gauge and the coral leader line tying it to the skin probe.
The reading comes from
the BABY here, which is what separates this image from image 1.
```

**3 — `ALARM_AIR_SENSOR_FAULT`** · *FALLO SENSOR AIRE* · «CALEFACTOR CORTADO - SIN MEDIDA DE AIRE - REVISAR AL BEBE»

```
REFS: sensor_aire/*
SCENE: Front view of the incubator with the baby inside. Mounted on the inner
back wall of the hood, above and behind the baby, there is a small rounded
rectangular air temperature sensor module with a short cable. The module is
outlined in coral red and a coral red diagonal slash crosses it out. Where the
sensor's reading would be, a small empty rounded rectangle is drawn in pale
blue with a single flat dash inside it, suggesting "no reading" — the dash is a
plain horizontal line, not a character. NO hand in this image.
FOCUS: the crossed-out air sensor module on the hood wall.
```

**4 — `ALARM_SKIN_SENSOR_FAULT_SKIN_MODE`** · *FALLO SONDA PIEL* · «CALEFACTOR CORTADO - REVISAR SONDA O PASAR A MODO AIRE»

```
REFS: sonda_piel/*
SCENE: Front view of the incubator with the baby inside, asleep and not smiling,
drawn a little larger than usual so the torso is clearly readable. The skin probe
is DISCONNECTED, and the image says so twice over:
 - the round probe disc is off the baby, lying loose on the mattress beside the
   body, with a clear empty patch of bare skin where it should be;
 - its cable runs to a small socket on the inner wall of the hood, and the
   cable's plug is OUT of that socket, hanging a short distance from it with a
   clean visible gap between plug and socket.
Only the plug, the empty socket and the gap between them are coral red; the
probe disc, its cable and the socket housing stay pale blue-grey.
NO hand in this image.
FOCUS: the gap between the unplugged probe connector and its socket — the probe
is disconnected, not merely loose.
```

**5 — `ALARM_FAN_FAILURE`** · *FALLO VENTILADOR* · «CALEFACTOR CORTADO - SIN CIRCULACION DE AIRE - REVISAR AL BEBE Y EL EQUIPO»

```
REFS: ventilador/*
SCENE: Front view of the incubator with the baby inside. In the air circulation
housing at the back of the unit, below the mattress tray, a circular fan with
four simple rounded blades is visible through a cutaway opening. The fan is
outlined in coral red and is clearly stopped: no motion arcs around it, and a
coral red diagonal slash crosses the fan circle. Two pale blue airflow curves
that would normally loop through the hood are drawn faint and broken, fading
out mid-path. NO hand in this image.
FOCUS: the stopped, crossed-out fan in the rear housing.
```

**6 — `ALARM_AIR_OUTLET_BLOCKED`** · *SALIDA DE AIRE OBSTRUIDA* · «CALEFACTOR CORTADO - DESPEJAR LA SALIDA DE AIRE»

```
SCENE: Front view of the incubator, baby inside. A folded cloth or small blanket
has been left draped over the air outlet grille at the side of the unit, sagging
over the slots and covering them. The obstructed grille is outlined in coral
red, and the airflow curves leaving it are drawn short and stubbed, blocked by
the cloth. NO hand in this image.
FOCUS: the hand lifting the cloth off the coral-outlined grille.
```

**7 — `ALARM_MAINS_INTERRUPTION`** · *CORTE DE RED* · «REVISAR LA CONEXION A LA RED»

```
REFS: entrada_red/*
SCENE: Front view of the incubator, baby inside, positioned slightly left inside
the ring. A power cable runs from the base of the unit down and to the right,
towards a wall socket. The plug has come out of the socket and hangs in mid-air
a short distance from it, cable slack and curving. The plug, the short gap
between plug and socket, and the socket are all outlined in coral red. NO hand in this image.
FOCUS: the gap between the plug and the wall socket.
```

### Prioridad MEDIA

**8 — `ALARM_AIR_TEMP_DEVIATION_HIGH`** · *TEMP AIRE ALTA* · «CALEFACTOR CORTADO - AIRE MAS DE 3 C SOBRE LA CONSIGNA»

```
SCENE: The COMPLETE unit in flat front elevation, baby inside with its head to
the RIGHT, asleep and not smiling.
The air inside the hood is tinted with a soft warm amber wash, denser near the
top, and three warning amber wavy heat lines rise in the air above the baby.
An upright THERMOMETER gauge is drawn INSIDE the hood, in the air space to the
LEFT of the baby and clear of the body — never outside the unit and never
crossing the teal ring. Its bulb is at the bottom and its column is filled high
in warning amber, well above a short pale blue tick on its side marking the
setpoint. No numbers and no scale marks beyond that single tick.
NO hand in this image.
FOCUS: the amber column rising above the pale blue setpoint tick, inside the
over-warm hood.
```

**9 — `ALARM_AIR_TEMP_DEVIATION_LOW`** · *TEMP AIRE BAJA* · «AIRE MAS DE 3 C BAJO LA CONSIGNA»

```
SCENE: Mirror of image 8 in structure, opposite in meaning. The air inside the
hood is tinted with a cool pale blue wash, denser near the mattress. Three small
cool "cold" marks — simple four-pointed sparkles in pale blue — float inside the
hood. The thermometer on the left has its column filled LOW in warning amber,
clearly below the pale blue setpoint tick on its side. The baby is swaddled a
little more snugly, the blanket drawn up higher over the chest.
FOCUS: the short amber column sitting below the pale blue setpoint tick.
```

**10 — `ALARM_SKIN_TEMP_DEVIATION_HIGH`** · *TEMP PIEL ALTA* · «CALEFACTOR CORTADO - PIEL MAS DE 1 C SOBRE LA CONSIGNA»

```
REFS: sonda_piel/*
SCENE: The COMPLETE unit in flat front elevation, exactly as in the base
reference — handle, latch block, both portholes with their pink latches, blue
mattress, corner blocks, cream drawer and green base strip all present. This is
NOT a close-up: do not crop into the baby, and do not drop any part of the unit.
The baby lies on the blue mattress with its head to the RIGHT, asleep and not
smiling. The round skin probe disc is correctly attached to its abdomen. A soft
warning amber radial glow spreads from the disc across the torso, three small
amber wavy heat lines rise from it, and the baby's cheeks are flushed a deeper
amber than the usual blush dots. The drawer stays plain — no badge in this one.
NO hand in this image.
FOCUS: the amber glow spreading from the attached skin probe on the abdomen.
```

**11 — `ALARM_SKIN_TEMP_DEVIATION_LOW`** · *TEMP PIEL BAJA* · «PIEL MAS DE 1 C BAJO LA CONSIGNA»

```
REFS: sonda_piel/*
SCENE: The COMPLETE unit exactly as in image 10 — same framing, same full
silhouette, baby's head to the RIGHT — but the opposite condition. The skin probe
is correctly attached to the abdomen and outlined in warning amber so the eye
lands on it, and from it a soft PALE BLUE cool wash spreads across the torso,
with three small pale blue four-pointed sparkles floating just above the baby.
The blanket is drawn up higher, snug over the chest and shoulders. The drawer
stays plain — no badge in this one.
NO hand in this image.
FOCUS: the cool blue wash around the amber-outlined probe. Images 10 and 11 are
a matched pair: same unit, same pose, warm glow versus cool wash.
```

**12 — `ALARM_HEATER_FAULT`** · *FALLO CALENTADOR* · «EL EQUIPO NO CALIENTA - REVISAR AL BEBE Y EL EQUIPO»

```
REFS: calefactor/*, bahia_servicio/*
SCENE: EXACTLY the same composition as image 5 (the fan failure): front view of
the incubator with the baby inside, asleep and not smiling, and one badge sitting
on the bare cream panel below the mattress. Same framing, same scale, same badge
position — the two images should read as a matched pair.
The only difference is what has failed and how loudly. The badge here contains
the flat ZIG-ZAG HEATER RESISTANCE symbol instead of the fan, and it is drawn in
WARNING AMBER (#E8A33D) rather than coral: a soft amber triangle with rounded
corners, containing a short vertical amber bar with a dot beneath it, sitting
right next to the crossed-out amber resistance. No heat lines rise from the
heater — it is cold.
Amber is the only saturated colour in this image; there is no coral anywhere.
NO hand in this image.
FOCUS: the amber warning badge over the cold heater resistance, in the same spot
where image 5 puts the crossed-out fan.
```

**13 — `ALARM_SUPPLY_UNDERVOLTAGE`** · *TENSION BAJA* · «REVISAR FUENTE Y CABLEADO»

```
SCENE: Close-up detail only — NO incubator hood, NO baby. Centred on cream, the
unit's DC power inlet: a panel-mounted socket on a short section of the unit's
pale blue-grey casing, and a barrel connector on its cable, only half inserted,
with a clear visible gap between connector and socket, and the cable kinked
sharply just behind the plug. Only the connector, the gap and the kink are amber
red; the casing and the cable stay pale blue-grey. NO hand in this image.
FOCUS: the half-inserted connector and the kinked cable.
```

**14 — `ALARM_HMI_LINK_LOST`** · *SIN ENLACE PANTALLA* · «DATOS NO FIABLES - REVISAR AL BEBE»

```
REFS: display_hmi/*
SCENE: The COMPLETE unit in flat front elevation with the baby inside, head to
the RIGHT, asleep and not smiling.
The touchscreen is NOT on an arm and NOT above the hood: it is a dark rounded
rectangular screen SET INTO the cream front panel below the mattress tray, on
the left half of that panel, exactly where the real unit carries it. Draw it flush
with the panel, like an inset window.
The screen face is blank and dark blue-grey — nothing on it, no text, no icons.
Over it, a warning amber BROKEN-LINK symbol: two short concentric signal arcs on
the left and two mirrored arcs on the right, with a clean empty gap between the
two groups where the middle arcs are missing. Amber is used only there.
The rest of the unit is unchanged and unmarked; there is no external cable.
NO hand in this image.
FOCUS: the blank inset screen on the cream front panel, with the amber
broken-link arcs across it.
```

**17 — `ALARM_HEATER_SENSOR_FAULT`** · *FALLO SENSOR CALENTADOR* · «CALEFACTOR CORTADO - SIN MEDIDA DE CONSUMO - REVISAR SENSOR DE CORRIENTE»

```
REFS: bahia_servicio/*, sensor_corriente/*, calefactor/*
SCENE: Close-up detail only — NO incubator hood, NO mattress, NO baby. Centred
on cream, the unit's cream-beige service drawer pulled open: a shallow
horizontal tray seen in flat front elevation. Inside it, on the left, a flat heater
element drawn as a low zig-zag ribbon, plainly INTACT and unmarked — it must not
read as a broken heater. On the right, a small rectangular current sensor board
with a ring clamp around the heater's supply wire. That board's thin signal
connector is unplugged: it hangs just above its empty socket with a clear
visible gap between them. ONLY the connector plug, the empty socket and that gap
are warning amber — exactly one amber area in the whole image; the board, the clamp,
the wire, the housing and the heater all stay pale blue-grey. NO hand in this image.
FOCUS: the gap between the unplugged connector and its socket. NOT the heater
element, which stays neutral and unmarked.
```

**18 — `ALARM_SENSORBOARD_LINK_LOST`** · *SIN ENLACE SENSORBOARD* · «SIN TEMPERATURA DE AIRE - REVISAR CONEXION DEL SENSORBOARD»

```
REFS: sensorboard/*
SCENE: EXACTLY the same composition as image 3 (the air sensor fault) — same
complete unit, same framing, baby's head to the RIGHT, and the same small
rectangular sensor module mounted high on the inside of the hood roof, above the
baby. The two images are a deliberate pair, because this condition is what leaves
the incubator with no air reading.
The difference is WHAT is marked and in WHAT colour. The sensor module itself is
NOT crossed out here — it is drawn plain and intact in pale blue-grey. Instead,
the thin cable running down from it reaches a small port on the unit's cream
front panel, and its connector is UNPLUGGED: it hangs just beside the port with a
clean visible gap. Only that connector, the empty port and the gap are WARNING
AMBER, plus two short amber broken-link arcs over the gap.
Nothing in the image is coral: the fault is the LINK, not the sensor.
NO hand in this image.
FOCUS: the unplugged connector at the cream panel. Image 3 crosses out the sensor
in coral; this one leaves the sensor intact and marks the broken connection in
amber.
```

### Prioridad BAJA

**15 — `ALARM_SKIN_SENSOR_FAULT_AIR_MODE`** · *SONDA PIEL NO VALIDA* · «SIN TEMP DE PIEL - MODO AIRE ACTIVO»

```
REFS: sonda_piel/*, sensor_aire/*
SCENE: Front view of the incubator, baby inside, calm and clearly fine — this is
a low-priority, informational image and must not look alarming. The skin probe
disc is detached and lying on the mattress beside the baby, cable curled, drawn
in pale blue-grey with a bright cyan (#12B5D6) outline and a small filled cyan
dot beside it. Meanwhile, high on the inside of the hood roof, the air sensor
module is drawn normally and intact, with two gentle PALE SAGE GREEN airflow
curves looping through the hood, showing that air mode is working and the baby is
fine. Do not use teal anywhere. NO hand in this image.
FOCUS: the contrast between the detached cyan-marked probe and the healthy air
sensor still working above it.
```

**16 — `ALARM_HUMIDITY_DEVIATION`** · *DESVIACION HUMEDAD* · «REVISAR DEPOSITO DE AGUA»

```
SCENE: Close-up detail only — NO incubator hood, NO baby. Centred on cream, the
water reservoir drawer pulled out of a short section of the unit's pale
blue-grey base casing, seen in flat front elevation with its front wall cut away
so the water level is visible. The water sits very low: a thin pale blue band
along the bottom, well below a short bright cyan (#12B5D6) minimum-level tick on
the drawer's inner wall. Two faint pale blue vapour curls rise weakly from it.
Only that tick and the water's surface line are cyan; the drawer, the casing and
the handle stay pale blue-grey. NO hand in this image.
FOCUS: the low water level sitting below the cyan minimum tick.
```

**19 — `ALARM_SENSORBOARD_DOOR_FAULT`** · *SENSOR PUERTA SOSPECHOSO* · «SENSOR HALL POSIBLE AVERIA - NO USAR COMO ENTRADA DE CONTROL»

```
SCENE: Close-up detail only — NO baby, no view into the hood. Centred on cream,
a short section of the incubator's front access door and its frame in flat
elevation, the door edge running vertically; the transparent door panel is drawn
as plain empty pale blue tint with nothing behind it. The door is CLOSED and
flush. On the frame beside the door edge sits a small rectangular hall sensor,
and facing it on the door, a small magnet. Around the sensor, three short bright
cyan (#12B5D6) arcs radiate, plus two more short cyan arcs pointing in opposite
directions — the sensor flickering between two states. The door, frame, panel
and magnet all stay pale blue-grey. Do NOT draw a question mark or any character.
FOCUS: the flickering cyan arcs around the hall sensor on a door that is
plainly shut.
```

### Fuera del enum

**`alm_display_link_lost.png`** — el display detecta por su cuenta la caída del
enlace con la motherBoard (`BOARD_LINK_TIMEOUT_MS` = 5000 ms) y suena por su
lado. Es la contrapartida de la 14, vista desde la pantalla.

```
REFS: display_hmi/*
SCENE: Close-up detail only — NO hood, NO baby. Centred on cream, the lower
CREAM FRONT PANEL of the unit, drawn in flat front elevation as a wide shallow
plain band, and SET INTO its left half the dark rounded
rectangular touchscreen, flush with the panel like an inset window — the same
screen and the same position as in image 14, just seen close.
The screen face is blank and dark blue-grey: nothing on it, no text, no icons.
Over it, a warning amber BROKEN-LINK symbol: two short concentric signal arcs on
the left and two mirrored arcs on the right, with a clean empty gap between the
two groups. Amber is used only there; the panel stays plain cream. There is no
external cable.
FOCUS: the break in the cable between display and incubator.
```

---

## 5. Dos cosas a resolver antes de encargar las imágenes

### 5.1 No caben en la partición

> Reescrita el 2026-09-06. La versión anterior razonaba sobre el presupuesto de
> SPIFFS y estaba equivocada de partición: **estas imágenes no van a SPIFFS, van
> dentro de la app**. Los números cambian bastante.

**Cómo llega hoy una imagen a la pantalla.** El PNG original vive en
`Display_HMI/assets_src/` (antes en `data/`), se convierte a un array C en
`Display_HMI/src/ui/assets/ui_img_*.c` y se compila dentro del binario. Nada se
lee del filesystem: los drivers de FS de LVGL están todos a 0 en
`include/config/lv_conf.h` (`LV_USE_FS_STDIO`, `LV_USE_FS_POSIX`, …) y no hay
ningún `lv_fs_drv_register` en el proyecto. La única lectura de SPIFFS en todo
el firmware es `/heartbeat.mp3` desde `src/tasks/AudioManager.cpp`.

**El presupuesto real es `app0`, y no se mide en peso de PNG.** Los arrays son
`LV_IMG_CF_TRUE_COLOR` con `LV_COLOR_DEPTH=16`, o sea **2 bytes por píxel sin
comprimir**. Lo que ocupa una imagen en flash depende solo de sus dimensiones;
que el PNG pese 40 KB o 2,2 MB da igual una vez convertido. Referencia real:
`ui_img_baby_place_sensor_png.c` son 987 KB de fuente C.

Estado tras la reorganización de particiones del 2026-09-06:

| | |
|---|---|
| `app0` | 5 MB (`0x500000`) |
| App hoy | 2,53 MB — **48 %** |
| Libre para assets nuevos | **~2,4 MB** |

Coste de 20 imágenes compiladas, según lado mayor:

| Tamaño | Por imagen | 20 imágenes | ¿Cabe en 2,4 MB? |
|---|---|---|---|
| 480×360 TRUE_COLOR | 345 KB | 6,9 MB | no |
| 360×360 TRUE_COLOR | 259 KB | 5,2 MB | no |
| 240×240 TRUE_COLOR | 115 KB | 2,3 MB | justo, sin margen |
| 360×360 `INDEXED_8BIT` | 128 KB | 2,6 MB | casi |

Reescalar y recomprimir el PNG **no ahorra nada** en flash. `--size` de
`tools/gen_alarm_images.py` sí, porque cambia los píxeles.

Salidas posibles, por orden de lo que resuelve:

1. **Cargar las imágenes desde SPIFFS y decodificarlas en runtime.** Es para lo
   que está la partición: 5,875 MB libres, y 20 PNG optimizados a 360 px ocupan
   ~1,5 MB. Requiere activar `LV_USE_PNG` y registrar un `lv_fs_drv_t` sobre
   SPIFFS, y decodificar contra los 8 MB de PSRAM (un pop-up cada vez, no hacen
   falta las 20 residentes). Cuesta trabajo de una vez y después el catálogo de
   imágenes deja de competir con el código por `app0`. `flasher_tool` ya escribe
   la imagen SPIFFS desde 2026-09, así que el canal de fábrica está resuelto.
2. **Compilar en indexado y a 240–300 px.** Sin trabajo de arquitectura, pero
   deja `app0` al borde y el problema vuelve con la primera fuente o imagen
   nueva. Recordar que el amárico sigue pendiente y también sale de aquí.
3. **Ampliar `app0` a costa de SPIFFS.** Hoy es barato — no hay unidades
   desplegadas y la tabla de particiones no viaja en el OTA — pero es pan para
   hoy: mueve el límite, no lo quita, y cierra la puerta a la opción 1.

Recomendación: opción 1, y decidir el tamaño exacto del pop-up **antes** de
generar, para pedirle a nano banana 2 la relación de aspecto final y no tener
que recortar 20 imágenes a mano.

### 5.2 El pop-up es aditivo: no sustituye a nada

Decisión tomada: al saltar una alarma se lanza un pop-up con su imagen, y **el
resto de la funcionalidad de alarmas se mantiene intacta**. El pop-up es una
capa informativa encima de lo que ya hay, no un reemplazo de la pantalla de
alarmas ni de la banda de estado.

Lo que eso implica al implementarlo, para no romper el comportamiento actual:

- **Cerrar el pop-up no toca el estado de la alarma.** No silencia, no acepta,
  no resetea. La condición sigue en `alarm_machine` exactamente igual que antes
  y la señal visual permanente sigue donde está (60601-1-8 6.8.1: la señal
  visual no se inactiva nunca).
- **El pop-up no puede tapar la banda de estado de alarma.** Si mientras está
  abierto aparece una segunda condición, el operador tiene que poder verla. En
  la práctica: dejar la banda superior fuera del área del pop-up.
- **Una alarma de mayor prioridad reemplaza el contenido del pop-up abierto**,
  en vez de encolarse detrás. `alarm_machine_top_priority()` ya da el criterio.
- **No relanzar el pop-up de una condición que el operador ya cerró** mientras
  esa condición siga presente — si no, un fallo persistente reabre el pop-up en
  bucle y el equipo queda inutilizable. Basta un flag por `AlarmId` que se borre
  cuando la condición vuelva a `ALARM_STATE_INACTIVE`.
- **El audio no depende del pop-up.** El zumbador lo gobiernan
  `alarm_machine_audio_required()` y `alarm_machine_audible_priority()`, y eso
  no cambia.

### 5.3 Coste de generar las imágenes

La suscripción **Google AI Pro no cubre la API de Gemini**: es un producto de
chat, y el acceso por API key se factura aparte, por Google AI Studio o Google
Cloud. Los modelos de imagen además **no tienen nivel gratuito**, así que hace
falta facturación activada en el proyecto o la primera llamada devuelve 429.

Precios (consultados el 2026-09-03, conviene reconfirmarlos):

| Modelo | Id | Por imagen 1K |
|---|---|---|
| Nano Banana 2 | `gemini-3.1-flash-image` | $0.067 |
| Nano Banana 2 Lite | `gemini-3.1-flash-lite-image` | $0.0336 |
| Nano Banana (1) | `gemini-2.5-flash-image` | $0.039 |

Las 20 imágenes a 1K con Nano Banana 2 salen por **~1.35 $**. Contando que cada
escena necesite dos o tres intentos hasta clavar el estilo, la tirada completa
se queda en el entorno de los 3–4 $. El coste no es el factor que decide nada
aquí.

Lo que sí conviene: iterar con `--only` sobre una sola escena hasta que el
estilo esté fijado, y solo entonces lanzar `--all`. Generar las 20 a la primera
y descubrir después que el anillo teal sale mal en todas es tirar la tirada
entera.

---

## 6. Estado de la lista

Las 19 condiciones salen de `shared/include/alarm_ids.h` (verificado). Ojo:
`docs/alarms.md` sigue hablando de "las 17 condiciones" y su tabla no incluye
las dos del SensorBoard (18 y 19), así que el reparto 7/8/2 que documenta es hoy
7/9/3. Ese documento está pendiente de actualizar.
