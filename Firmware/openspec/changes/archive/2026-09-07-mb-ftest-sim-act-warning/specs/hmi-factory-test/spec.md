## ADDED Requirements

### Requirement: El contador de progreso no cuenta los omitidos

La barra de progreso y su texto `hechos/total tests` SHALL excluir los tests
en SKIP: `hechos` son las filas terminales que no son SKIP (PASA, FALLA,
AVISO) y `total` es el número esperado de tests menos los omitidos conocidos
hasta el momento. Al terminar, `hechos` SHALL coincidir con la suma de la
cabecera (errores + avisos + OK) y `total` con `hechos`.

#### Scenario: Batería con ocho omitidos
- **WHEN** la batería termina con 20 OK, 5 avisos, 1 error y 8 omitidos
- **THEN** la cabecera dice 1 error, 5 avisos, 20 OK y la barra `26/26 tests`
- *(Verificación manual en el CrowPanel: banco 2026-09-07, antes decía 34.)*
