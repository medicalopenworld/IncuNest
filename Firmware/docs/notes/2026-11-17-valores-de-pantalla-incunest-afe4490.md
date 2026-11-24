# Valores de pantalla de `incunest_afe4490`: propuesta de contrato

Fase 2 de la revisión del interfaz entre la librería e IncuNest. La fase 1
está en `2026-11-16-interfaz-libreria-incunest-afe4490.md`. Lo que ya está
hecho se anota en el apartado 8.

## 1. Qué son

Tres campos nuevos en la muestra que entrega la librería: `spo2_disp`,
`hr_disp` y `pi_disp`. Son el número que un monitor debe enseñar. Cada uno
lleva al lado un estado que dice por qué se ve lo que se ve.

Las cinco magnitudes clínicas de hoy (`spo2`, `pi`, `hr1`, `hr2`, `hr3`)
siguen igual, con su contrato de cuatro reglas. Sirven para pruebas,
validación, calibración y telemetría. Un monitor deja de leerlas.

La librería ya hace lo mismo con la onda: `ppg_disp` es la señal preparada
para pintar.

## 2. Lo que ya está decidido (Alex)

- Los valores de pantalla se calculan en la librería, no en IncuNest.
- No llevan SQI mientras ningún consumidor lo necesite.
- Un valor mantenido en pantalla nunca supera los 10 s que piden OMS/UNICEF.
- No hace falta una lectura de estado nueva en la librería. IncuNest leerá
  `getData()` a 50 Hz. La cola de la librería entrega entonces una muestra de
  cada diez, con unos 20 ms de retraso, que no importa. (Ver el apartado 7.)

## 3. Contrato propuesto

Cuatro reglas, las mismas para los tres valores.

1. **Un valor de pantalla es un número finito o NaN.** Finito: el monitor lo
   pinta. NaN: el monitor pinta guiones. No hay que mirar nada más para
   decidir qué pintar.
2. **Un valor de pantalla finito está siempre dentro de su rango declarado**,
   el mismo de las magnitudes clínicas.
3. **Cada valor lleva un estado**, uno de cinco. El estado no cambia qué
   número se pinta; dice cómo acompañarlo.

   | Estado | Valor | Qué pasa | Lo que haría un monitor |
   |---|---|---|---|
   | Sin sonda | NaN | La sonda no está aplicada | Ocultar el panel |
   | Adquiriendo | NaN | La sonda se acaba de aplicar y aún no hay valor | Guiones, «midiendo» |
   | Actual | finito | El valor es de ahora | El número, normal |
   | Mantenido | finito | No hay valor nuevo; se enseña el último bueno | El número, con una marca |
   | Señal inadecuada | NaN | Hay sonda y ha pasado tiempo de sobra, pero no hay valor | Guiones, «revise la sonda» |

4. **Un valor mantenido tiene fecha de caducidad.** Pasado el tiempo máximo
   (configurable, nunca más de 10 s), el valor pasa a NaN y el estado a
   «señal inadecuada».

Los tres valores son independientes: puede haber HR y guiones en la SpO2.

Lo que no entra: SQI, motivos detallados, redondeo. El valor sigue siendo un
número con decimales; pasarlo a entero o a dos decimales es cosa de quien lo
pinta.

## 4. Cómo se calcula cada uno, en dos pasos

### Paso 1: lo mismo que se ve hoy

El objetivo es mover el cálculo a la librería sin cambiar nada en pantalla.

- **`hr_disp`**: la regla que hoy vive en `CommTask`. HR2 y HR3 de acuerdo
  (menos de 8 lpm de diferencia), SQI máximo ≥ 0,8 y mínimo ≥ 0,5, media
  ponderada por SQI, y un tiempo mínimo de acuerdo antes de empezar a
  enseñar. HR1 no entra, como hoy.
- **`spo2_disp`**: la `spo2` tal cual.
- **`pi_disp`**: el `pi` tal cual.
- **Sin mantenimiento**: un segundo malo da guiones, como hoy. El estado
  «mantenido» no aparece todavía.

