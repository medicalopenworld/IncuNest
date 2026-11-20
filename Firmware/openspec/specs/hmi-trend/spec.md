# hmi-trend Specification

## Purpose
TBD - created by archiving change hmi-trend. Update Purpose after archive.
## Requirements
### Requirement: La tendencia tiene un punto de entrada en el centro de alarmas

`AlarmCenter` SHALL mostrar, en su cabecera y a la derecha del botón TEST, un
botón TENDENCIA que abre `TelemetryHistory`. La tendencia SHALL abrirse sobre
el centro de alarmas, que queda abierto debajo. El resto de la cabecera
(TEST, título, X) SHALL conservar su función y su zona táctil. La política de
cesión de la tendencia (cierre con cualquier alarma activa o enlace caído) NO
SHALL cambiar.

#### Scenario: El botón abre la tendencia sin alarmas
- **WHEN** no hay ninguna alarma activa y el operador abre el centro de
  alarmas y pulsa TENDENCIA
- **THEN** se abre la vista de tendencia por encima del centro de alarmas
- **AND** al cerrarla con X el centro de alarmas sigue abierto
- *(Verificación manual en banco, tarea 1.8.)*

#### Scenario: Una alarma activa cierra la tendencia
- **WHEN** la tendencia está abierta
- **AND** la motherBoard anuncia una alarma de cualquier prioridad
- **THEN** la tendencia se cierra en la siguiente pasada del bucle de UI
- **AND** el centro de alarmas queda visible debajo
- *(Verificación manual en banco, tarea 1.8.)*

### Requirement: Las alarmas se marcan como líneas verticales en los tres gráficos

Cada flanco de alarma SHALL registrarse con su id, su prioridad, su sentido
(alta/baja) y el instante del flanco. Un flanco es el cambio de estado de una
condición recibido por `CTRL,ALM` (de inactiva a activa, o de activa a
inactiva). Cada flanco SHALL dibujarse como una
línea vertical en los tres gráficos (aire, piel, humedad) cuando cae dentro
de la ventana visible. Un flanco de alta SHALL dibujarse continuo con el color
de su prioridad (alta rojo, media naranja, baja amarillo oscuro); un flanco
de baja SHALL dibujarse discontinuo en verde. Una línea repetida de
`CTRL,ALM` sin cambio de estado NO SHALL generar un evento nuevo. La vista
SHALL mostrar una leyenda de las marcas y de la consigna. El anillo de
eventos SHALL contener como mucho 64 eventos y descartar el más antiguo al
llenarse.

#### Scenario: Un flanco de alta y uno de baja quedan registrados en orden
- **WHEN** se registran un flanco de alta y, después, uno de baja de la misma
  alarma
- **THEN** la ventana que los contiene devuelve ambos, en orden cronológico,
  con su sentido y su prioridad
- *(Test de host: `test/test_trend_events`.)*

#### Scenario: El anillo de eventos descarta el más antiguo
- **WHEN** se registran más de 64 eventos
- **THEN** quedan los 64 más recientes en orden cronológico
- *(Test de host: `test/test_trend_events`.)*

#### Scenario: Las marcas aparecen en los tres gráficos
- **WHEN** salta una alarma con la tendencia cerrada y luego se abre
- **THEN** los tres gráficos muestran una línea vertical en el instante de la
  alta, y otra discontinua en el de la baja si la alarma se resolvió
- *(Verificación manual en banco, tarea 1.8.)*

### Requirement: La consigna es una línea histórica de cada gráfico

Cada gráfico SHALL mostrar, además de la medida, una línea de consigna
histórica (serie propia, no solo el valor actual). Una consigna SHALL
registrarse únicamente si esa magnitud está bajo control según el eco de la
motherBoard: la de aire con control de temperatura en modo aire, la de piel
con control de temperatura en modo piel, la de humedad con control de
humedad. En cualquier otro caso, y en una ranura sin telemetría, la consigna
SHALL quedar como "sin dato" y no dibujarse.

#### Scenario: La validez de la consigna sigue a actuación y modo
- **WHEN** la actuación es solo temperatura en modo aire
- **THEN** la consigna de aire es válida y las de piel y humedad no
- **AND** con actuación de temperatura y humedad en modo piel son válidas
  las de piel y humedad y no la de aire
- **AND** con el control apagado no es válida ninguna
- *(Test de host: `test/test_trend_policy`.)*

#### Scenario: La línea de consigna sigue los cambios
- **WHEN** el operador cambia la consigna de aire con el control activo y se
  abre la tendencia
- **THEN** la línea de consigna muestra el escalón en el instante del cambio
- *(Verificación manual en banco, tarea 1.8.)*

