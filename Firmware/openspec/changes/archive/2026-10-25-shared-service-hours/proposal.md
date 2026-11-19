## Why

Placas afectadas: **motherBoard**, **Display_HMI** y `shared/` (el codec de la
línea nueva vive ahí para que las dos placas compartan una sola definición).

La motherBoard ya acumula las horas de uso de cada actuador y las persiste en
NVS (`NS_RT`, claves `heater_t`, `fan_t`, `hum_t`, `photo_t`, `ctrl_time`,
`standby`; `timeTrackHandler()` en `src/system/updateData.cpp`) y las publica a
ThingsBoard. Pero nadie las interpreta: no hay un umbral por actuador, el
personal de la unidad no ve nunca que el ventilador o el nebulizador se acercan
al fin de su vida útil, y no queda constancia de cuándo se hizo el último
servicio. El plan de mantenimiento del fabricante habla en horas de uso; el
equipo tiene el dato y no lo usa.

Un recordatorio de servicio es **información**, no una condición de alarma de
IEC 60601-1-8: no hay peligro inmediato para el paciente ni acción urgente. Por
eso no se añade ningún `AlarmId` (no cambia `alarm_ids.h`) y no suena el
zumbador. Se muestra en la pantalla de mantenimiento del HMI y como aviso suave
al desbloquear, con el mismo motor de avisos que el recordatorio de limpieza,
cediendo igual ante alarmas, wizard y otros diálogos.

## What Changes

- **Umbrales de servicio por actuador**, compilados en
  `motherBoard/include/config/service_thresholds.h`. Valores provisionales
  (calefactor 5 000 h, ventilador 10 000 h, humidificador 2 000 h, fototerapia
  10 000 h) hasta tener el plan de mantenimiento del fabricante. Sin
  configuración remota en este cambio.
- **Horas desde servicio** = horas de vida acumuladas − "base" guardada en NVS
  en el último servicio registrado. Los contadores de vida útil NO se tocan: se
  siguen publicando tal cual.
- **Línea nueva `CTRL,SERVICE,n{,id,horasDesdeServicio,umbralHoras}×n`** (MB →
  HMI), enviada al arrancar el enlace (`HMI,UI_READY`), bajo petición
  (`HMI,SERVICE_REQ`), al registrar un servicio y cada hora. No se tocan los
  campos de `CTRL,STATE` (la línea está al límite y su campo 24 está reservado).
- **Comando `HMI,SERVICE_DONE,<id>`** (HMI → MB): la placa guarda en NVS las
  horas acumuladas de ese actuador como nueva base y el epoch del servicio, y
  contesta con un `CTRL,SERVICE` actualizado.
- **HMI**: una vista de servicio dentro del diálogo de mantenimiento existente
  (`MaintenanceDialog`), alcanzable desde el botón SERVICIO del diálogo (que se
  abre desde Ajustes > Mantenimiento) y abierta sola al desbloquear cuando algún
  actuador ha vencido (con "MAS TARDE" de 24 h). Registrar "servicio hecho"
  exige dos pasos (pulsar HECHO y confirmar), no se permite en formación.
- **Telemetría**: `svc_heater_h`, `svc_fan_h`, `svc_hum_h`, `svc_photo_h` (horas
  desde servicio) en el grupo DIAG de ThingsBoard.
- Textos en las 4 lenguas del catálogo `i18n_strings.def`.

## Fuera de alcance

- Configuración remota de umbrales (RPC/atributos compartidos).
- Control de acceso real al registro de servicio: el HMI no tiene PIN en Ajustes
  > Mantenimiento hoy. La confirmación en dos pasos es la única salvaguarda y la
  falta de PIN queda anotada como deuda.
- Alarma, sonido o bloqueo del equipo por servicio vencido.
- Contadores de control y standby (no tienen un componente que sustituir).

## Impact

- `shared/include/service_hours_proto.h`, `shared/src/service_hours_proto.cpp`
  (nuevos): ids de actuador y codec de las tres líneas.
- motherBoard: `modules/service/service_calc.{h,cpp}` (lógica pura, tests en
  host), `modules/service/service_hours.{h,cpp}` (NVS + `in3`), `CommTask.cpp`,
  `telemetry_keys.h`, `transport_policy.h`/`GPRS.cpp`/`Wifi_OTA.cpp` (+4 claves
  DIAG), `preferences_keys.h`, `EEPROM.cpp`. MB sin PSRAM: todo estático.
- Display_HMI: `modules/service/service_state.{h,cpp}` (lógica pura, tests en
  host), `CommTask.cpp`, `MaintenanceDialog.cpp`, `i18n_strings.def`.
- `Firmware/PROTOCOL.md`, `Firmware/docs/hmi.md`, `Firmware/docs/transport_matrix.md`
  y `thingsboard_dashboards.md`.
- Sin cambio de versión de firmware (lo decide el release).
