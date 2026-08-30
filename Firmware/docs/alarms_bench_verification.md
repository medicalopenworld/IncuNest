# Verificación en banco del sistema de alarmas

Lo que el firmware no puede verificar por sí mismo. Los 186 tests nativos
cubren la lógica pura (`alarm_machine`, `alarm_policy`, `alarm_window`,
`alarm_text`, `alarm_history`, `alarm_test`); `security.cpp`, `Buzzer.cpp`, la integración completa y todo el
display **no tienen ninguna cobertura en ejecución**. Compilar no es
funcionar, así que esta lista es la única evidencia de que el sistema hace lo
que dice.

Cada punto lleva el resultado observado. Los pendientes se anotan aquí cuando
se ejecuten, no en notas sueltas.

Leyenda: ✅ verificado · ❌ falló · ⬜ pendiente

---

## 1. El audio no cesa por tiempo — ✅ verificado

**Por qué importa.** IEC 60601-1-8 6.10: las señales acústicas cesan **solo**
cuando el operador activa un estado de inactivación o resetea la alarma. El
código anterior se callaba solo a los ~250 s (`buzzerAlarmBeepCount`, 500
conmutaciones × 500 ms), lo que era una no conformidad.

**Cómo se prueba.** Provocar una alarma cualquiera, no tocar nada, y
cronometrar. Los 10 min del enunciado original **no son un parámetro del
código**: son un tiempo de observación elegido por ser más del doble del corte
antiguo, de modo que si sigue sonando no queda duda de que el agotamiento ya
no existe. Con 5 min largos basta.

**Esperado.** Ráfagas de 10 pulsos (ALTA) cada 10 s, indefinidamente.

**Observado.** Sigue sonando. La no conformidad queda cerrada.

## 2. El operador puede silenciar desde el display — ❌ falló, corregido, pendiente de re-verificar

**Observado en banco.** **No aparece el botón de silencio.**

**Causa raíz.** Defecto preexistente en `Display_HMI`, no introducido por esta
rama. `ui_MuteAlarm` se crea en `ui_ScreenAlarms` y se oculta al arrancar
(`UITask.cpp`, `ui_init`), y `MuteAlarm_cb()` lo vuelve a ocultar al pulsarlo,
pero **no existía ni un solo `lv_obj_clear_flag()` en todo el fichero**: el
botón nunca llegaba a mostrarse. El callback funcionaba; lo que faltaba era
hacerlo visible.

Pasaba desapercibido porque el zumbador se agotaba solo a los ~4 min, así que
nadie echaba de menos poder callarlo. Desde que el punto 1 está cerrado, esto
es la diferencia entre una alarma que se puede silenciar y una que no.

**Corregido.** `update_alarm_panels()` muestra el botón mientras haya alarma
activa no silenciada, y lo oculta en caso contrario. El silencio se cancela
—y el botón reaparece— al limpiarse todas las alarmas o al llegar una alarma
nueva (`CommTask.cpp`, "Si se activa una alarma nueva, desmutar"), que es lo
que exige 6.8.1: silenciar una condición no puede dejar mudas las siguientes.

**Pendiente de re-verificar en banco:**
- ⬜ El botón aparece al saltar una alarma.
- ⬜ Al pulsarlo, el zumbador calla.
- ⬜ El audio **vuelve a los 2 minutos** (`ALARM_AUDIO_PAUSE_MS` = 120 000 ms)
      si la condición sigue viva.
- ⬜ La señal visual **no** desaparece al silenciar (6.8.1 lo prohíbe).
- ⬜ Con una alarma silenciada, una alarma nueva **sí** suena.

## 3. Los textos de alarma llegan al display en español — ✅ verificado