Una diferencia inevitable: `CommTask` cuenta «dos vueltas buenas seguidas» de
un bucle de 1 s. En la librería se define en tiempo (por ejemplo, un segundo
de acuerdo continuo). Se comprueba sobre las capturas que `hr_disp` da lo
mismo que la regla de hoy, salvo ese segundo de margen.

Por qué la HR se toma del acuerdo de dos métodos y no de uno solo: la
librería tiene medido que HR3 publica a veces un tercio del pulso, entre un
4,5 % y un 19 % de sus valores según el sujeto (su justificación, §22.1).
Exigir que HR2 coincida lo descarta.

### Paso 2: mejoras, cada una con su medida

- **Mantener el último valor bueno.** No es inocuo. La librería ya probó a
  mantener 5 o 10 s la última SpO2 y empeoró en el neonato que se movía: el
  último valor ya estaba contaminado por el movimiento (su justificación,
  §17). Hay que medir qué valor se mantiene (quizá no el último, sino uno de
  unos segundos antes) y cuánto tiempo.
- **Cuánto dura «adquiriendo».** Se saca de las capturas: el tiempo en que
  normalmente aparece el primer valor. Hoy la HR tarda unos 6 s de mediana y
  la SpO2 unos 10 s, con colas mucho más largas.
- **Mejorar la regla de `hr_disp`** con las capturas con ECG: umbrales,
  entrada de HR1, y qué mira la puerta de pulso de la SpO2.
- **`pi_disp` y `spo2_disp`** tras el PI por peine espectral, si se hace.

## 5. Qué hacen la norma y los fabricantes con un valor mantenido

Búsqueda del 2026-11-17. Lo marcado «sin confirmar» no se ha comprobado en el
manual del fabricante.

- **ISO 80601-2-61**: indicación obligatoria si el valor lleva más de 30 s
  sin actualizarse, y un indicador de señal inadecuada. No publicar el valor
  ya cumple. No fija la forma de la marca (color, parpadeo, atenuación).
- **OMS/UNICEF 2019**: actualización de dato válido en 10 s o menos.
- **Nellcor**: mantiene el último valor; aviso a los 25 s; lo retira a los
  40 s (SpO2) o 50 s (pulso). Sin confirmar si el número cambia de aspecto.
- **Masimo**: sigue enseñando el valor con baja confianza, atenuado y con
  mensaje. Sin confirmar.
- **Philips**: no mantiene; sustituye el número por `-?-` con un mensaje.
- **Nihon Kohden**: mantiene 30 s y luego guiones. **GE**: guiones.

Lección recogida en PulseNest: no mantener un valor sin límite de tiempo y
sin marca visible. Los valores retenidos ocultaron bradicardias en estudios
de UCIN.

Con el estado de la regla 3, IncuNest tiene lo necesario para marcar el
valor. La forma de la marca es una decisión de producto, del HMI.

## 6. Decisiones pendientes de Alex

- ¿Valen los cinco estados de la regla 3?
- ¿Se hace primero el paso 1 (lo mismo que hoy, sin mantener nada) y el
  mantenimiento después, con medidas?
- ¿El nombre `hr_disp`, o `pr_disp` (lo que mide un pulsioxímetro es la
  frecuencia de pulso)? Propuesta: `hr_disp`, por coherencia con `hr1`,
  `hr2`, `hr3` y con lo que pone en pantalla.

## 7. El lado de IncuNest, después

En un segundo PR, cuando exista la versión de la librería:

- `CTRL,VIT` envía los tres valores de pantalla. La fusión de `CommTask`
  desaparece.
- Telemetría: se propone enviar los valores de pantalla (lo que vio la
  enfermera) además de las magnitudes clínicas de hoy. Hacen falta claves
  nuevas.
- El HMI decide cómo marcar un valor mantenido. Hace falta que el estado
  viaje en el protocolo.