### Requirement: El modelo guarda 24 h en ranuras de 10 s sobre PSRAM

El modelo de datos SHALL guardar 8640 ranuras de 10 s (24 h) por serie, seis
series (aire, piel, humedad y sus tres consignas) como enteros de 16 bits en
décimas, en un único bloque de PSRAM. NO SHALL reservarse SRAM interna para
el modelo. El valor `INT16_MIN` SHALL significar "sin dato". Las ranuras sin
telemetría entre dos muestras SHALL rellenarse con "sin dato" en las seis
series, de modo que el índice del anillo siga siendo el tiempo. Una muestra
con ranura menor o igual que la última NO SHALL sobrescribirla.

#### Scenario: Los huecos se rellenan con sin dato
- **WHEN** se registran muestras en las ranuras 100 y 105
- **THEN** las ranuras 101 a 104 existen y valen "sin dato" en las seis
  series
- *(Test de host: `test/test_trend_store`.)*

#### Scenario: El anillo da la vuelta conservando lo reciente
- **WHEN** se registran más de 8640 ranuras consecutivas
- **THEN** quedan las 8640 más recientes y las anteriores ya no se pueden leer
- *(Test de host: `test/test_trend_store`.)*

#### Scenario: Una muestra repetida o anterior se ignora
- **WHEN** se intenta registrar una muestra en una ranura menor o igual que la
  última escrita
- **THEN** no se modifica ninguna ranura
- *(Test de host: `test/test_trend_store`.)*

### Requirement: Las ventanas son 4 h, 12 h y 24 h con decimación min/max

La vista SHALL ofrecer las ventanas 4 h (por defecto), 12 h y 24 h. Cada
gráfico SHALL tener siempre 1442 puntos (721 cubos de mínimo y máximo). Los
límites de cubo SHALL alinearse a múltiplos absolutos de ranura (no a
"ahora") para que el dibujo no parpadee al avanzar el reloj. Un cubo con algún
dato SHALL emitir su mínimo y su máximo en el orden en que ocurrieron; un cubo
sin ningún dato SHALL emitir "sin dato" en ambos puntos (corta la línea). El
eje X SHALL mostrar cinco etiquetas de hora y una rejilla por ventana (4 h
cada 15 min, 12 h cada hora, 24 h cada 2 h).

#### Scenario: Un pico aislado sobrevive a la decimación de 24 h
- **WHEN** hay un único valor alto entre 12 ranuras bajas del mismo cubo
- **THEN** el máximo del cubo es ese valor
- *(Test de host: `test/test_trend_decimate`.)*

#### Scenario: Los cubos son estables al avanzar una ranura
- **WHEN** el reloj avanza una ranura dentro del mismo cubo
- **THEN** el inicio alineado de la ventana y el contenido de los cubos
  completos no cambian
- *(Test de host: `test/test_trend_decimate`.)*

#### Scenario: Cambiar de ventana no corta la tendencia
- **WHEN** el operador selecciona 12 h y luego 24 h
- **THEN** los tres gráficos se repintan con la ventana elegida y las
  etiquetas de hora del eje cambian en consecuencia
- *(Verificación manual en banco, tarea 2.7.)*

### Requirement: La base de tiempo pasa a epoch sin perder datos

Con hora sincronizada, la ranura de una muestra SHALL ser el epoch UTC en
segundos dividido entre 10. Antes de tener hora, SHALL usarse la base
monótona de `esp_timer`. En la primera sincronización, y en toda corrección
posterior de más de una ranura, los datos y eventos ya guardados SHALL
desplazarse el mismo número de ranuras que el reloj, conservando su separación
relativa. Una diferencia de una ranura o menos SHALL ignorarse.

#### Scenario: La primera hora re-etiqueta lo ya registrado
- **WHEN** se han registrado muestras con el reloj sin sincronizar y llega la
  primera hora válida
- **THEN** las muestras anteriores conservan su separación y la última queda
  en la ranura de epoch correspondiente a su instante
- *(Test de host: `test/test_trend_clock`.)*

#### Scenario: El redondeo no mueve el eje
- **WHEN** la hora vuelve a leerse con una diferencia de una ranura o menos
  respecto al desplazamiento vigente
- **THEN** no se aplica ningún desplazamiento
- *(Test de host: `test/test_trend_clock`.)*

### Requirement: La tendencia se vuelca a LittleFS y sobrevive a un corte de red