**Por qué importa.** El parser de `CTRL,ALM` usaba `%31[^,]` y **descartaba la
línea entera** cuando la descripción superaba 31 caracteres. 28 de las 48
descripciones (16 condiciones × 3 idiomas) la superan, y en español —el idioma
por defecto— fallaban 12 de 16: esas alarmas cortaban el calefactor y sonaban,
pero **no aparecían en pantalla**. Los anchos se derivan ahora de
`ALARM_TYPE_LEN`/`ALARM_DESC_LEN` y no pueden volver a divergir del tamaño real
de los buffers.

**Observado.** Sí sale la descripción de alarmas en español.

## 4. Sonda de piel desconectada en modo AIRE — ✅ verificado

**Por qué importa.** `applyNTCResult()` no refresca el sello de tiempo cuando
la lectura cae fuera de la ventana de descarte, que es lo que ocurre sin sonda.
Sin distinguir **ausencia** de **fallo**, en la configuración de fábrica
—modo AIRE, sin sonda conectada— aparecía una alarma BAJA permanente que no se
retiraba nunca: fatiga de alarma de manual.

**Corregido antes de la prueba.** En modo AIRE la condición solo se declara si
la sonda llegó a leer correctamente alguna vez en este ciclo de alimentación.
Una sonda que nunca se conectó es una ausencia, no un fallo. En modo PIEL no
cambia nada: allí la sonda es obligatoria y su ausencia sí es un fallo.

**Observado.** No aparece alarma. Es el comportamiento pretendido.

**Sigue sin verificarse el caso complementario:**
- ⬜ Sonda **conectada y funcionando** en modo AIRE que después falla:
      ¿aparece entonces la alarma BAJA? Es el caso que el latch debe dejar
      pasar, y el que confirma que la corrección no lo silencia todo.

## 5. El umbral de obstrucción no da falsos positivos — ✅ verificado (parcial)

**Por qué importa.** `FAN_DUTY_BLOCKED_THRESHOLD` corta el calefactor
(201.12.3.101). Valía 160, y con el calefactor a máxima potencia la tensión se
hunde y el PID sube el duty hasta ~158 para sostener `FAN_TARGET_RPM`: el
umbral quedaba justo encima de la compensación legítima, así que un pico normal
disparaba la alarma y enfriaba al bebé.

**Cómo se prueba.** Calefactor a máxima potencia, salida de aire limpia,
registrar el duty que el PID necesita para sostener las rpm. El arranque ya lo
registra en el log.

**Esperado.** Duty por debajo de 190 con margen.

**Observado.** Se queda por debajo de 190.

**Lo que esto NO demuestra.** Que 190 detecte una obstrucción real, ni que
aguante la dispersión entre unidades. Falta:
- ⬜ Obstruir la salida y confirmar que el duty sube por encima de 190, que la
      alarma salta a los 5 s (`AIR_BLOCKED_SUSTAIN_MS`) y que el calefactor se
      corta.
- ⬜ Repetir en varias unidades para acotar la dispersión y ajustar el umbral
      con datos, no con estimación.

---

## Pendientes

Ninguno se ha ejecutado todavía.

### 6. ⬜ Fallo térmico de aire

Elevar el aire por encima de `in3.airTemperatureSetMax` (38 °C por defecto).
- El calefactor se corta.
- Al enfriarse, la alarma **NO se limpia sola**: es *latching* por
  201.15.4.2.1 aa)/bb).
- El calefactor **sí** vuelve a estar disponible al bajar la temperatura,
  aunque la alarma siga: el corte depende de la condición física, no de la
  señal.
- La alarma solo se quita **apagando el equipo**, y eso es deliberado: que el
  corte térmico salte significa que el termostato ha fallado —así lo describe
  201.15.4.2.1 aa)— así que es una condición de servicio, no algo que el
  operador deba poder borrar para seguir usando el equipo. La norma exige
  reset manual sin especificar el mecanismo; el ciclo de alimentación lo es.
- **Comprobar que el texto en pantalla lo dice.** Los textos se reescribieron
  precisamente para esto: el título es "FALLO TERMICO AIRE" y no "corte", y la
  descripción avisa de que el aviso queda fijo hasta reiniciar. Sin esa
  última frase el operador cree que la pantalla se ha quedado colgada.

