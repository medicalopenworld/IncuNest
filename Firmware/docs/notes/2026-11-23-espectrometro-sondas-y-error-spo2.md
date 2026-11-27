# Longitud de onda del rojo de las sondas Medle ST-S0163 y presupuesto de error de SpO2

Medidas con espectrómetro de 10 sondas del mismo lote (2026-11-19), su efecto
en la SpO2 con nuestra curva, y la lista de factores de la sonda que mueven la
SpO2 con un presupuesto de error. Sale de la sesión "Características sonda
Medle ST-S0163" (y de su duplicada del mismo día).

Esta nota no cambia nada en el código. Es la **fuente** para actualizar la
documentación de PulseNest y de la librería `incunest_afe4490`, que lleva la
sesión de desarrollo (apartado 8).

**Corregida el 2026-11-27:** el efecto de la corriente del LED estaba
exagerado y mal atribuido. Lo corrigen el apartado 4.1 nuevo, las filas 5 de
los apartados 5 y 6, y cinco filas del apartado 8.

## 1. Resumen

- Con el AFE (pulsado, ~50 mA), el rojo de las 10 sondas tiene un **centro
  medio de 663,1 nm y un pico medio de 664,9 nm**. La desviación típica entre
  sondas es de **0,83 nm**, con 2,2 nm entre la más corta y la más larga.
- La librería dice que nuestro rojo es de 668 nm. Ese número es un pico
  medido con 50 mA **continuos**; con el AFE el rojo queda **unos 3 nm más
  bajo**. La corriente es la misma: la diferencia es calor, porque el AFE
  solo enciende el LED una cuarta parte del tiempo.
- Hoy la corriente del LED no cambia nunca (49,8 mA). Bajarla a la mitad
  movería la SpO2 como mucho de 0,3 a 0,6 puntos al 97 % (apartado 4.1).
- **PPG-001, la sonda con la que se ajustó la curva, es de las más cortas**:
  la media del lote queda 0,9 nm por encima.
- Dentro de este lote, el rojo aporta **0,1 a 0,3 puntos de SpO2** (una
  desviación típica, entre 90 % y 97 %), igual que un sistema comercial. No
  hace falta clasificar las sondas por longitud de onda. **Entre lotes no
  tenemos datos.**
- El límite de la norma es un error cuadrático medio de **3,0 puntos**
  (ISO 80601-2-61:2026, 201.12.1.101.1 a), declarado **por cada combinación
  de sonda y monitor**. Eso explica que los monitores solo garanticen la
  precisión con sus modelos de sonda.

## 2. Instrumento y método

- **Espectrómetro:** Gigahertz-Optik MSC15-Bili, SN 78812, programa S-MSC15
  2026.1.2.0. Rango de 360 a 830 nm, ancho de banda óptico de 10 nm con
  corrección CIE 214, exactitud de ±1 nm en la longitud de onda de pico,
  temperatura de uso de 10 a 30 °C.
- **Integración de 12 µs** (la mínima), en modo continuo con guardado TXT
  automático. Con el AFE las capturas caen al azar respecto al pulso rojo; se
  usan las de mayor amplitud.
- **Definiciones del programa**, comprobadas recalculándolas desde el fichero
  exportado (el manual no las da):
  - Pico: máximo interpolado.
  - Centro: punto medio de la anchura a media altura (FWHM).
  - Centroide: media ponderada por potencia de **todo** el espectro.
- **El centroide del programa no vale con el AFE.** El LED infrarrojo suma
  señal en el extremo alto del espectro y el centroide sale entre 663 y
  713 nm. Calculado del espectro exportado entre 600 y 720 nm sale estable
  (659 a 662 nm). Alex lo confirmó con el infrarrojo apagado.
- Centro menos centroide es un 3,0 a 3,1 nm constante en continua: para
  comparar sondas da igual la cifra que se use. Esta nota usa **el centro**.
- La corrección CIE 214 deja lóbulos negativos entre 602 y 612 nm y entre
  704 y 720 nm. No afectan al centro ni al pico.

## 3. Resultados

### 3.1. Tres sondas a 50 mA continuos (2026-11-19, madrugada)

Centro en nm, medido por Alex con fuente de 50 mA:

