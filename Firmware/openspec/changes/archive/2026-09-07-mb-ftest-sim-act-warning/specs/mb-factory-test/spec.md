## ADDED Requirements

### Requirement: Activación de la SIM Onomondo

El test `SIM_ACT` (id 28, pasivo) SHALL ejecutarse solo si el ICCID leído del
módem empieza por `ONOMONDO_ICCID_PREFIX` (`894573`); con otra SIM SHALL dar
SKIP `sim no onomondo`, y SKIP `sin sim` si `GSM_SIM` falló. Con SIM Onomondo
SHALL arrancar la activación contra la API en cuanto haya ICCID y WiFi, y dar
PASS si la API confirma la SIM activada (o ya activa). Si en
`FTEST_SIM_ACT_TIMEOUT_MS` (80 s) no pudo activarla SHALL dar **WARN** con el
motivo en `detail` (`sin iccid`, `sin wifi`, `sin respuesta`, o el error de la
API): la falta de red externa no es un fallo de la placa. El test NUNCA SHALL
dar FAIL.

#### Scenario: Banco sin WiFi con SIM Onomondo
- **WHEN** hay ICCID `894573…` y `WIFIIsConnected()` es falso durante 80 s
- **THEN** `SIM_ACT` da WARN `sin wifi` y el veredicto no pasa a HW ERROR por
  ese test
- *(Verificación manual en banco.)*

#### Scenario: SIM de otro operador
- **WHEN** el ICCID no empieza por `894573`
- **THEN** `SIM_ACT` da SKIP `sim no onomondo` sin contactar con la API
- *(Verificación manual en banco.)*

#### Scenario: Activación correcta
- **WHEN** hay WiFi y la API responde `activated: true`
- **THEN** `SIM_ACT` da PASS con el detalle de la activación
- *(Verificación manual en banco con red.)*