### 7. ⬜ Desviación de temperatura

Modo AIRE, +3 °C sobre consigna: alarma **y** corte de calefactor.
Modo AIRE, −3 °C: alarma y el calefactor **sigue encendido** (dd) es explícito).
Modo PIEL, los mismos dos casos con ±1 °C (ee)).

Durante los primeros 30 min tras activar el control, el lado frío no debe
alarmar —la norma condiciona la desviación a haber alcanzado condiciones
estables— pero el lado caliente **sí corta el calefactor desde el primer
instante**.

### 8. ⬜ Arranque con la salida de aire obstruida

El autotest declara la obstrucción, el firmware pone el PID del ventilador en
manual, y el primer ciclo de vigilancia **no debe borrar** ese resultado.

### 9. ⬜ Cambio de modo AIRE ↔ PIEL con alarma de sonda viva

Al cambiar de modo, la condición del par contrario debe retirarse. Si queda
colgada, la alarma no se limpia jamás.

### 10. ⬜ Telemetría a nube

Las alarmas siguen publicándose. Aviso: `temp_alarm` **ya no existe** — se
partió en `air_temp_high/low_alarm` y `skin_temp_high/low_alarm`, y los paneles
de ThingsBoard que la usen hay que migrarlos.

---

## Bloque B — todo lo acumulado sin verificar

Nada de lo que sigue se ha visto funcionar en hardware. Son ~80 commits con
cobertura nativa donde la había y **cero ejecución real**.