| Sonda | Inicio | +5 min | +10 min | +15 min |
|---|---|---|---|---|
| PPG-001 | 665,9 | 666,8 | 667,0 | 667,2 |
| PPG-002 | 669,3 | 669,8 | 670,0 | — |
| PPG-003 | 667,8 | 668,1 | 668,9 | 668,9 |

- El "inicio" de PPG-002 no fue en frío: la sonda llevaba encendida desde
  3 min antes. Su calentamiento real desde frío es de unos +3 nm.
- Entre las tres hay 3,4 nm.

### 3.2. Diez sondas con el AFE (2026-11-19, tarde, central de MOW)

Todas del lote **LOT:260521002** (`ST-S0163-26`). La etiqueta del fabricante
no lleva número de serie; PPG-001 a PPG-010 son números nuestros, escritos a
mano. AFE a unos 50 mA en el rojo, infrarrojo encendido, cerca de 1 min por
sonda. Mediana de las capturas válidas de cada sonda, en nm:

| Sonda | Centro | Pico | Capturas |
|---|---|---|---|
| PPG-006 | 662,0 | 663,9 | 33 |
| PPG-001 | 662,2 | 664,1 | 36 |
| PPG-008 | 662,3 | 664,1 | 39 |
| PPG-004 | 662,6 | 664,3 | 56 |
| PPG-010 | 662,9 | 664,5 | 61 |
| PPG-002 | 663,3 | 665,2 | 58 |
| PPG-007 | 663,6 | 665,5 | 35 |
| PPG-003 | 664,0 | 665,7 | 39 |
| PPG-009 | 664,1 | 665,8 | 37 |
| PPG-005 | 664,2 | 666,0 | 28 |
| **Media** | **663,1** | **664,9** | |
| **Desviación típica** | **0,83** | **0,80** | |

- La FWHM es de 20,0 a 20,5 nm en todas.
- Con n = 10, la desviación típica real está entre 0,6 y 1,5 nm (intervalo
  del 95 %).
- **Con el AFE la dispersión es la mitad que en continua** (1,8 nm frente a
  3,4 nm entre las tres primeras) y el orden cambia: PPG-002 se calentaba más
  que las otras en continua.
- PPG-001 repite dentro de 0,5 nm (662,7 por la mañana, 662,2 por la tarde).
  Durante el minuto de captura el centro sube de 0,3 a 0,6 nm. Diferencias
  menores de 0,5 nm entre sondas no son fiables.

### 3.3. Longitud de onda equivalente

Es la de un LED de una sola longitud de onda que la hemoglobina absorbería
igual que el espectro real. Con el espectro de PPG-002 en continua y los
coeficientes de Prahl: 659,6 nm al 97 %, 662,3 al 85 % y 663,2 al 70 %. La
cola por debajo de 640 nm (un 5,5 % de la potencia) la mueve hasta 5 nm, así
que el valor absoluto tiene unos ±3 nm de incertidumbre. Las diferencias
entre sondas son más fiables que el valor absoluto.

## 4. Sensibilidad de la SpO2 a la longitud de onda del rojo

Con la curva `R-CURVE-STS0163-HOSPNAV-20261004` (SpO2 = 123,98 − 39,13·R),
Beer-Lambert y los coeficientes de Prahl. Un rojo más largo absorbe menos, la
R baja y **la SpO2 lee de más**.

Hay dos cotas, según de dónde venga el exceso de nuestra R sobre la teórica
(al 100 %, 0,54 a 0,61 en vez de 0,40, sin explicar):

- Cota alta, si el exceso es multiplicativo (cálculo de esta sesión).
- Cota baja, la mitad, si es un desplazamiento aditivo (cálculo del estudio
  de curvas de PulseNest).

Error en puntos de SpO2:

| SpO2 real | Por nm | 1 desv. típica del lote (0,83 nm) | Sonda media frente a PPG-001 (+0,9 nm) | Peor sonda, PPG-005 (+2,0 nm) |
|---|---|---|---|---|
| 85 % | 0,24 a 0,47 | 0,20 a 0,39 | +0,22 a +0,44 | +0,5 a +1,0 |
| 90 % | 0,20 a 0,39 | 0,16 a 0,32 | +0,18 a +0,36 | +0,4 a +0,8 |
| 95 % | 0,15 a 0,30 | 0,12 a 0,25 | +0,14 a +0,28 | +0,3 a +0,6 |
| 97 % | 0,13 a 0,26 | 0,11 a 0,22 | +0,12 a +0,24 | +0,3 a +0,5 |
| 100 % | 0,10 a 0,20 | 0,08 a 0,17 | +0,09 a +0,19 | +0,2 a +0,4 |

- **Simetría:** para cambios pequeños (temperatura, ±0,4 nm) el error es
  simétrico. Para ±3,4 nm, acortar el rojo pesa un 10 % más que alargarlo
  (al 85 %, +1,58 frente a −1,72 en la cota alta), porque la absorción de la
  desoxihemoglobina se curva hacia arriba en las longitudes de onda cortas.
- **Temperatura:** unos 0,1 a 0,15 nm/°C, es decir, 0,03 a 0,05 puntos/°C.

### 4.1. Corriente del LED

De Kock (1991) midió que el pico del LED rojo sube **unos 8 nm cuando su
intensidad se multiplica por 10** (una "década"). Es una **cota alta**: eran
LED de 1991 y el resumen no dice si los excitaba en continua o en pulsos. El
desplazamiento va con el logaritmo de la corriente, no con la corriente:
bajar de 50 a 25 mA son 0,3 décadas, 2,4 nm como mucho.

**Casi todo ese desplazamiento es calor, no corriente.** Con la misma
corriente de 50 mA, el rojo sale de 3 a 5 nm más largo en continua que con el
AFE (apartados 3.1 y 3.2), porque el AFE solo enciende el LED una cuarta parte
del tiempo. Con el AFE, por tanto, bajar la corriente moverá el rojo menos que
en continua. Para pasar de 50 a 25 mA estimo de 0,5 a 2 nm, sin medir: depende
de cuánto se calienta el chip dentro de cada pulso.

**Bajar la corriente acorta el rojo, y la SpO2 lee de menos.** Error en puntos
de SpO2 con la cota alta de 8 nm por década (Beer-Lambert, las dos cotas del
apartado 4):

| Bajar de 50 mA a | Rojo, como mucho | Error al 97 % | Error al 90 % | Error al 85 % |
|---|---|---|---|---|
| 25 mA (÷2) | −2,4 nm | −0,3 a −0,6 | −0,5 a −1,0 | −0,6 a −1,2 |
| 12,5 mA (÷4) | −4,8 nm | −0,8 a −1,6 | −1,1 a −2,2 | −1,3 a −2,6 |
| 5 mA (÷10) | −8 nm | −1,5 a −2,9 | −2,0 a −3,9 | −2,3 a −4,6 |

Con el AFE, la estimación para 25 mA es menor: de −0,1 a −0,5 puntos al 97 % y
de −0,1 a −1,0 al 85 %.

- **Hoy el efecto es cero.** Nada cambia la corriente: IncuNest deja los
  49,8 mA por defecto de la librería, y el HGAC, que IncuNest activa, solo
  cambia la ganancia del receptor (RF), no la corriente.
- **Si algún día se baja:** a la mitad cuesta poco. Dividirla por 4 o más se
  come buena parte del margen de 3,0 puntos por debajo del 90 %; habría que
  medir el desplazamiento y corregir la curva para esa corriente.
- **Cómo medirlo** (pendiente): PPG-001 en el soporte de siempre, infrarrojo
  apagado (`$SET led1=0` en PulseNest), rojo a 50, 25, 12,5 y 6 mA
  (`$SET led2=…`), un minuto en cada paso, espectrómetro a 12 µs (a 50 µs si
  a 6 mA sale ruidoso). Se compara el centro de cada paso.

## 5. Factores de la sonda que mueven la SpO2

Ordenados por relevancia. "Sin controlar" es el error si nadie controla el
factor; el presupuesto del apartado 6 es el de nuestro sistema.

