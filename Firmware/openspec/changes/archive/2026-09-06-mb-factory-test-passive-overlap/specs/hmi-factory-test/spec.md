## ADDED Requirements

### Requirement: Varios tests en curso a la vez

La pantalla SHALL admitir varias filas de motherBoard en RUNNING
simultáneamente y resultados fuera de orden de id. La fila que sigue la
página SHALL ser la que recibió RUNNING más recientemente (el test activo del
momento). En la ordenación, dentro del bucket "en curso", WAIT SHALL ir antes
que RUNNING y el RUNNING más reciente primero. El watchdog por fila SHALL ser
`FTEST_ROW_TIMEOUT_MS` = 150 s: cubre los 90 s por test activo de la
motherBoard y los 45 s de plazo pasivo máximo con margen.

#### Scenario: Página que sigue al activo
- **WHEN** hay quince filas en RUNNING por los pasivos y llega
  `CTRL,FTEST,<id de actuators>,0`
- **THEN** la página salta a la fila de actuadores, no a la primera fila
  blanca de la lista
- *(Verificación manual en el CrowPanel.)*

#### Scenario: Pasivo legítimamente largo
- **WHEN** `gsm_at` lleva 100 s en RUNNING porque la parte activa tardó y su
  plazo de 45 s aún no se ha cumplido desde su arranque
- **THEN** el HMI no lo marca FALLA `timeout` antes de 150 s
- *(Verificación manual.)*
