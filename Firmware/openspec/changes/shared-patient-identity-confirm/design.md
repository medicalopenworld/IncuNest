## Context

`PatientState` (HMI) restaura al arrancar `pat_id`/`pat_ther` y llama a
`BabyWizard_SetSession()`. Desde ese momento `BabyWizard_HasLiveSession()` es
verdad en cuanto la placa informa de terapia, y la UI pinta nombre, edad y
minutos. Nada contrasta el `seq` con la placa. La placa guarda el bebé activo en
NVS (`KEY_ACTIVE_SEQ`) y lo fija al flanco de encendido de cualquier terapia
(`babyStore_setActiveSeq(s_wizardSeq)`), borrándolo al apagarse todas.

## Decisions

### D1. `activeSeq` como campo 24 de `CTRL,STATE`

`babyStore_getActiveSeq()`, `%u`, tras `latchedBitmask`. Otra rama añade campos
detrás: el 24 se reserva para este. La tabla de `PROTOCOL.md` se documentó en
otra rama hasta el 23; aquí se añade el 24 con la misma forma.

**Búfer `msg[192]` -> `msg[224]`.** Peor caso razonable de la línea completa
(`fwVer` de 20, enteros de hasta 11 caracteres donde el rango no está acotado,
tres máscaras de 10 caracteres, `%.2f` de hasta 8): unos 183 bytes con el campo
nuevo, a 9 del límite. Un `snprintf` truncado no da error: da una línea que el
HMI descarta entera y una pantalla que deja de refrescarse, así que se sube con
holgura en vez de apurar.

El HMI lo parsea con el mismo `sscanf` (`result >= 24`). Con menos campos queda
«desconocido» (`activeSeqKnown = false`), nunca `0`: `0` significa «la placa no
tiene bebé al mando», que es información.

### D2. Tabla de confirmación

Solo para una identidad **restaurada** (sin confirmar). Entradas: `sn` de la
placa, si manda el campo, `activeSeq`, y si hay terapia viva según la propia
placa (`actuation != 0 || phototherapyMode != 0` de `ctrl_state_msg`, no el
estado de los interruptores de la UI, que se sincroniza más tarde).

| Condición (por orden) | Acción |
|---|---|
| aún no ha llegado ningún `CTRL,STATE` | esperar |
| `sn` guardado distinto del de la placa | **borrar** |
| la placa manda el campo y `activeSeq == seq` | **confirmar** |
| la placa manda el campo y `activeSeq != 0` y `!= seq` | **borrar** |
| la placa manda el campo, `activeSeq == 0` y hay terapia viva | **borrar** |
| la placa manda el campo, `activeSeq == 0` y no hay terapia | esperar (no se pinta de todos modos) |
| la placa no manda el campo | contraste por lista (D3) |

Una identidad que sale de una `PROFILE_ACK` del asistente (la placa acaba de
responder por ella) nace **confirmada**.

### D3. Placa antigua: contraste por `PROFILE_LIST`

Una sola petición cuando el enlace está vivo. Si el bebé no está en la lista, o
está con otro nombre u otras semanas, se borra. Si está, la identidad queda
**no confirmada pero listada**: se pinta **gris y con «?»** tras el nombre, sin
edad ni minutos. Elegí gris con «?» y no ocultarla porque el operador sí gana
sabiendo que el HMI *cree* que es ese bebé; lo que no se permite es presentarlo
como cierto.

La petición no tiene consumidor propio del flag `g_pendingProfileList`: al llegar
su respuesta se baja el flag solo si ni el asistente ni el visor de bebés están
abiertos, para que una lista vieja no se tome por la respuesta fresca de la
siguiente petición de otro módulo.

### D4. Mientras no esté confirmada

Con una placa que manda el campo: **no se muestra nada** (ni identidad ni
minutos): la decisión llega con el primer `CTRL,STATE`, un segundo. Con placa
antigua: gris con «?» tras el contraste de lista; antes de la lista, nada.

### D5. Atajo `selectExisting`

`BabyWizard` solo lo toma si `BabyWizard_HasLiveSession()` **y**
`PatientState_IsConfirmed(seq)`. En formación (seq de prácticas, que
`PatientState` no gestiona) se considera confirmada: la lección usa el atajo.

### D6. `pat_id` v2 con `sn`

Tamaño 40 -> 44 bytes, `sn` de la placa en los últimos 4. Se captura de
`ctrl_state_msg.serialNumber` al fijar la identidad. v1 no se puede atar a nadie:
se descarta (y se borra la clave). `pat_ther` mantiene su v1; su `seq` ya la ata
a la identidad.

### D7. Acreditar minutos

Solo si la placa manda el campo y `activeSeq == s_acc.seq`; sin dato de la
placa, solo con la identidad confirmada. Coincide con la regla de la placa: ella
acredita al `seq` activo en el flanco de encendido, y si no tiene (`0`) no
acredita a nadie.

Los minutos son `known` solo si vienen de una siembra (lista del asistente), de
un `pat_ther` válido o de una resincronización. Sin eso se ocultan y no cuentan
(un contador que arrancase en 0 se mostraría como si fuera el total real).

### D8. Mismo `seq`, otro nombre (o semanas)

Es otro bebé: la placa reasignó el `seq`. Se reinician edad, acumulados y
confirmación. Las semanas de gestación no cambian en un bebé, así que también
cuentan.

### D9. `PROFILE_ACK 0`

`PROFILE_NEW` rechazado (placa llena) no dice nada del bebé que sigue dentro. El
olvido de la sesión recordada y `OnClear` solo ocurren si la espera era de un
`PROFILE_SELECT`.

### D10. Edad

Con hora de captura 0, o con más de 365 días transcurridos, la edad es
desconocida: sin hora no se puede afirmar cuántos días han pasado, y un año de
estancia neonatal es ya un dato corrupto. Si falta la hora **ahora** y hubo
captura, también desconocida (al arrancar, hasta que llega la hora del RTC o de
la placa): un contador de días congelado en el valor de hace horas es peor que
ninguno. `gestWeeks` se valida al decodificar (0, o 20-44; el asistente admite
20-40).

### D11. Layout

Una sola línea de 12 pt sin «Total» (`3h 20m`), **bajo el título** de cada
cabecera y con su misma alineación (izquierda en humedad y fototerapia, derecha
en temperatura). Las tres cabeceras tienen 50-54 px y el título (14 pt) ocupa
~16 centrados, así que la línea cae en los ~14 px libres de debajo, sin competir
en horizontal con el título ni con el botón. `ui_HumidityLabel` está en x=20,
y=-98, la misma fila que su botón y su panel: consistente.

Botón «Bebés»: detalle con `LV_LABEL_LONG_DOT`; los días de vida se acotan a
`99+` con una función pura (el formato pasa a `%s`+`d`).

### D12. Pruebas

Las decisiones (tabla D2, veredicto de lista, mismo bebé, acreditación, estado
de visualización, formatos) se extraen como funciones puras y se cubren en
`[env:native]` del HMI. El motor de `PatientState` en sí (NVS, hilos, UI) no se
pone detrás de interfaces de tiempo/NVS en este cambio: queda como deuda anotada;
la verificación de su pegamento es manual en banco.

## Risks

- Una placa que mande `activeSeq` pero con el bebé al mando por una vía que no
  fija `s_wizardSeq` verá su identidad borrada con terapia viva. Es el
  comportamiento pedido (la placa no sabe de quién es esa terapia).
- Las unidades con v1 pierden la identidad en el primer arranque con este
  firmware: se vuelve a elegir bebé. Aceptado; la alternativa es mostrar un
  bebé que no podemos atar a una placa.