| # | Factor | Sensibilidad o efecto | Origen |
|---|---|---|---|
| 1 | Longitud de onda central del rojo | Apartado 4; 2 a 5 puntos entre modelos de 660 y 670 nm | Cálculo y medidas |
| 2 | Geometría óptica y sitio de medida (modelo de sonda) | 2 a 3 puntos entre sitios | Literatura |
| 3 | Colocación: desalineación, luz que no cruza el tejido | Tira hacia el 85 % (R = 1); de 0 a más de 5 puntos | Barker 1993 |
| 4 | Forma del espectro rojo: anchura, colas, emisión secundaria | La cola por debajo de 640 nm mueve hasta 5 nm la longitud de onda equivalente | Cálculo, medidas y preprint de 2026 |
| 5 | Excitación del monitor (corriente, ciclo de trabajo) | Hasta 8 nm por cada ×10 de corriente (cota alta); a igual corriente, de 3 a 5 nm entre continua y pulsado, por calor (apartado 4.1) | de Kock 1991 y medidas |
| 6 | Envejecimiento y averías | 258 de 847 sondas en uso, inexactas o con fallo eléctrico | Milner y Mathews 2012 |
| 7 | Presión de contacto y ajuste | Hasta unos 5 puntos a la baja (frente, adultos) | Literatura |
| 8 | Luz ambiente y apantallamiento | 0,3 puntos por 1 % de error diferencial en el nivel continuo | Cálculo |
| 9 | Longitud de onda del infrarrojo | 0,04 a 0,05 puntos/nm; ±0,4 a ±0,5 con ±10 nm | Cálculo aproximado; el preprint da más |
| 10 | Temperatura del LED | 0,03 a 0,05 puntos/°C | Reynolds 1991 y cálculo |
| 11 | Fotodiodo, cable, diafonía eléctrica | Menos de 0,3 puntos | Estimación |

Para comparar factores sirve una "moneda común": un 1 % de cambio en la R
equivale a 0,27 puntos al 97 %, 0,34 al 90 % y 0,39 al 85 %.

**Geometría óptica** es todo lo que decide qué camino recorre la luz:
transmisión o reflexión, distancia entre emisor y detector, apertura del
emisor (130° en esta sonda), ventana del fotodiodo, alineación, rigidez de la
pinza y color del interior. La R lleva dentro el cociente de los caminos
medios del rojo y del infrarrojo; si cambia la geometría cambia la curva,
aunque el LED sea idéntico.

**El monitor no pregunta el sitio de medida:** va implícito en el modelo de
sonda. Cada modelo se calibra y se etiqueta para un sitio y un rango de peso,
y se identifica ante el monitor (resistencia RCAL o memoria en Nellcor). Si un
modelo se permite en dos sitios, el fabricante declara peor precisión en vez
de corregir. Nellcor, mismo monitor (1 desviación típica, 2002): ±2 en la
adhesiva de dedo, ±3 en la pinza reutilizable, ±3,5 en la de oreja y ±4 en la
reutilizable en neonatos.

**Nellcor clasifica cada LED rojo en fábrica** y codifica su grupo en la
resistencia RCAL; el monitor carga la curva de ese grupo. Según Nellcor, quien
usa un rango amplio con una sola curva pierde precisión sobre todo por debajo
del 85 %. Nosotros usamos una sola curva y no clasificamos.

## 6. Presupuesto de error

Una desviación típica, en puntos de SpO2, entre 90 % y 97 %. Son estimaciones
salvo donde pone "medido". El factor 1 ya usa la dispersión medida en el
lote.

Suposiciones:

- Bloque A: factores independientes entre sí, se suman en cuadratura. El 4 es
  solo lo que queda tras fijar el centro, porque sale del mismo LED que el 1.
- Bloque B: sesgos fijos; entran en el total como sesgo² + dispersión².
- Bloque C: fallos ocasionales, no gaussianos; quedan fuera del total.

