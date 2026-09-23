# Limpieza de código legacy — motherBoard (defines, ramas HW_NUM muertas, funciones sin llamador)

**Fecha:** 2026-07-10
**Status:** Propuesta — pendiente de ejecución.

## Contexto

La limpieza de la UI on-board (`docs/superpowers/specs/2026-07-08-legacy-onboard-ui-removal-proposal.md`)
borró `motherBoard/src/legacy/` (~3244 líneas), pero dejó remanentes fuera de esa carpeta: defines de
`include/config/` que solo tenían sentido para esa UI, funciones sin llamador, y — el hallazgo más
grande de este inventario — ramas de compilación condicional `#if (HW_NUM<16)` repartidas por 5
ficheros que ya no se compilan nunca.

**Confirmado en `platformio.ini`**: los únicos entornos reales son `IncuNest_V17` (línea 38,
`HW_NUM=17`) y `IncuNest_V16` (línea 61, `HW_NUM=16`), más `native` (host, sin `HW_NUM`, solo
`modules/control/`). Ninguna revisión de hardware por debajo de 16 se fabrica ni se compila hoy —
toda rama `#if (HW_NUM<16)` es código estructuralmente muerto, no solo un define suelto.

Este cambio es solo `motherBoard`. No toca `Display_HMI` ni `shared/`.

## Mapa por categoría

### Fase 0 — bloques comentados muertos

| Fichero | Línea(s) | Qué |
|---|---|---|
| `src/main.cpp` | 219-226 | Bloque de reintento de tarea (`xTaskCreatePinnedToCore(GPRS_Task…)`) abandonado + log suelto |
| `src/main.cpp` | 85 | `// Adafruit_ILI9341 tft = ...` — driver alternativo superado por `TFT_eSPI` |
| `src/tasks/Wifi_OTA.cpp` | 1008-1013 | Log de debug periódico deshabilitado (`ESP_LOGI` "alive") |
| `src/tasks/GPRS.cpp` | 488 | `// updateRequestSent = tb.Subscribe_Firmware_Update(callback);` |
| `src/tasks/GPRS.cpp` | 812-813 | `StaticJsonDocument`/`JsonObject` comentado, huérfano |
| `src/system/initHardware.cpp` | 634-637, 1109, 1124 | `digitalWrite(FAN,...)`/`delay()` comentados dentro de init TFT legacy |
| `src/system/security.cpp` | 284, 866-868 | Fragmento condicional muerto (`// {`, `// shutBuzzer();`) |
| `src/drivers/IncuNest_humidifier.h` | 61 | `// MAM_IncuNest_Humidifier();` |

### Fase 1 — funciones y globals sin llamador (no dependen de HW_NUM)

| Símbolo | Definición | Prototipo (`main.h`) |
|---|---|---|
| `alarmPendingToDisplay()` | `security.cpp:492` | `main.h:466` |
| `clearDisplayedAlarm()` | `security.cpp:502` | `main.h:468` |
| `clearAlarmPendingToClear()` | `security.cpp:504` | `main.h:469` |
| `alarmPendingToClear()` | `security.cpp:506` | `main.h:467` |
| `buzzerConstantTone()` | `Buzzer.cpp:47` | `main.h:444` |
| `wifiDisable()` | `Wifi_OTA.cpp:350` | `main.h:477` |

Globals huérfanos (solo `extern`, nunca leídos): `ypos`, `print_text`, `pos_text[8]`,
`initialSensorPosition` — definición en `main.cpp:~161`, `extern` en `initHardware.cpp:100-103`,
`ISR.cpp:91-94`, `security.cpp:109-112`. Son estado de dibujo de la UI on-board ya eliminada.

### Fase 2 — ramas `#if (HW_NUM<16)` muertas (estructural)

| Fichero | Línea(s) | Qué |
|---|---|---|
| `include/config/board.h` | 119-313 | Bloques de pines/config para HW_NUM ≤8/9/13/14/15: `TOUCH_IRQ`, `TOUCH_CS`, `TFT_RST`, `TFT_CS_EXP`, `GPIO_EXP_0..15`, `UNUSED_GPIO_EXP0..3`, `SD_CS`, `GPRS_EN`, `HUMIDIFIER_CTL`, `DISPLAY_CONTROLLER_IC`, `AFE4490_ADC_READY` |
| `include/config/board.h` | 36-42, 44-47 | `DISPLAY_SPI_CLK` (los 3 branches resuelven al mismo valor — colapsar o borrar), `ANALOG_TO_AMP_FACTOR`/`CURRENT_MEASURES_AMOUNT` del branch `HW_NUM<=8` |
| `include/main.h` | 427-428, 498, 513 | Prototipos colgantes `UI_mainMenu()`, `userInterfaceHandler(int)`, `drawHardwareErrorMessage()`, `basictemperatureControl()` — sin definición viva, solo referenciados dentro de ramas muertas |
| `src/system/initHardware.cpp` | 267-330, 605-640, 1201-1210, 1303-1330 | Init de hardware condicionado a `HW_NUM<13/15`, incluye la única llamada a `drawHardwareErrorMessage()` (línea 1304, dentro del `#if` muerto en 1303) |
| `src/modules/comm/CommTask.cpp` | 16-130 | Rama de protocolo/comm condicionada a `HW_NUM<16` |
| `src/modules/sensors/sensors_module.cpp` | 196-200 | Rama de lectura de sensores condicionada a `HW_NUM<16` |

