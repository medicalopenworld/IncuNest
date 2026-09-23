## MODIFIED Requirements

### Requirement: Activación de la SIM Onomondo

El test `SIM_ACT` (id 28, pasivo) SHALL ejecutarse solo si el ICCID leído del
módem empieza por `ONOMONDO_ICCID_PREFIX` (`894573`); con otra SIM SHALL dar
SKIP `sim no onomondo`, y SKIP `sin sim` si `GSM_SIM` falló. Con SIM Onomondo
SHALL arrancar la activación contra la API en cuanto haya ICCID y WiFi, y dar
PASS si la API confirma la SIM activada (ya activa, o activada en esta
pasada). Si la API **contesta** y la SIM queda sin activar (GET distinto de
200, cuerpo sin `activated`, PATCH rechazado) SHALL dar **FAIL** con el
motivo. Si **no se pudo consultar** la API (sin TLS/HTTP, 429/5xx
persistentes, sin clave aprovisionada en NVS, sin tarea) o si en
`FTEST_SIM_ACT_TIMEOUT_MS` (80 s) no hubo ICCID o WiFi, SHALL dar **WARN** con
el motivo (`sin iccid`, `sin wifi`, `sin respuesta`, `api N`, `sin key`): no se
conoce el estado de la SIM y no es un fallo de la placa.

La clave de la API SHALL leerse de NVS (`mb_cfg`/`onomondo_key`), escrita por
el flasher al flashear, y NO de una macro de compilación: la clave controla
todas las SIM de la organización y los binarios se publican. Una clave
ausente, vacía o que no valide SHALL producir WARN `sin key`, nunca un PASS
silencioso y nunca un FAIL — el estado de la SIM es desconocido, no erróneo.

#### Scenario: Banco sin WiFi con SIM Onomondo
- **WHEN** hay ICCID `894573…` y `WIFIIsConnected()` es falso durante 80 s
- **THEN** `SIM_ACT` da WARN `sin wifi` y el veredicto no pasa a HW ERROR por
  ese test
- *(Verificación manual en banco.)*

#### Scenario: SIM Onomondo que la API no activa
- **WHEN** hay WiFi, el GET devuelve 404 (SIM fuera de la cuenta) o el PATCH
  es rechazado
- **THEN** `SIM_ACT` da FAIL con `get 404` o `patch N` y el veredicto es
  HW ERROR
- *(Verificación manual en banco con red.)*

#### Scenario: SIM de otro operador
- **WHEN** el ICCID no empieza por `894573`
- **THEN** `SIM_ACT` da SKIP `sim no onomondo` sin contactar con la API
- *(Verificación manual en banco.)*

#### Scenario: Activación correcta o ya activa
- **WHEN** hay WiFi y la API responde `activated: true`, ya sea antes o tras
  el PATCH
- **THEN** `SIM_ACT` da PASS con `ya activada` o `activada`
- *(Verificación manual en banco con red.)*

#### Scenario: Placa sin clave aprovisionada
- **WHEN** hay ICCID `894573…` y WiFi, pero la NVS de la placa no trae
  `mb_cfg`/`onomondo_key`, o su valor no valida
- **THEN** `SIM_ACT` da WARN `sin key` sin abrir ninguna conexión con la API,
  y el veredicto no pasa a HW ERROR por ese test
- *(Verificación manual en banco con red y una placa sin aprovisionar.)*