| # | Factor | Nuestro sistema | Sistema comercial |
|---|---|---|---|
| | **A. Aleatorios, de sonda a sonda** | | |
| 1 | Longitud de onda del rojo | 0,1 a 0,3 (medido, un lote) | 0,1 a 0,3 |
| 4 | Forma del espectro rojo | 0,2 | 0,15 |
| 8 | Luz ambiente | 0,2 | 0,2 |
| 9 | Longitud de onda del infrarrojo | 0,25 | 0,15 |
| 10 | Temperatura | 0,15 | 0,15 |
| 11 | Fotodiodo, cable, diafonía | 0,2 | 0,2 |
| | **Total A (cuadratura)** | **0,45 a 0,55** | **0,4 a 0,5** |
| | Variabilidad entre personas | 1,5 a 2,0 | 1,5 a 2,0 |
| | **Total en el sitio calibrado** | **1,6 a 2,1** | **1,6 a 2,1** |
| | **B. Sesgos fuera de la condición calibrada** | | |
| 2 | Otro sitio de medida | 1 por el tejido; 2,7 observado en dedo de adulto | 1 a 2 |
| 5 | Otra corriente de LED | 0 hoy; a 25 mA, como mucho de 0,3 a 1,0 a la baja (apartado 4.1) | Menos de 0,3 |
| — | Otro lote de sondas | Sin medir; hasta 0,5 al 97 % y 1 al 85 % con ±2 nm | Controlado por clasificación |
| | **Total con el sesgo del factor 2** | **1,9 a 2,3** (con 1 punto); **3,1 a 3,4** (con 2,7) | **1,9 a 2,9** |
| | **C. Fallos ocasionales, fuera del total** | | |
| 3 | Colocación | Sin medir | Casi siempre detectada |
| 6 | Envejecimiento y averías | Sin datos (sondas nuevas) | 30 % de sondas en uso (Milner) |
| 7 | Presión de contacto | Sin medir | Sin cifra |

Lectura:

- **En el sitio calibrado, la sonda apenas mueve el total.** Domina la
  variabilidad entre personas, igual que en un sistema comercial.
- **Nuestro total es contra el monitor de referencia, no contra sangre.** La
  norma advierte de que una referencia así añade error propio; contra
  gasometría no hay dato.
- **Fuera del sitio calibrado rozamos o pasamos el 3,0 de la norma** si el
  desfase de 2,7 puntos entre pie de neonato y dedo de adulto es real. El
  rationale de la librería (§18) cree que la parte del tejido es de 1 punto o
  menos y que el resto viene de comparar dos referencias. Es lo que más
  conviene aclarar.
- La variabilidad entre personas está deducida de las especificaciones
  comerciales; nuestras sesiones dieron de 1,2 a 2,4 contra el Mindray.

## 7. Qué hacer con los lotes

- **Con cada lote nuevo**, medir de 5 a 10 sondas con el AFE (no con fuente
  continua) y comparar la media del centro con los 663,1 nm de
  LOT:260521002. Si difiere más de 1 nm, corregir la curva para ese lote.
- Anotar el lote en el registro de cada sesión de calibración, como ya hace
  `PulseNest/docs/session_configs/subj11.toml`.
- Marcar el número propio de cada sonda en el cable o el conector (etiqueta
  termorretráctil), nunca en la pinza que toca al paciente, y llevar un
  registro que una número, lote, caja y fechas.
- Protocolo de medida: AFE a 50 mA en el rojo y **0 mA en el infrarrojo**,
  soporte fijo, sin luz ambiente, referencia de oscuridad al empezar y cada
  30 a 45 min, PPG-001 como referencia al principio, a mitad y al final.

## 8. Qué falta en la documentación del proyecto

Lo lleva la sesión de desarrollo; esta nota es la fuente.

