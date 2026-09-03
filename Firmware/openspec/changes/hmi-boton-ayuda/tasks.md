Todo el cambio es de **Display_HMI**. No hay entorno de test en esta placa:
cada fase se verifica con `pio run -e main` (compilación) y, donde se indica,
con prueba **manual** en el CrowPanel real. Ningún checkbox de esta lista
reclama cobertura automatizada.

## 1. Display_HMI — configuración de soporte

Commit: `feat(hmi): defaults de SUPPORT_EMAIL y SUPPORT_TUTORIAL_URL`.

- [x] 1.1 Añadir en `include/protocol/Credentials_public.h`, fuera del bloque
      `#if __has_include`, `SUPPORT_EMAIL` (`support@medicalopenworld.org`)
      y `SUPPORT_TUTORIAL_URL` con guardas `#ifndef`, para que un
      `Credentials.h` local pueda redefinirlos. Comentar que no son secretos:
      viven ahí porque es el fichero de configuración de despliegue.

## 2. Display_HMI — informe de depuración y petición de soporte

Commit: `feat(hmi): informe de depuracion y peticion de soporte via ThingsBoard`.

- [x] 2.1 Crear `src/modules/support/support_report.h/.cpp` con
      `support_report_build(char*, size_t)` (texto ASCII `clave=valor`,
      ≤ 400 B, orden del design.md), `support_report_subject(char*, size_t)`
      (`IncuNest SN %04d - Solicitud de soporte`) y
      `support_report_build_mailto(char*, size_t, const char *msg, bool
      withReport)` (percent-encoding RFC 3986 de asunto y cuerpo).
- [x] 2.2 Estado de la petición en el mismo módulo: `SupportRequest_Submit(const
      char *msg)` (toma la instantánea del informe y copia bajo `portMUX`,
      marca `PENDING`), `SupportRequest_GetState()`
      (`IDLE/PENDING/SENT/FAILED`), `SupportRequest_Reset()`, y el par
      `SupportRequest_TakePending()` / `SupportRequest_SetResult()` para la
      tarea WiFi/OTA. El módulo no conoce ThingsBoard: el JSON y la
      publicación viven en `Wifi_OTA.cpp` (`supportRequestService()`), que es
      quien tiene `tb_wifi`.
- [x] 2.3 Llamar a `supportRequestService()` desde `WIFI_TB_OTA()`
      (`Wifi_OTA.cpp`) en la rama `tb_wifi.connected()`, antes de la
      telemetría periódica. Sin tocar el intervalo de esa telemetría.
- [x] 2.4 `pio run -e main` en verde.

## 3. Display_HMI — menú de ayuda, vídeo tutorial y contacto

Commit: `feat(hmi): menu de ayuda, tutorial guiado y boton en el heading`
(fases 3, 4 y 5 juntas: el menú arranca el tutorial, el tutorial resalta el
botón del heading y el botón abre el menú, así que no hay orden en que las
tres compilen por separado sin stubs).

- [x] 3.1 Crear `include/ui/HelpDialog.h` + `src/ui/HelpDialog.cpp` con el
      patrón de `TimeDialog.cpp` (overlay reutilizado, `s_closeBtn` hijo de la
      tarjeta, `buildContent()` por vista, `TXT(es,en,fr)`, `makeBtn`). API:
      `HelpDialog_Init(lv_obj_t *parent)`, `HelpDialog_Open()`,
      `HelpDialog_Close()`, `HelpDialog_IsOpen()`, `HelpDialog_Poll()`.
- [x] 3.2 Vista MENÚ: tres botones grandes (TUTORIAL GUIADO / VIDEO TUTORIAL /
      CONTACTAR SOPORTE) con símbolo LVGL y subtítulo de una línea. El primero
      cierra el diálogo y llama a `HelpTour_Start()`.
