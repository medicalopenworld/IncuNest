## 1. Punto de partida — coordinación con motherBoard

- [ ] 1.1 Comprobar el estado de `mb-thingsboard-sdk-bump` (tarea 1, decisión
      de arquitectura). Si esa decisión fue **C (descartar la subida)**, cerrar
      este change sin implementar nada más, por coherencia de versiones entre
      placas — ver D3 de `design.md` (no es una dependencia técnica dura, es
      una decisión de proyecto).
- [ ] 1.2 Si se continúa, releer el código fuente de
      `thingsboard-arduino-sdk` v0.15.0 (o la versión objetivo vigente) para
      las firmas nuevas exactas de `Firmware_Send_Info`,
      `Start_Firmware_Update`, `Provision_Request` y `sendTelemetryJson` — no
      se investigó a ese nivel de detalle en este proposal (ver Open Questions
      de `design.md`).

## 2. Migración de la API de actualización OTA

Commit: `feat(hmi): migrar Wifi_OTA.cpp a la API de OTA de thingsboard-arduino-sdk v0.15.0`.

- [ ] 2.1 Reemplazar `OTA_Update_Callback` por el tipo/patrón equivalente de
      la API nueva.
- [ ] 2.2 Portar la llamada a `Firmware_Send_Info` a su equivalente nuevo,
      confirmando que la información de versión/tamaño de firmware enviada no
      cambia de semántica.
- [ ] 2.3 Portar la llamada a `Start_Firmware_Update` a su equivalente nuevo.
- [ ] 2.4 `pio run -e main` en verde para esta sección, sin errores de
      `OTA_Update_Callback`/`Firmware_Send_Info`/`Start_Firmware_Update`.

## 3. Migración de la API de provisioning

Commit: `feat(hmi): migrar provisioning a la API de thingsboard-arduino-sdk v0.15.0`.

- [ ] 3.1 Reemplazar `Provision_Callback` por el tipo/patrón equivalente de la
      API nueva.
- [ ] 3.2 Portar la llamada a `Provision_Request` a su equivalente nuevo.
- [ ] 3.3 `pio run -e main` en verde para esta sección, sin errores de
      `Provision_Callback`/`Provision_Request`.

## 4. Migración de `sendTelemetryJson`

Commit: `fix(hmi): adaptar sendTelemetryJson a la firma JsonDocument de thingsboard-arduino-sdk v0.15.0`.

- [ ] 4.1 Portar la(s) llamada(s) a `sendTelemetryJson` de `JsonObject` a
      `const JsonDocument&`, confirmando que no cambia qué campos se publican
      (ver Riesgo de `design.md` sobre la vida útil del objeto).
- [ ] 4.2 `pio run -e main` en verde, sin warnings nuevos.

## 5. Build y dependencias

Commit: `chore(hmi): subir thingsboard-arduino-sdk a v0.15.0 y TBPubSubClient a v2.12.1`.

- [ ] 5.1 Actualizar `Display_HMI/platformio.ini`: `thingsboard-arduino-sdk` a
      v0.15.0 y `TBPubSubClient` a v2.12.1, en los tres entornos
      (`main`, y los que compartan `lib_deps`).
- [ ] 5.2 `pio run -e main` completo en verde, cero warnings nuevos en
      cualquier fichero tocado.

## 6. Verificación en hardware (manual — obligatoria, no hay entorno de test)

- [ ] 6.1 **Verificación manual en hardware real** (documentar qué se probó y
      con qué resultado): ciclo completo de actualización OTA contra un
      backend ThingsBoard real, incluyendo el caso de corte de red a mitad de
      la descarga (el dispositivo debe conservar el firmware anterior
      funcional, sin bootloop ni "waiting download mode" — ver
      `Firmware/docs/known_issues.md` #5 para el precedente de fallos de
      arranque del CH340, aunque de causa distinta).
- [ ] 6.2 **Verificación manual en hardware real**: flujo de provisioning
      completo contra un backend ThingsBoard real, confirmando que el
      dispositivo obtiene credenciales válidas y publica telemetría después.
- [ ] 6.3 **Verificación manual en hardware real**: comparar el payload de
      telemetría recibido en ThingsBoard antes y después de la migración de
      `sendTelemetryJson`, confirmando que no faltan campos.

## 7. Cierre

- [ ] 7.1 `openspec archive hmi-thingsboard-sdk-bump` una vez implementado y
      verificado en hardware real (o cerrado sin implementar, si la tarea 1.1
      determinó que motherBoard descartó la subida).
