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

Nada de lo que sigue se ha visto funcionar en hardware. Son ~70 commits de
trabajo con cobertura nativa donde la había y **cero ejecución real**.

**Antes de empezar: flashear LAS DOS placas.** En esta tanda cambiaron el enum
de alarmas (17 condiciones), `CTRL,STATE` (dos campos nuevos), `CTRL,ALM`
(prioridad) y `CTRL,ALM_HISTORY` (campo `resolved`). Una placa vieja contra un
display nuevo descarta líneas enteras.

**Atajo:** el punto 11 (prueba de función de alarma) ejercita de una vez el
zumbador, el banner, los tres patrones de prioridad y los tres colores. Hazlo
el primero: si falla, casi todo lo demás fallará también y te ahorra el
recorrido.

### 11. ⬜ Prueba de función de alarma (201.12.3.105)

Ajustes → **PROBAR ALARMAS**, con el equipo sin ninguna alarma activa.

- Suenan **tres ráfagas** seguidas, de menos a más urgente, separadas por medio
  segundo: 1 pulso, luego 3, luego 10. Duración total ~6 s.
- La de ALTA se oye como **cinco pulsos, pausa, cinco pulsos** — no como diez
  seguidos. Esa agrupación es el patrón de la Tabla 3 y es lo que la hace
  reconocible; si suena monótona, el hueco de grupo no está saliendo.
- El banner aparece **en la propia pantalla de ajustes** (no solo en el
  bloqueo) y cambia de color con cada tramo: cian fijo → ámbar parpadeando
  lento → rojo parpadeando rápido.
- Con una alarma real activa, el botón **no hace nada**: la placa lo rechaza.
- Provoca una alarma real a mitad de la prueba (desconecta el ventilador): la
  prueba se **corta en el acto** y suena la alarma de verdad.

### 12. ⬜ Registro de alarmas

- Provoca una alarma y ábrela: aparece a la vez en **Activas** y en
  **Registro**, ahí como "sin resolver".
- Resuélvela **con el centro de alarmas abierto**: baja sola de Activas a
  Registro y la fila se pone **verde**, sin cerrar y reabrir.
- Si la placa no tiene hora (sin WiFi), la fila resuelta dice "resuelta" en vez
  de una fecha. Que salga verde igualmente es el punto: antes se quedaba en
  "sin resolver" para siempre porque el centinela de hora chocaba con el de
  resolución.
- Pulsa una fila del registro: sale el pop-up con la descripción. Si dice
  "descripción no disponible", la placa no está contestando `CTRL,ALM_DESC`.
- Reinicia el equipo: el registro **sobrevive**. Ojo, el blob subió a versión 2
  y los registros anteriores se descartan una sola vez — la primera vez tras
  actualizar sale vacío, y es correcto.

### 13. ⬜ Silencio por condición

- Con dos alarmas activas, silencia **una**: la otra sigue sonando (6.8.1).
- La fila silenciada muestra el icono de campana con X discontinua y el texto
  AUDIO EN PAUSA; la otra no.
- Aparece el icono permanente arriba a la derecha, **en todas las pantallas**.
- El botón cambia a **REANUDAR**. Púlsalo: el audio vuelve de inmediato (6.8.4).
- Deja pasar los **10 minutos** sin tocar nada: el audio vuelve solo y el botón
  se ofrece otra vez. Esto es lo que estaba roto y lo que más conviene
  confirmar; con cronómetro, porque el plazo hay que declararlo en las
  instrucciones de uso.
- Provoca el corte de red (desenchufa): su fila sale con **NO SILENCIABLE**
  desactivado, y no se puede callar (201.12.3.103).

### 14. ⬜ Banner y navegación

- El banner solo aparece en la **pantalla de bloqueo**. Sal del bloqueo con una
  alarma sonando: desaparece **al instante**, sin esperar al siguiente cambio
  de alarma. Ese era el fallo del cálculo perezoso.