- [x] 3.3 Vista VÍDEO: `lv_qrcode` (≥ 300 px) con `SUPPORT_TUTORIAL_URL`, la
      URL en texto debajo, instrucción de escaneo y botón VOLVER.
- [x] 3.4 Vista CONTACTO: `lv_textarea` (una línea, `max_length` 160) +
      `lv_btnmatrix` de letras y dígitos en un solo mapa (sin `123`/`ABC`:
      cinco filas caben en 250 px y se ahorra el cambio de modo), asunto
      mostrado como texto fijo, botones VOLVER / ENVIAR / QR MOVIL. ENVIAR:
      si `!WIFIIsConnectedToServer()` va a la vista QR con el aviso "Sin
      conexion con el servidor"; si hay servidor, `SupportRequest_Submit()` y
      etiqueta "Enviando...".
- [x] 3.5 Vista RESULTADO/QR: QR `mailto:` (destinatario + asunto + mensaje +
      informe), texto del estado del envío desde el equipo (registrada /
      fallida / sin conexión), botón SIN INFORME (regenera el QR sin informe
      si el móvil no lo lee) y CERRAR. Si `lv_qrcode_update()` falla por
      tamaño, degradar automáticamente (sin mensaje → sin informe) y
      avisarlo en pantalla.
- [x] 3.6 `HelpDialog_Poll()`: consume `SupportRequest_GetState()` (timeout
      `SUPPORT_SEND_TIMEOUT_MS` = 15 s → FAILED) y cierra con
      `UI_IsCriticalAlarmActive()`.
- [x] 3.7 `pio run -e main` en verde.

## 4. Display_HMI — tutorial guiado

Commit: el mismo de la fase 3.

- [x] 4.1 Crear `include/ui/HelpTour.h` + `src/ui/HelpTour.cpp`: overlay en
      `lv_layer_top()` (fondo negro 45 %, intercepta toques), marco ámbar
      sobre `lv_obj_get_coords()` del control, bocadillo con texto
      (`montserrat_18`, ancho 520) y botones ANTERIOR / SIGUIENTE / SALIR
      colocado en la mitad opuesta al control. API: `HelpTour_Init()`,
      `HelpTour_Start()`, `HelpTour_Stop()`, `HelpTour_IsOpen()`,
      `HelpTour_Poll()`.
- [x] 4.2 Tabla estática de pasos `{ &ui_X, &ui_ScreenY, es, en, fr }` con
      19 pasos: bienvenida (sin control), botón de ayuda, reloj, conectividad, candado,
      Bebés, alarmas (botón + check "todo OK"), Ajustes, contenedor de
      temperatura (AIR/SKIN, flechas, toggle), humedad, fototerapia, y en
      Ajustes las filas Info, WiFi, Idiomas y Modos; paso final. Textos
      ASCII sin acentos en ES/EN/FR.
- [x] 4.3 Navegación entre pantallas con `lv_scr_load()` +
      `lv_obj_update_layout()`; saltar pasos con `!lv_obj_is_visible()`;
      volver a `ui_ScreenMain` al salir o terminar.
- [x] 4.4 `HelpTour_Poll()`: cerrar con `UI_IsCriticalAlarmActive()`.
- [x] 4.5 `pio run -e main` en verde.

## 5. Display_HMI — botón en el heading y cableado en UITask

Commit: el mismo de la fase 3.

- [x] 5.1 `ElementsCreation.cpp`: nuevo `HEADING_SLOT0_HELP 20`, mover
      `HEADING_SLOT1_CLOCK` a `(173 - 400)` y `HEADING_SLOT2_CONN` a `282`;
      actualizar el comentario de reparto. Crear `ui_HelpButton` (lv_btn
      44×44, radius 22, bg `0x0075EE`) con `ui_HelpButtonLabel` (`?`,
      `montserrat_28`, blanco) en `ui_ScreenMain`; declarar en
      `ElementsCreation.h`; wrapper `ui_event_HelpButton` → `HelpButton_cb`.
      Sin `ext_click_area` (el hueco al reloj son 19 px).
