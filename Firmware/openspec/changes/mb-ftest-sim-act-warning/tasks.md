## 1. motherBoard

Commit: `fix(motherboard): sim_act solo con SIM Onomondo y WARN si no puede activar`.

- [x] 1.1 `ONOMONDO_ICCID_PREFIX` en `factory_test_api.h`; `sim_act` hace SKIP
      con otra SIM y WARN (sin wifi / sin respuesta / error de API) en vez de
      FAIL.
- [x] 1.2 `pio run -e IncuNest_V18` en verde.

## 2. Display_HMI

Commit: `fix(hmi): el contador de progreso del test de hardware excluye los omitidos`.

- [x] 2.1 `renderVerdictAndProgress()`: hechos = terminales no SKIP, total =
      esperados − omitidos; rango de la barra actualizado.
- [x] 2.2 `pio run -e main` en verde.

## 3. Documentación

- [x] 3.1 `PROTOCOL.md` (nota v2.3.3 y fila 28), `docs/hardware.md`.

## 4. Verificación manual (banco)

- [ ] 4.1 **Manual** — sin WiFi: `sim_act` en amarillo con `sin wifi`; con SIM
      que no sea Onomondo no aparece.
- [ ] 4.2 **Manual** — el contador de la barra coincide con la suma de la
      cabecera (errores + avisos + OK).