Al cerrar esta fase, `testDisplay()` queda sin llamador real (vivía solo para las revisiones
antiguas), y con él `BACKLIGHT_CONTROL`/`DIRECT_BACKLIGHT_CONTROL` pasan a muertos transitivamente
— se limpian en la fase 3.

### Fase 3 — headers de config huérfanos (depende de fase 2)

| Fichero | Qué |
|---|---|
| `include/config/ui_constants.h` | Fichero completo (~40 defines + 5 enums: `UI_PAGES`, `UI_EVENTS_ID`, `UI_EVENTS_ID_POS`, `MAIN_MENU_UI`, `SETTINGS_MENU_UI`, `CALIBRATION_MENU_UI`) — sin referencias fuera del propio fichero |
| `include/config/board.h` | `HUMIDIFIER_INTERFACE` (48-56), `AFE_LED_ALM` (105), `NTC_QTY` (326), `SDCard` (354), `BL_NORMAL`/`BL_POWERSAVE` (453-454), `SCREEN_BRIGHTNESS_FACTOR`/`BACKLIGHT_POWER_SAFE*` (470-482), `maxDACvalue` (402), `BACKLIGHT_CONTROL`/`DIRECT_BACKLIGHT_CONTROL` (transitivo, fase 2) |
| `include/config/task_config.h` | `BACKLIGHT_TASK_PRIORITY` (10), `UI_TASK_PRIORITY` (14), `UI_TASK_PERIOD_MS` (42), `BACKLIGHT_TASK_PERIOD_MS` (46), `CALIBRATION_TASK_PERIOD_MS` (49) |
| `include/main.h` | `UI_MENU_OLD` (123) |
| `include/config/telemetry_keys.h` | `DISPLAY_CURR_TEST_KEY` (18), `CALIBRATION_RAW_TEMPERATURE_RANGE_AIR_KEY`/`CALIBRATION_RAW_TEMPERATURE_LOW_AIR_KEY` (87-88) — confirmado sin referencias en todo `Firmware/`, incluida `Thingsboard/` |

## Fuera de alcance

`src/system/EEPROM.cpp:128-162` — `migrateFromEEPROM()` y sus ~30 constantes `OLD_*` **no están
muertos**: siguen siendo el fallback de migración para unidades que aún tengan el layout EEPROM
crudo antiguo (llamado desde `EEPROM.cpp:274`). Se mantienen fuera de este cambio hasta confirmar
que todas las unidades desplegadas ya migraron a NVS/Preferences.

## Decisiones ya tomadas

1. Cambio limitado a `motherBoard`; `Display_HMI` y `shared/` no se tocan aquí.
2. Las ramas `HW_NUM<16` se tratan como fase propia (fase 2) dentro de este mismo documento, no como
   un cambio separado, porque comparten motivo y se benefician de la misma tanda de verificación.
3. `EEPROM.cpp` (`OLD_*` + `migrateFromEEPROM()`) se mantiene: es código vivo, no legacy real.
4. Las claves de telemetría candidatas se verificaron contra todo el repo (incluida `Thingsboard/`)
   antes de marcarlas borrables.

## Fases de ejecución

| Fase | Qué | Riesgo | Bloqueada por |
|---|---|---|---|
| 0 | Bloques comentados muertos | Ninguno | — |
| 1 | Funciones/globals sin llamador (no HW_NUM) | Bajo | — |
| 2 | Ramas `#if (HW_NUM<16)` en board.h/main.h/initHardware.cpp/CommTask.cpp/sensors_module.cpp | Medio — toca inicialización de hardware y protocolo | Fases 0-1 (para que main.h quede consistente) |
| 3 | Headers de config huérfanos (`ui_constants.h`, resto de `board.h`, `task_config.h`, `main.h:UI_MENU_OLD`, 3 claves de `telemetry_keys.h`) | Bajo — solo defines, ya sin consumidores tras fase 2 | Fase 2 |

## Verificación

- Compilación: `pio run -e IncuNest_V16` y `pio run -e IncuNest_V17` (las dos placas reales) tras
  cada fase — deben seguir compilando sin warnings nuevos.
- Tests: `pio test -e native` — no debería verse afectado (solo cubre `modules/control/`), se
  confirma que sigue en verde.
- Manual en hardware, al cerrar la fase 2 (la más estructural): arranque limpio en placa real,
  actuación y alarmas vía `Display_HMI`, telemetría reportando a ThingsBoard con las claves
  esperadas.

## Siguiente paso

Sin bloqueos pendientes. Ejecutar fases 0 y 1 primero (sin riesgo), verificar, luego fase 2, y
cerrar con la fase 3.