- [x] 5.2 `UITask.cpp`: `HelpButton_cb` → `HelpDialog_Open()`;
      `HelpDialog_Init(ui_ScreenMain)` y `HelpTour_Init()` junto a
      `TimeDialog_Init`; `HelpDialog_Poll()` y `HelpTour_Poll()` junto a
      `TimeDialog_Poll()`; exención de auto-bloqueo en `inactivity_timer_cb`
      cuando `HelpDialog_IsOpen() || HelpTour_IsOpen()`.
- [x] 5.3 `pio run -e main` en verde. Flash 2 519 564 B frente a 2 454 400 B
      de referencia: +65 164 B (78,0 % → 80,1 %). RAM estática 126 556 B
      frente a 123 628 B: +2 928 B (buffers del mailto, la petición
      pendiente y sus copias en la tarea WiFi).

## 6. Verificación manual en banco (CrowPanel real) — **manual**

Sin commit propio; el resultado se anota en el commit `test: verify
hmi-boton-ayuda` (checklist rellena) o, si no hay hardware en ese momento,
queda documentado como pendiente en ese mismo commit.

- [ ] 6.1 El `?` aparece a la izquierda del reloj; reloj y conectividad
      siguen legibles; el candado responde en toda su zona táctil y el `?`
      no se dispara al tocar el borde del reloj.
- [ ] 6.2 En `ui_ScreenLock` el reloj y la conectividad están en la misma x
      que en Main (no saltan al bloquear) y no hay `?`.
- [ ] 6.3 Menú: los tres botones abren su vista; X cierra desde cualquiera.
- [ ] 6.4 Vídeo: el QR se lee con un móvil y abre `SUPPORT_TUTORIAL_URL`.
- [ ] 6.5 Contacto sin servidor: ENVIAR lleva al QR con "Sin conexion"; el QR
      `mailto:` abre el correo del móvil con destinatario, asunto con el SN y
      cuerpo con mensaje + informe.
- [ ] 6.6 Contacto con servidor: ENVIAR muestra "Enviando..." y después
      "Peticion registrada"; la telemetría aparece en el dispositivo de
      ThingsBoard con las cuatro claves.
- [ ] 6.7 Tutorial: recorre todos los pasos, salta humedad si está
      deshabilitada, entra en Ajustes y vuelve a Main al salir; ANTERIOR
      funciona; no se acciona ningún control durante el recorrido.
- [ ] 6.8 Auto-bloqueo: con el menú o el tutorial abiertos, 30 s sin tocar no
      llevan a la pantalla de bloqueo; al cerrarlos el temporizador vuelve a
      contar.
- [ ] 6.9 Alarma crítica (prueba de alarmas de la motherBoard) cierra menú y
      tutorial y deja la pantalla principal.
- [ ] 6.10 Cambiar de idioma y reabrir: menú, vistas y tutorial en el idioma
      nuevo.

## 7. Documentación y servidor

Commit: `docs: update for hmi-boton-ayuda`.

- [x] 7.1 `docs/hmi.md`: sección del heading (nuevo `?`) y sección "Ayuda"
      (tutorial, vídeo, contacto, vías de envío).
- [x] 7.2 `docs/thingsboard_dashboards.md`: apartado "Peticiones de soporte"
      con las cuatro claves de telemetría y la regla recomendada (filtro
      `support_request` → *to email* con `support_to` → *send email*).
- [x] 7.3 `Display_HMI/README.md`: mención a `SUPPORT_EMAIL` /
      `SUPPORT_TUTORIAL_URL` en la sección de credenciales.
- [x] 7.4 ADR `docs/adr/0001-contacto-soporte-via-thingsboard-y-mailto.md`
      (decisión 1 del design.md) a partir de `docs/adr/0000-template.md`.
