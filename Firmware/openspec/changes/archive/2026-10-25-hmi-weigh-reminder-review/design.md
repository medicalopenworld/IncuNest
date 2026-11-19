## Context

La revisión de `hmi-weigh-reminder` encontró que PESAR AHORA sellaba al
enviar, sin esperar a la placa. Ver `docs/hmi.md` §8.1 para el diseño vigente
y el procedimiento de banco.

## Decisiones

- **Confirmación = `CTRL,PROFILE_RANGE`.** Según `PROTOCOL.md` es la única
  respuesta a `HMI,PROFILE_WEIGHT`. Se compara el `seq` para ignorar rezagados
  de otro bebé.
- **Dos momentos de sello a propósito.** Asistente y Bebés no esperan
  respuesta; cambiarlos exigiría rediseñar sus máquinas de espera. Un sello al
  enviar que falla solo provoca un aviso de más.
- **Un único hueco de sello.** Se mantiene; la consecuencia (al volver a un
  bebé, el plazo cuenta desde "visto") se documenta en vez de ampliar la NVS.
- **Estado inyectable.** `weigh_env_t` (hora, bebé al mando, formación) separa
  la lógica de NVS y `BabyWizard`.
- **Armados independientes**, el de pesada sin consumirse sin bebé.

## Verificación

Lógica pura: `pio test -e native` (`test_weigh_reminder`, `test_weigh_state`).
UI, NVS y enlace: manual en banco (procedimiento en `docs/hmi.md` §8.1).