| Contenido | Estado | Dónde |
|---|---|---|
| Lista de factores con cifras de la literatura | Ya recogido, más completo | `PulseNest/docs/r_curve_calibration_survey.md` §5 |
| Sensibilidad a la longitud de onda (cota baja) | Ya recogido | Mismo documento, §6 |
| Desfase pie-dedo y sus candidatos | Ya recogido | Rationale de la librería, §18 |
| La spec dice que el HGAC "mueve el rojo con la corriente del LED" y que la curva mezcla corrientes | **Corregir**: el HGAC solo mueve RF (§5.8) y la corriente es fija, 49,8 mA | Spec de la librería, línea 1274 |
| Con el AFE, el efecto de la corriente es sobre todo calor y menor que en continua (apartado 4.1) | **Falta** | Survey §5.1, fila de la corriente del LED |
| Que las sesiones de la curva `R-CURVE-STS0163-HOSPNAV-20261004` usaron 49,8 mA | **Comprobar** en la cabecera de las capturas, que anota la corriente | PulseNest, sesiones HOSPNAV del 2026-09-23 |
| Medidas del espectrómetro (apartado 3) | **Falta** | Survey §6, spec de la librería y rationale §4.2 |
| El rationale dice que la R no cambia al mover ILED por color: vale para la amplitud, no para la longitud de onda | **Matizar** (apartado 4.1) | Rationale de la librería §2.1 |
| Si la fase 4 del HGAC llega a mover ILED, tiene que limitar el rango o corregir la curva según la corriente | **Falta** (apartado 4.1) | Rationale de la librería §2.9 |
| "Nuestro rojo es 668 nm": con el AFE es 664,9 de pico y 663,1 de centro | **Corregir** | Survey línea 278 y spec de la librería línea 1272 |
| Presupuesto de error con totales (apartado 6) | **Falta** | Rationale de la librería, sección nueva |
| Milner 2012 y precisión de Nellcor por modelo de sonda | **Falta** | Survey §5.1 |
| El survey dice "≈ 0 por encima del 85 %" para la longitud de onda | **Choca** con su propia tabla del §6 | Survey §5.1 |
| La spec dice que I_FP 100 mA vale con el ~25 % de ciclo del AFE | **Revisar**: la hoja de datos lo da a 1/8 de ciclo y 1 kHz | Spec de la librería, cerca de la línea 4928 |

## 9. Datos y scripts

Fuera de Git, en `C:\PRJ\MOW\Misc\Spectral_Analyzer_MSC15\`:

- `Meas\2026-11-19\`: capturas originales (TXT en Latin-1; el espectro sigue
  a la línea `wavelength / nm`).
- `Meas\2026-11-19_probe_assignment.csv`: fichero → sonda de la madrugada.
- `Meas\2026-11-19_probe_summary_AFE.csv`: resumen por sonda de la tarde.
- `analysis_2026-11-19\`: scripts (`pm.py`, `sens_table.py`,
  `lambda_eq.py`…), sus salidas y `RESULTADOS_10_sondas_AFE.md`.
- `2026-11-19_factores_error_SpO2_recuperado.md`: copia literal de la
  conversación de factores de error.

Hoja de datos de la sonda (solo el emisor):
`PulseNest/docs/Medle_probe/Datasheet/`. Picos de 663/665/667 nm (rojo) y
895/905/915 nm (IR) a 20 mA; anchura de 17 y 65 nm; I_F 50 mA; I_FP 100 mA a
1/8 de ciclo y 1 kHz. No trae datos del fotodiodo ni del cable.

## 10. Fuentes

- ISO 80601-2-61:2026, apartado 201.12.1.101.1 a).
- Nellcor, [visión técnica de OxiMax](https://www.ece.mcmaster.ca/faculty/debruin/debruin/EE%204BD4/OxiMaxTechNote.pdf) (RCAL, curvas por sonda).
- Nellcor, [patente US6801797B2](https://patents.google.com/patent/US6801797B2/en).
- Nellcor, [precisión por modelo de sonda, 2002](https://bioclinicalservices.com.au/nellcor/clinical/nellcor-oxygen-saturation-accuracy-spec-grid-march-2002).
- [Preprint de 2026 sobre perfil espectral y tolerancias](https://www.biorxiv.org/content/10.64898/2026.04.20.719559v1.full) (sin revisión por pares).
- de Kock et al. 1991, [intensidad del LED](https://www.tandfonline.com/doi/abs/10.3109/03091909109016208).
- Reynolds et al. 1991, [temperatura del LED](https://researchnow.flinders.edu.au/en/publications/temperature-dependence-of-led-and-its-theoretical-effect-on-pulse/).
- Milner y Mathews 2012, [precisión de oxímetros en uso](https://pubmed.ncbi.nlm.nih.gov/22324874/).
- Barker 1993, [sonda mal colocada](https://experts.llu.edu/en/publications/the-effect-of-sensor-malpositioning-on-pulse-oximeter-accuracy-du-2/).
- Coeficientes de extinción de la hemoglobina: S. Prahl (OMLC).
