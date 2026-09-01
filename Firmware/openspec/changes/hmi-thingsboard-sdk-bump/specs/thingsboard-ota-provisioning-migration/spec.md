## ADDED Requirements

### Requirement: La actualización OTA sigue completando con éxito tras la migración de API

El ciclo de actualización OTA SHALL completar con éxito tras migrar
`Display_HMI/src/tasks/Wifi_OTA.cpp` a la API de `thingsboard-arduino-sdk`
v0.14+ (reemplazo de `OTA_Update_Callback`, `Firmware_Send_Info`,
`Start_Firmware_Update` por sus equivalentes nuevos), con el mismo resultado
observable que produce hoy contra v0.13.0: el firmware nuevo se descarga,
valida y aplica, y el dispositivo arranca con la versión nueva.

Esta es la ruta más sensible del cambio — toca arranque/carga de firmware
(`.claude/rules/security.md`) — y no tiene entorno de test: la única forma de
darla por buena es ejecutarla en hardware real contra un backend ThingsBoard
real.

#### Scenario: Ciclo de actualización OTA completo
- **WHEN** ThingsBoard publica un firmware nuevo para el dispositivo y
  Display_HMI, ya migrado a la API nueva, inicia la actualización
- **THEN** el firmware se descarga completo, se valida, se aplica, y el
  dispositivo arranca ejecutando la versión nueva
- *(Verificación manual en hardware real, obligatoria: no hay entorno de test
  en Display_HMI y esta ruta es de arranque/carga de firmware.)*

#### Scenario: Un fallo de descarga durante la actualización no deja el dispositivo en un estado no arrancable
- **WHEN** la descarga del firmware se interrumpe (p. ej. corte de red) a
  mitad de la actualización
- **THEN** el dispositivo conserva el firmware anterior funcional y no queda
  en un estado de bootloop ni de "waiting download mode" — coherente con la
  filosofía fail-safe de `.claude/rules/security.md`
- *(Verificación manual en hardware real: cortar la conexión de red a mitad
  de una actualización OTA y confirmar que el dispositivo arranca con
  normalidad.)*

### Requirement: El provisioning sigue completando con éxito tras la migración de API

El flujo de provisioning SHALL completar con éxito, obteniendo credenciales
válidas de ThingsBoard para el dispositivo, igual que hoy contra v0.13.0, tras
migrar la llamada a `Provision_Request` (reemplazo de
`Provision_Callback`/`Provision_Request` por la API nueva).

#### Scenario: Provisioning completo contra un backend real
- **WHEN** un dispositivo sin credenciales previas ejecuta el flujo de
  provisioning, ya migrado a la API nueva
- **THEN** el dispositivo obtiene credenciales válidas de ThingsBoard y queda
  operativo para publicar telemetría
- *(Verificación manual en hardware real contra un backend ThingsBoard real:
  no hay forma de simular esto en un entorno de test.)*

### Requirement: La telemetría publicada no pierde campos tras el cambio de firma de `sendTelemetryJson`

Display_HMI SHALL seguir publicando exactamente los mismos campos de
telemetría, con los mismos nombres y tipos, que publica hoy contra v0.13.0,
tras portar `sendTelemetryJson` de aceptar un `ArduinoJson::JsonObject` a
exigir un `const ArduinoJson::JsonDocument&`.

#### Scenario: Los campos de telemetría existentes no cambian tras el cambio de firma
- **WHEN** Display_HMI publica telemetría a ThingsBoard tras la migración de
  `sendTelemetryJson`
- **THEN** el payload recibido en ThingsBoard contiene los mismos campos,
  nombres y tipos que antes de la migración
- *(Verificación manual en banco, comparando el payload recibido en
  ThingsBoard antes y después de la migración.)*

### Requirement: Display_HMI compila limpio contra la API nueva

`pio run -e main` SHALL completar sin errores de compilación relacionados con
la API de ThingsBoard (OTA, provisioning, telemetría) tras la migración.

#### Scenario: Build limpio en el entorno de producción
- **WHEN** se ejecuta `pio run -e main` sobre Display_HMI ya migrado a la API
  nueva
- **THEN** la compilación termina en éxito, sin errores ni warnings nuevos
  relacionados con `OTA_Update_Callback`, `Provision_Callback`,
  `Firmware_Send_Info`, `Start_Firmware_Update`, `Provision_Request` ni
  `sendTelemetryJson`
- *(Cubierto por compilación: `pio run -e main`. No hay entorno `native` en
  Display_HMI.)*