- **Lectura a 50 Hz.** Hoy `SPO2_Task` vacía la cola de la librería cada
  1 ms. Al pasar a 50 Hz hay tres sitios que cuidar:
  - La captura PPG diezma hoy de 500 a 50 Hz quedándose con una muestra de
    cada diez. Tendría que quedarse con todas.
  - `CTRL,PPG` sale de `CommTask` con su propio reloj de 20 ms. Dos relojes
    de 50 Hz independientes se desfasan: de vez en cuando se enviaría la
    misma muestra dos veces o se saltaría una. Lo limpio es que la onda se
    envíe cuando llega una muestra nueva.
  - El vigilante del chip no cambia: su plazo es de 1 s.

## 8. Estado

**2026-11-17.** Alex aprueba los cinco estados y hacer primero el paso 1.

El paso 1 está hecho en la librería y publicado como v0.124 (commit
`1c9c63e`, etiqueta y release):

- Seis campos nuevos al final de la muestra: los tres valores y sus tres
  estados. El estado «mantenido» existe pero no se produce todavía.
- La regla de la HR es la de CommTask, definida en tiempo: se enseña cuando el
  acuerdo ha durado más de 1 s, y tras una interrupción de más de 2 s se
  vuelve a exigir esa espera.
- «Adquiriendo» dura como mucho 15 s desde que se aplica la sonda. Es un valor
  provisional, por medir en el paso 2.
- Un grupo de tests nuevo (11 casos). Uno compara vuelta a vuelta con la regla
  de CommTask durante 600 vueltas: coincide siempre.
- El ejemplo de la librería se ha reescrito sobre los valores de pantalla. No
  compilaba desde la v0.90 y decidía con el SQI.

Comparación con la regla de CommTask sobre las 19 capturas, en las 20 fases
que puede tener su bucle de 1 s (252 126 vueltas):

| | Vueltas |
|---|---|
| Mismo resultado | 99,85 % |
| La librería enseña y CommTask no | 0,04 % |
| CommTask enseña y la librería no | 0,12 % |
| Valor distinto cuando los dos enseñan | 0 |

Las diferencias son acuerdos que empiezan o acaban entre dos vueltas del
bucle, que un bucle de 1 s no ve.

Dato de partida para el paso 2: con la sonda aplicada se enseña una HR el
55 % del tiempo, con cualquiera de las dos reglas. En el neonato que se movía,
entre el 0 y el 11 % según la sesión. No se ha comparado con el ECG.

Al subir IncuNest a esta versión hay que añadir los tres valores de pantalla
a su muestra «sin medida» (la que entrega cuando el chip se para).

**2026-11-17, lado de IncuNest.** Hecho en la rama
`feat/pulse-oximetry-display-values` (sale de la rama del PR #34), como
motherBoard 18.59.0, en dos commits locales (`a11b905d`, valores de
pantalla; `5afbb56f`, lectura a 50 Hz), **sin subir**. El HMI no se toca.

- Librería en v0.124.
- `CTRL,VIT` envía `hr_disp`, `spo2_disp` y `pi_disp`. La regla de la HR ya
  no está en `CommTask`. El formato de la línea no cambia.
- La muestra «sin medida» lleva los tres valores de pantalla a NaN y sus
  estados en «sin sonda».
- `SPO2_Task` lee la librería 50 veces por segundo, con periodo fijo. Antes
  la leía cada 1 ms.
- `CTRL,PPG` sale una vez por muestra leída (número de secuencia), sin reloj
  propio en `CommTask`.
- La captura PPG guarda todas las muestras que recibe; ya no diezma.

Compila para V18 y pasan los 828 tests nativos. Cargado en la motherBoard
V18 del banco y probado por Alex con el HMI 4.7.0: todo se ve bien.

Fuera de este cambio, para después: el estado de cada valor en el protocolo
y en el HMI, y las claves de telemetría con los valores de pantalla.
