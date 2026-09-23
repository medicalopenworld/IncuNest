## 1. Display_HMI

Commit: `fix(hmi): sin logs en secciones criticas, anillo de 32, cabecera desde filas, aviso en pop-up`.

- [x] 1.1 `CommTask.cpp`: log de descarte fuera de la sección crítica; revisar
      todas las secciones críticas del fichero.
- [x] 1.2 `FTEST_RING_LEN` 32 y contador de descartes visible.
- [x] 1.3 Cabecera y persistencia desde las filas; `FTEST_DONE` solo cierra.
- [x] 1.4 Aviso de entrada como pop-up modal.
- [x] 1.5 Drenado del anillo independiente del estado de la UI.
- [x] 1.6 `pio run -e main` en verde.

## 2. motherBoard

Commit: `fix(motherboard): buzzer sin confirmacion, power_src y sb_door omitidos, charger WARN sin vbus`.

- [x] 2.1 `buzzer` → SKIP `sin microfono` sin CONFIRM; infraestructura de
      CONFIRM retirada si queda sin uso.
- [x] 2.2 `power_src` y `sb_door` → SKIP `omitido`.
- [x] 2.3 `charger` → WARN `sin vbus` sin respuesta.
- [x] 2.4 Pausa de 60 ms entre tests.
- [x] 2.5 `pio run -e IncuNest_V18` y `pio test -e native` en verde.

## 3. Documentación

- [x] 3.1 `PROTOCOL.md`, `docs/hmi.md`, `docs/hardware.md` (pendientes para
      cuando el jig alimente por VBUS y monte la puerta).

## 4. Verificación manual (banco)

- [ ] 4.1 **Manual** — batería completa a batería sin ningún reinicio del HMI
      ni "anillo lleno".
- [ ] 4.2 **Manual** — la cabecera cuadra con los botones rojos y el veredicto.
- [ ] 4.3 **Manual** — el zumbador no pregunta; el aviso inicial es un pop-up.