Con hora sincronizada, el HMI SHALL volcar el modelo (series y eventos) al
fichero `/trend.bin` de LittleFS cada 5 min si han entrado muestras nuevas, y
al entrar en bloqueo con una separación mínima de 2 min respecto al volcado
anterior. El volcado SHALL ejecutarse en la tarea de UI fuera de `LVGL_Lock`,
en trozos de como máximo 4 KB y como mucho uno cada 50 ms, sobre un fichero
temporal que se renombra al terminar, de modo que un corte en mitad conserve
el fichero anterior intacto. Un fallo de escritura NO SHALL afectar a la
tendencia en RAM ni reintentarse en bucle. El fichero SHALL tener cabecera con
magic, versión, tamaño de cabecera, segundos por ranura, número de ranuras,
epoch de la última ranura, número de series y de eventos, y dos CRC-32
(cabecera y carga). Un fichero con magic, versión, longitud o CRC erróneos se
SHALL ignorar entero.

#### Scenario: La serialización ida y vuelta conserva series y eventos
- **WHEN** se serializa un modelo con huecos y eventos y se restaura sobre un
  modelo vacío
- **THEN** cada ranura y cada evento restaurados coinciden con los originales
- *(Test de host: `test/test_trend_file`.)*

#### Scenario: Un fichero corrupto se rechaza entero
- **WHEN** se altera un byte de la cabecera, de la carga, la versión o la
  longitud del fichero
- **THEN** la validación falla y el modelo no se modifica
- *(Test de host: `test/test_trend_file`.)*

#### Scenario: Cuándo toca volcar
- **WHEN** el reloj no está sincronizado, o no han entrado muestras desde el
  último volcado
- **THEN** no toca volcar
- **AND** con reloj sincronizado y muestras nuevas toca volcar a los 5 min, o
  al entrar en bloqueo si pasaron al menos 2 min
- *(Test de host: `test/test_trend_policy`.)*

#### Scenario: Un corte de corriente conserva la tendencia
- **WHEN** el equipo lleva más de una hora registrando, se le corta la
  corriente y se reconecta
- **THEN** al recuperar la hora la tendencia muestra lo registrado hasta el
  último volcado, el intervalo apagado queda como "sin dato" y los marcadores
  de alarma de ese tramo siguen presentes
- *(Verificación manual en banco, tarea 2.7.)*

### Requirement: La restauración alinea por epoch y no pisa lo vivo

El HMI SHALL intentar restaurar `/trend.bin` al sincronizarse el reloj por
primera vez tras el arranque, y solo entonces. Las muestras del fichero SHALL
alinearse por el epoch de su última ranura; el intervalo entre esa ranura y la
primera muestra viva SHALL quedar como "sin dato"; si hay solape, las
muestras vivas SHALL prevalecer; lo que exceda de 24 h SHALL descartarse por
el lado antiguo. Un fichero cuya última ranura sea posterior a la ranura
actual (más de una) SHALL rechazarse. Sin hora sincronizada NO SHALL
restaurarse ni volcarse nada.

#### Scenario: El hueco del apagado queda sin dato
- **WHEN** el fichero termina 30 min antes de la primera muestra viva
- **THEN** las 180 ranuras intermedias valen "sin dato" y las del fichero
  conservan su posición temporal
- *(Test de host: `test/test_trend_file`.)*

#### Scenario: Las muestras vivas prevalecen en el solape
- **WHEN** el fichero y la sesión actual cubren las mismas ranuras con valores
  distintos
- **THEN** esas ranuras conservan el valor vivo
- *(Test de host: `test/test_trend_file`.)*

#### Scenario: Un fichero del futuro se rechaza
- **WHEN** la última ranura del fichero es posterior a la ranura actual más
  una
- **THEN** la restauración se rechaza y el modelo no cambia
- *(Test de host: `test/test_trend_file`.)*

### Requirement: El almacenamiento persistente vive en un módulo `hmi_fs` reutilizable

El HMI SHALL montar LittleFS sobre la partición `spiffs` existente, sin
modificar la tabla de particiones, en un módulo `hmi_fs` que exponga montaje
idempotente (con un único formateo si el montaje falla), lectura completa,
información de espacio y escritura atómica (temporal + renombrado). El
montaje SHALL hacerse fuera de `LVGL_Lock` y después de que el panel RGB haya
reservado sus bounce buffers. Ninguna operación de `hmi_fs` SHALL ejecutarse
con `LVGL_Lock` tomado.

#### Scenario: El montaje es idempotente y tolera una partición virgen
- **WHEN** el HMI arranca con la partición `spiffs` sin formatear
- **THEN** el primer uso la formatea una vez y la monta, y los siguientes
  usos no repiten el montaje
- *(Verificación manual en banco, tarea 2.7: no hay entorno de test para
  LittleFS.)*

