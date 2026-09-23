# Fotos de referencia de la incubadora

Aquí van fotos **del equipo real** para que las imágenes de alarma se parezcan
a una IncuNest y no a una incubadora genérica de stock. `tools/gen_alarm_images.py`
las adjunta a cada petición junto a las referencias de estilo.

Dos tipos de referencia, que responden a preguntas distintas y no se sustituyen:

- **Estilo** — `Display_HMI/assets_src/Baby_phototherapy_eyes_covering.png` y
  `Baby_place_sensor.png`. Dicen *cómo dibujar*: trazo, paleta, anillo, bebé.
  Ya están cableadas en el script, no hace falta tocarlas.
- **Fidelidad** — lo de esta carpeta. Dicen *qué hay que dibujar*: la forma real
  de la cúpula, dónde está la bahía de servicio, qué conector es cuál.

## Dónde dejar cada cosa

```
docs/alarm_images_refs/
├── _comunes/          <- se adjuntan a TODAS las imágenes
│   ├── incubadora_perfil.jpg
│   └── incubadora_frontal.jpg
├── ventilador.jpg     <- solo si una escena las pide por nombre
├── bahia_servicio.jpg
├── sensor_corriente.jpg
└── ...
```

**`_comunes/`**: fotos del equipo entero. **Hoy NO se adjuntan automáticamente.**
Por decisión de producto (2026-09-04) la incubadora dibujada sigue siendo la
genérica de los assets `Baby_*`, para no dejar desparejados los cuatro que ya
están en `assets_src/`. Adjuntar fotos del equipo real a todas las imágenes tiraría en
dirección contraria al texto del bloque de estilo, y el resultado sería un
híbrido peor que cualquiera de las dos opciones. Se quedan aquí como referencia
humana, y por si esa decisión se revisa.

Si algún día se revisa: hay que cambiar a la vez el párrafo `THE INCUBATOR` del
bloque de estilo **y** volver a adjuntarlas en `collect_refs()`. Cambiar solo
una de las dos cosas es lo que produce el híbrido.

**Raíz**: una foto por componente, con el nombre que quieras. Solo se usan
cuando una escena las nombra.

## Cómo pedirlas desde una escena

En `docs/alarm_popup_image_prompts.md`, la primera línea del bloque de la escena:

```
REFS: bahia_servicio.jpg, sensor_corriente.jpg
SCENE: Close-up detail only — NO incubator hood, NO baby...
```

Admite comodines (`sensor_*.jpg`). La línea `REFS:` no llega al modelo: la
consume el script.

## Qué foto funciona

- **De perfil y con el objeto entero en cuadro.** Las escenas son alzados
  laterales; una foto en escorzo obliga al modelo a inventarse la vista.
- **Fondo despejado y luz plana.** Un banco lleno de cables detrás mete ruido
  que acaba apareciendo dibujado.
- **Una cosa por foto.** «El conector del sensor de corriente» funciona; «el
  interior del equipo» no, porque no se sabe qué mirar.
- **Con algo que dé escala** si el tamaño importa (una mano, una moneda).

JPG o PNG, cualquier resolución: el script las reescala a 1024 px antes de
enviarlas.

## Límite

Máximo **6 imágenes por petición** (`MAX_REFS`), estilo incluido. O sea: 2 de
estilo + `_comunes/` + las específicas. Si te pasas, el script avisa y recorta —
y el recorte se come primero las específicas, que suelen ser las que más falta
hacen. Mantén `_comunes/` en dos ficheros.