- Pulsar el banner abre el centro de alarmas sin desbloquear.
- **Sin ninguna alarma**, pulsar el check verde de la pantalla principal abre
  el centro de alarmas. En el bloqueo el check **no** es pulsable, a propósito:
  ahí el toque desbloquea.

### 15. ⬜ Chasquido de confirmación

- Suena al tocar botones, interruptores y tarjetas.
- **No** suena al tocar el fondo ni un panel decorativo.
- **No** suena al escribir en el teclado del asistente de bebé (ni una tecla).
- **Sí** suena al cambiar de pestaña en las gráficas.
- Juicio a oído: ¿el clic se oye **más** que la alarma? Si es que sí —y hoy lo
  es— apúntalo: es el hallazgo de nivel sonoro que va con la medida acústica.

### 16. ⬜ Averías por cable

Las tres se provocan desconectando algo, sin calentar nada.

- **Sonda de piel en corto**: puentea los dos pines del conector. El log dice
  `CORTOCIRCUITO` con su resistencia, no `CIRCUITO ABIERTO`. En modo piel salta
  la alarma ALTA y corta calefactor; en modo aire, la BAJA.
- **Sonda desconectada**: el log dice `CIRCUITO ABIERTO`. Distinguirlas es todo
  el objetivo del cambio.
- **Ventilador**: desconecta el tacómetro (sin parar el aire) para probar el
  detector, o el ventilador entero para probar el corte.
- **Calefactor desconectado antes de arrancar**: sale `FALLO CALENTADOR` —el de
  cableado—, **no** `FALLO SENSOR CALENTADOR`. Que salga el segundo significaría
  que la separación quedó al revés.

### 17. ⬜ Enlace con el display

- Desconecta el cable serie con el equipo en marcha: a los **5 s** la placa
  declara `SIN ENLACE PANTALLA` (se ve en el log de la placa; el display, obvio,
  ya no lo pinta).
- Reconéctalo: la condición se retira sola.
- **Enciende solo la placa, sin display**: no debe declararla nunca. Un enlace
  que jamás existió no es un enlace caído, y si esto falla tendrás la alarma en
  cada arranque.
- **Con el display encendido y la placa muerta** (apaga solo la alimentación de
  la motherBoard, o desconecta el cable): el zumbador del **display** debe
  empezar a sonar el patrón MEDIA (3 pulsos de 150 ms, ráfaga cada 25 s) dentro
  de los 5 s siguientes al último `CTRL,STATE`/`CTRL,TEL` recibido. Es la
  comprobación clave de este caso: antes de este cambio, este escenario
  concreto no sonaba nada — solo el banner visual.
- **Con el cable desconectado y la placa viva** (el caso anterior, el habitual):
  los dos zumbadores suenan el mismo patrón, desfasados entre sí. Es lo
  esperado (docs/alarms.md §8) — el display no puede distinguir este caso del
  anterior, así que no lo intenta.
- **Botón de SILENCIAR con el enlace caído**: pulsarlo calla el zumbador del
  display y muestra el indicador AUDIO PAUSED con su cuenta atrás de 10 min.
  Pasados los 10 min sin que vuelva el enlace, el patrón se reanuda solo.
- **Reconectar durante una pausa activa**: al reconectar el cable, el
  indicador AUDIO PAUSED del display desaparece. Si el enlace se pierde de
  nuevo después, la señal debe sonar desde el primer instante — la pausa
  anterior no debe arrastrarse a la nueva pérdida.
- **Pulsar la pantalla durante una ráfaga**: el chasquido de confirmación debe
  omitirse sin alterar el ritmo de los 3 pulsos de la ráfaga en curso.

### 18. ⬜ Que nada de lo anterior rompió lo de antes

Rápido, pero conviene: control de temperatura en aire y en piel, humedad,
fototerapia con temporizador, asistente de bebé y su teclado, historial de
bebés. Se han tocado `Switch_cb`, los envíos de estado y el arranque de la UI.

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
