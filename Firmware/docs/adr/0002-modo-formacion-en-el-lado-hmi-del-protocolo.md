# ADR-0002: El modo formación se implementa en el lado HMI del protocolo, sin que la motherBoard lo sepa

- **Estado**: aceptado
- **Fecha**: 2026-09-04
- **Placas afectadas**: Display_HMI
- **Cambio OpenSpec**: `openspec/changes/hmi-cursos-formacion`

## Contexto

Los cursos de formación (`hmi-training-courses`) piden que el personal
pulse los controles reales de la pantalla: subir una consigna, encender el
control de temperatura, dar de alta a un bebé. En el firmware actual no hay
ninguna separación entre "la UI cambia" y "la incubadora cambia":

- Cualquier cambio local pone `hmi_msg.shouldSendData` y sale en la
  siguiente trama (`CommTask.cpp:1190-1196`); la motherBoard actúa sobre
  calefactor, humidificador y fototerapia.
- `Display_ApplyCtrlState()` (`CommTask.cpp:925+`) aplica el `CTRL,STATE`
  de la placa (~1 Hz) a switches, consignas y `hmi_msg`, con una gracia de
  eco de 2,5 s solo para cinco campos.
- El asistente de alta crea el perfil en la placa a mitad del recorrido
  (`Communication_SendProfileNew`, `BabyWizard.cpp:461`) y de ahí sale a
  ThingsBoard con `baby_seq`.

Es un dispositivo médico: una lección no puede encender un calefactor ni
crear un paciente.

## Opciones consideradas

| Opción | A favor | En contra |
|---|---|---|
| A. Actuar de verdad y restaurar al salir | Sin código nuevo en CommTask | El calefactor se enciende durante la lección; si el firmware se cae a mitad, el estado queda cambiado; los perfiles creados hay que borrarlos y ya han salido a ThingsBoard |
| B. Flag de formación en el protocolo; la motherBoard ignora órdenes y no persiste | Solución "limpia" en el sitio correcto | Toca `shared/`, `PROTOCOL.md` y las dos placas (cambio grande y coordinado) para un fin no clínico; hasta que las dos placas lo lleven no hay formación |
| C. Demostración pasiva para todo lo que toque actuadores | Cero riesgo | No es "interactivo de verdad", que es el requisito |
| **D. Cortar en la HMI**: mientras dura la lección, `CommTask` no envía cambios ni peticiones y no aplica el estado de la placa a la UI; las respuestas que esperan los asistentes se simulan localmente | Seguridad por construcción (si no se envía, la placa no actúa); cero cambios de protocolo; la UI se comporta igual que en real | La pantalla muestra estado simulado durante la lección; hay que mantener las simulaciones alineadas con el parser |

## Decisión

**D.** `training_mode.{h,cpp}` en `src/state/` con `Training_Enter()`,
`Training_Exit()`, `Training_IsActive()`. Mientras está activo:

1. El envío periódico a la placa sigue (keepalive y detección de enlace)
   pero con la **instantánea de `hmi_msg` tomada al entrar**: la placa
   recibe exactamente lo que ya tenía.
2. `Communication_Send*` de perfil, hora, prueba y silencio de alarma no
   envían; encolan una respuesta simulada que pone los mismos flags
   `g_pending*` que pondría el parser (`seq` de formación `0xFFFF`, rango
   NTE calculado con `shared/include/nte_table.h`, `TIME_ACK` aceptado).
3. `Display_ApplyCtrlState()` actualiza `ctrl_state_msg` (alarmas, enlace,
   barras) y retorna antes de tocar la UI o `hmi_msg`.
4. No se persiste en NVS nada cambiado durante la lección.
5. `Training_Exit()` restaura la instantánea local, anula la gracia de eco y
   deja que el siguiente `CTRL,STATE` vuelva a mandar.

Se combina con un **gate clínico** (sin terapia activa, sin alarma, con
enlace, sin perfil activo, sin apagado en curso) y una franja fija
"MODO FORMACION" en pantalla: el sandbox evita que la incubadora cambie; el
gate evita que se tape el estado real con un bebé dentro.

## Consecuencias

- La motherBoard no cambia ni una línea. `PROTOCOL.md` intacto.
- `CommTask.cpp` gana un punto de decisión por cada `Communication_Send*`
  y uno en `Display_ApplyCtrlState()`. Las simulaciones viven junto al
  parser con comentario cruzado: si cambia el formato de una respuesta, hay
  que tocar los dos sitios.
- El estado que ve el alumno durante la lección es simulado (consignas,
  toggles), pero las **medidas** (temperaturas, humedad) siguen siendo las
  reales de la placa, porque la telemetría (`CTRL,TEL`) no se corta. Los
  textos de las lecciones lo dicen.
- **Deuda explícita**: si más adelante se quiere que la motherBoard sepa que
  hay formación (p. ej. para registrar en ThingsBoard "formación en curso"
  o para bloquear su propio botón físico), se pasa a la opción B como
  cambio de `shared/` aparte; este ADR no lo impide, la HMI ya tiene el
  interruptor centralizado.
- **Revisar este ADR si**: la placa deja de aceptar el estado repetido como
  keepalive, o si alguna respuesta nueva del protocolo deja de tener
  simulación (síntoma: un asistente se queda esperando en formación).
