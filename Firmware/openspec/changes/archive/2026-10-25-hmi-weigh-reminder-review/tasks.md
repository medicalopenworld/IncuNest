El código de esta revisión ya está en la rama; este change solo pone al día el
spec. Las tareas de código se hicieron en los commits `fix(hmi)` citados.

## 1. Display_HMI: revisión de código

- [x] 1.1 Sello con confirmación desde PESAR AHORA (`9730199a`).
- [x] 1.2 No consumir el armado de pesada sin bebé; encadenado y re-armado (`9730199a`).
- [x] 1.3 `s_loaded` y reintento de apertura de NVS; máquina de estados pura con tolerancia de 60 s y tests (`c04b4184`).
- [x] 1.4 Descarte del rango rezagado (`9730199a`).
- [x] 1.5 Interruptor `wgh_en` (`dc094532`).
- [x] 1.6 `docs/hmi.md` §8.1 (`598c9fcb`).

## 2. Verificación

- [x] 2.1 `pio test -e native` en verde (86 casos) y `pio run -e main` en verde.
- [ ] 2.2 **Manual, PENDIENTE en banco**: procedimiento de `docs/hmi.md` §8.1, incluido el enlace caído y el diseño del panel Mantenimiento en los cuatro idiomas.