Cada punto lleva **Preparación / Pasos / Debe ocurrir / Si falla**. El apartado
"si falla" no es relleno: dice dónde mirar, y está para que un fallo no se
convierta en una sesión de depuración a ciegas como la del indicador de AUDIO
PAUSED (`known_issues.md` #6).

### Antes de empezar — dos cosas que ya han costado tiempo

**1. Flashea LAS DOS placas.** En esta tanda cambiaron el enum de alarmas
(17 condiciones), `CTRL,STATE` (tres campos nuevos y uno con significado nuevo),
`CTRL,ALM` (prioridad) y `CTRL,ALM_HISTORY` (campo `resolved`). Una placa vieja
contra un display nuevo **descarta líneas enteras**, y los síntomas no se
parecen en nada a la causa.

**2. Comprueba que el binario es el que crees.** Ajustes → Información: la
versión del HMI lleva pegada la marca de compilación, del estilo
`4.0.0 (Aug 17 2026 00:14:32)`. Si no coincide con la hora a la que compilaste,
**para y flashea otra vez**. Dos rondas de depuración se fueron en una placa
sin reflashear.

**Orden recomendado.** Haz el **11** primero: ejercita de una vez el zumbador,
el banner, los tres patrones y los tres colores. Si falla, casi todo lo demás
fallará y te ahorra el recorrido. Después el **17**, porque un enlace que no se
recupera bien contamina todas las lecturas posteriores.

---

### 11. Prueba de función de alarma (201.12.3.105) — pendiente

**Preparación.** Equipo encendido y **sin ninguna alarma activa**: si el icono
de alarmas de la barra muestra un número, resuelve antes lo que haya.

**Pasos.**
1. Abre el centro de alarmas — icono de la barra, o el check verde si no hay
   alarmas.
2. Pulsa **PROBAR**, arriba a la izquierda de la tarjeta.

**Debe ocurrir.** Una secuencia de unos 6 s con tres ráfagas, de menos a más
urgente, separadas por medio segundo:

| Tramo | Pulsos | Banner |
|---|---|---|
| BAJA | 1 | cian, **fijo** |
| MEDIA | 3 | ámbar, parpadeo **lento** (~0,75 s) |
| ALTA | 10 | rojo, parpadeo **rápido** (~0,25 s) |

Y tres detalles que son el objeto de la prueba:

- La de ALTA se oye como **cinco pulsos, pausa, cinco pulsos**. No como diez
  seguidos. Esa agrupación (el hueco de `2x + y` de la Tabla 3) es lo que hace
  reconocible la máxima prioridad frente a otro equipo de la sala.
- El banner se pinta **encima de la propia tarjeta** del centro de alarmas.
  Antes se suprimía ahí y la prueba salía muda de vista.
- Cada pulso tiene **rampa de subida y bajada**; no arranca de golpe.

**Si falla.**
- *No pasa nada y sale el aviso «Hay una alarma activa»*: hay una condición
  señalizando que la pantalla puede no estar mostrando. Mira el log de la
  placa (`[ALARM] prueba rechazada: bitmask=0x…`) y decodifica el bitmask.
- *Suena pero no hay banner*: el campo `almTest` de `CTRL,STATE` no llega.
- *Banner pero no suena*: comprueba si el zumbador de la placa suena con
  alarmas reales; si tampoco, el problema no es la prueba.
- *Los diez pulsos suenan uniformes*: no se aplica el hueco de grupo —
  `gapAfterPulse()` en `Buzzer.cpp`.

**Comprobación adicional.** Con una alarma real activa el botón debe negarse y
decirlo. Y provocando una alarma a mitad de la prueba (desconecta el tacómetro
del ventilador), la secuencia debe **cortarse en el acto** y sonar la alarma de
verdad: una prueba nunca puede pisar una alarma real.

---

### 12. Registro de alarmas (6.12.2) — pendiente

**Preparación.** Ten a mano una alarma fácil de provocar y de quitar. La de
sonda de piel desconectada en modo aire es la más cómoda.

**Pasos.**
1. Provoca la alarma y abre el centro de alarmas.
2. Con el centro **abierto**, resuelve la alarma (reconecta la sonda).
3. Pulsa la fila del registro.
4. Reinicia el equipo y vuelve a abrir el registro.

**Debe ocurrir.**
- Paso 1: la alarma aparece **a la vez** en *Activas* y en *Registro*, ahí como
  «sin resolver».
- Paso 2: baja sola de *Activas* a *Registro* y la fila se pone **verde**, sin
  cerrar y reabrir el centro.
- Paso 3: sale un pop-up con la descripción de la alarma.
- Paso 4: el registro **sobrevive** al reinicio.

**Si falla.**
- *La fila no baja sola*: el display no vuelve a pedir el registro al cambiar
  el conjunto de alarmas activas (`activeIdSignature()`).
- *Dice «descripción no disponible»*: la placa no contesta `CTRL,ALM_DESC`.
- *El registro sale vacío tras actualizar*: **es correcto la primera vez**. El
  blob de NVS subió a versión 2 y lo anterior se descarta una sola vez. Si sale
  vacío la segunda vez, entonces sí es un fallo.

**Sobre la hora.** Sin WiFi la placa no tiene reloj, así que la fila resuelta
dirá «resuelta» en lugar de una fecha. **Que salga verde igualmente es el
punto**: antes se quedaba en «sin resolver» para siempre, porque el centinela
de hora chocaba con el de resolución.

---

### 13. Silencio por condición (6.8.1, 6.8.4, 6.8.5, 201.12.3.104) — pendiente

> **Este punto tiene un fallo abierto**: `known_issues.md` #6 — el indicador no
> se dibuja al silenciar. Ejecútalo igual, sirve para acotar el fix, pero
> **empieza por la cuenta atrás**: es la sonda más barata. Si no baja,
> `CTRL,STATE` no está llegando y el problema es de transporte, no de interfaz.

**Preparación.** Dos alarmas activas a la vez, de condiciones distintas.

**Pasos.**
1. Silencia **una** de las dos.
2. Observa la cuenta atrás de la esquina inferior derecha unos 10 s.
3. Pulsa **REANUDAR** en esa misma fila.
4. Vuelve a silenciarla y deja pasar los **10 minutos** completos, con
   cronómetro.
5. Desenchufa la alimentación de red para provocar esa alarma.

**Debe ocurrir.**
- La otra alarma **sigue sonando**: silenciar una no afecta a las demás (6.8.1).
- La fila silenciada muestra la campana con X **discontinua** y «AUDIO EN
  PAUSA»; la otra no.
- Abajo a la derecha aparece la misma campana con la cuenta atrás, **en todas
  las pantallas**, y el número **baja segundo a segundo**.
- El botón dice **REANUDAR** en verde; al pulsarlo el audio vuelve de inmediato.
- A los 10 minutos el audio vuelve solo y el botón se ofrece otra vez.
- La fila del corte de red sale con **NO SILENCIABLE** desactivado y no se
  puede callar: 201.12.3.103 exige 10 min de aviso y la pausa dura justo eso.

**Si falla.** Consulta la tabla de descartes de `known_issues.md` #6 antes de
investigar nada. El silencio en la placa, el formato, el `sscanf`, la longitud
de línea, el índice de bit y el recorte del icono **ya están descartados con
evidencia**.

**Anota el plazo medido.** 6.8.5 obliga a declarar la duración del AUDIO PAUSED
en las instrucciones de uso, así que el número del cronómetro es dato de
expediente, no una curiosidad.

---

### 14. Banner y navegación — pendiente

**Pasos.**
1. Con una alarma sonando y la pantalla bloqueada, observa el banner.
2. Púlsalo.
3. Desbloquea y ve a la pantalla principal, con la alarma aún activa.
4. Resuelve todas las alarmas y pulsa el **check verde** de la principal.
5. Repite el paso 4 en la pantalla de bloqueo.

**Debe ocurrir.**
- El banner solo se ve **en el bloqueo**, arriba, con el color y el parpadeo de
  la prioridad más alta.
- Pulsarlo abre el centro de alarmas **sin desbloquear**.
- Al salir del bloqueo **desaparece al instante**, no al siguiente cambio de
  alarma. Ese era el fallo del cálculo perezoso.
- El check verde de la principal **abre el centro de alarmas**.
- En el bloqueo el check **no** es pulsable: ahí el toque desbloquea, y hacerlo
  pulsable crearía una zona muerta permanente.

**Si falla.** Que el banner tarde en desaparecer significa que
`alarm_banner_update()` ha dejado de llamarse en cada pasada del bucle de UI.

---

### 15. Chasquido de confirmación — pendiente

**Pasos.** Toca, por este orden: un botón, un interruptor, una tarjeta del
centro de alarmas, el **fondo** de una pantalla, un panel decorativo, las
**teclas** del asistente de bebé y una **pestaña** de las gráficas.

**Debe ocurrir.**

| Toque | ¿Suena? |
|---|---|
| Botón, interruptor, tarjeta | **Sí** |
| Fondo o panel decorativo | **No** |
| Teclas del asistente | **No**, ni una |
| Pestaña de gráficas | **Sí** |

**Si falla.** Que suene en el fondo significa que el filtro volvió a mirar
`LV_OBJ_FLAG_CLICKABLE`, que LVGL pone en **todos** los objetos. Que suene en
el teclado significa que se filtró solo por `lv_keyboard_class`, y el teclado
del asistente es un `lv_btnmatrix` a propósito.

**Juicio a oído, y es importante.** ¿El clic se oye **más** que la alarma? Hoy
sí, y está al revés: el transductor de las alarmas debe ser lo más audible del
equipo. Anótalo, porque es entrada de la medida acústica pendiente y puede
acabar siendo un cambio de hardware.

---

### 16. Averías por cable — pendiente

Las cuatro se provocan desconectando algo, sin calentar nada. Necesitas el log
de la placa a la vista.

| Avería | Cómo | Debe salir |
|---|---|---|
| Sonda de piel **en corto** | puentea los dos pines del conector | log `CORTOCIRCUITO` con su resistencia |
| Sonda **desconectada** | quítala | log `CIRCUITO ABIERTO` |
| **Ventilador** | desconecta el tacómetro (prueba el detector) o el ventilador entero (prueba el corte) | `FALLO VENTILADOR`, ALTA, corta calefactor |
| **Calefactor** desconectado antes de arrancar | quítalo y enciende | `FALLO CALENTADOR` |

**Qué se comprueba en cada una.**
- Corto y circuito abierto **tienen que distinguirse**: es todo el objetivo de
  la clasificación por resistencia. Si los dos dicen lo mismo, no sirvió.
- La sonda en corto **en modo piel** es ALTA y corta calefactor; **en modo aire**
  es BAJA. La urgencia depende del modo, no de la avería.
- El calefactor desconectado debe dar `FALLO CALENTADOR` —el de cableado—, **no
  `FALLO SENSOR CALENTADOR`**. Que salga el segundo significaría que la
  separación de las dos averías quedó al revés.

---

### 17. Enlace entre placas, en los dos sentidos — pendiente

**Pasos.**
1. Con el equipo en marcha, **desconecta el cable serie** y cronometra.
2. Observa la pantalla del display.
3. Reconecta.
4. Aparte: enciende **solo la placa**, sin display, y espera un minuto.

**Debe ocurrir.**
- A los **5 s** la placa declara `SIN ENLACE PANTALLA`, prioridad MEDIA: 3
  pulsos cada ~25 s. Se ve en el log de la placa; el display, obviamente, ya no
  lo pinta.
- El display, por su lado, muestra el banner **SIN ENLACE CON LA PLACA** en
  cualquier pantalla, y **las medidas pasan a `--`** con las barras a cero.
  Esta es la parte de seguridad: una cifra congelada no se ve congelada.
- Las **consignas se quedan**: son lo que pediste, no una medida.
- Al reconectar, todo vuelve solo, sin reiniciar.
- Arrancando sin display, la placa **no debe declarar la alarma nunca**. Un
  enlace que jamás existió no es un enlace caído; si esto falla, tendrás la
  alarma en cada encendido.

**Si falla.** Que las cifras vuelvan a su valor viejo un segundo después de
borrarse significa que alguno de los ocho llamantes de `update_labels()` está
repintando sin respetar la guarda.

---

### 18. No-regresión de lo que ya funcionaba — pendiente

Rápido, pero no lo saltes: esta tanda tocó `Switch_cb`, los envíos de estado,
el arranque de la UI y el motor del zumbador.

- Control de temperatura en **modo aire** y en **modo piel**, con sus flechas de
  consigna.
- **Humedad**: activar, ajustar, desactivar.
- **Fototerapia** con temporizador: `+`/`−`, iniciar, cancelar y dejar que
  expire sola.
- **Asistente de bebé** completo, incluido escribir el nombre con el teclado.
- **Historial de bebés**: lista, alta y gráfica de peso.
- **Idiomas**: cambia a inglés y a francés y vuelve. Los textos nuevos —banner,
  centro de alarmas, botón PROBAR— deben traducirse con el resto.

**Presta atención a la fluidez.** Si la interfaz va lenta, sospecha de algo que
se repinta en cada pasada del bucle de UI: ya pasó una vez con el banner, por
llamar a `lv_label_set_text()` y `lv_obj_move_foreground()` incondicionalmente.

---

## Fuera del alcance del banco

Requieren laboratorio o hardware que no existe todavía:

- **Nivel sonoro**: ≥65 dB(A) a 3 m (201.9.6.2.1.102) y ≤80 dB(A) dentro del
  compartimento (201.9.6.2.1.103). Tiran en sentidos opuestos.
- **Corte térmico independiente del termostato** (201.15.4.2.1 aa): hoy el
  corte lee el mismo sensor que el PID. Exige un segundo canal físico.
- **Alarma de corte de red durante 10 min** (201.12.3.103): exige reserva de
  energía dedicada.
- **Cortocircuito en la sonda de piel** (201.12.3.102): el timeout no lo
  detecta.
- **Prueba de función de alarma para el operador** (201.12.3.105): no
  implementada.
