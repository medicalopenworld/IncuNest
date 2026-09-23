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

## 2. Display_HMI — informe de depuración y `mailto:`

Commit: `feat(hmi): informe de depuracion y peticion de soporte via ThingsBoard`
(la vía ThingsBoard se retiró después en
`refactor(hmi): contacto con soporte solo por QR mailto`).

- [x] 2.1 Crear `src/modules/support/support_report.h/.cpp` con
      `support_report_build(char*, size_t)` (texto ASCII `clave=valor`,
      ≤ 400 B, orden del design.md), `support_report_subject(char*, size_t)`
      (`IncuNest SN %04d - Solicitud de soporte`) y
      `support_report_build_mailto(char*, size_t, bool withReport)`
      (percent-encoding RFC 3986 del asunto y del cuerpo).
- [x] 2.2 ~~Petición pendiente UI → tarea WiFi/OTA y publicación en
      ThingsBoard~~ Implementado y retirado: por decisión de producto el
      contacto es solo el QR `mailto:`. `Wifi_OTA.cpp` vuelve a estar
      intacto.
- [x] 2.3 `pio run -e main` en verde.

## 3. Display_HMI — menú de ayuda, vídeo tutorial y contacto

Commit: `feat(hmi): menu de ayuda, tutorial guiado y boton en el heading`
(fases 3, 4 y 5 juntas: el menú arranca el tutorial, el tutorial resalta el
botón del heading y el botón abre el menú, así que no hay orden en que las
tres compilen por separado sin stubs).

- [x] 3.1 Crear `include/ui/HelpDialog.h` + `src/ui/HelpDialog.cpp` con el
      patrón de `TimeDialog.cpp` (overlay reutilizado, `s_closeBtn` hijo de la
      tarjeta, `buildContent()` por vista, `TXT(es,en,fr)`, `makeBtn`). API:
      `HelpDialog_Init(lv_obj_t *parent)`, `HelpDialog_Open()`,
      `HelpDialog_IsOpen()`, `HelpDialog_Poll()` (sin `_Close()` público: no
      tenía llamador; se cierra desde dentro o por alarma crítica).
- [x] 3.2 Vista MENÚ: tres botones grandes (TUTORIAL GUIADO / VIDEO TUTORIAL /
      CONTACTAR SOPORTE) con símbolo LVGL y subtítulo de una línea. El primero
      cierra el diálogo y llama a `HelpTour_Start()`.
- [x] 3.3 Vista VÍDEO: `lv_qrcode` (≥ 300 px) con `SUPPORT_TUTORIAL_URL`, la
      URL en texto debajo, instrucción de escaneo y botón VOLVER.
- [x] 3.4 Vista CONTACTO: QR `mailto:` de 340 px (destinatario `SUPPORT_EMAIL`,
      asunto con el SN, informe en el cuerpo), instrucción de escaneo, texto
      con destinatario y asunto, botón SIN INFORME / CON INFORME (regenera el
      QR sin o con el informe) y VOLVER. Si `lv_qrcode_update()` falla por
      tamaño con informe, degradar a sin informe y avisarlo en pantalla.
      (La primera versión tenía formulario con teclado y envío por
      ThingsBoard; retirados en el `refactor(hmi)` de simplificación.)
- [x] 3.5 `HelpDialog_Poll()`: cierra ante cualquier alarma activa o enlace
      perdido y tras `HELP_IDLE_TIMEOUT_MS` sin tocar.
- [x] 3.6 `pio run -e main` en verde.

## 4. Display_HMI — tutorial guiado

Commit: el mismo de la fase 3.

- [x] 4.1 Crear `include/ui/HelpTour.h` + `src/ui/HelpTour.cpp`: overlay en
      `lv_layer_top()` (transparente, intercepta toques) con cuatro sombras
      negras al 50 % alrededor del recuadro (el control queda sin atenuar),
      marco ámbar con halo sobre `lv_obj_get_coords()` del control (se
      probó una flecha del bocadillo al marco y se quitó: no quedaba bien),
      bocadillo con texto
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
- [ ] 6.5 Contacto: el QR `mailto:` se lee con un móvil (iOS y Android) y
      abre el correo con destinatario `SUPPORT_EMAIL`, asunto
      `IncuNest SN <serie> - Solicitud de soporte` y el informe en el cuerpo;
      SIN INFORME reduce el QR y CON INFORME lo devuelve.
- [ ] 6.6 Contacto sin WiFi: idéntico al 6.5 (el equipo no envía nada); el
      informe muestra `wifi=0 tb=0`.
- [ ] 6.7 Tutorial: recorre todos los pasos, salta humedad si está
      deshabilitada, entra en Ajustes y vuelve a Main al salir; ANTERIOR
      funciona; no se acciona ningún control durante el recorrido.
- [ ] 6.8 Auto-bloqueo: con el menú o el tutorial abiertos, 30 s sin tocar no
      llevan a la pantalla de bloqueo; a los 3 min sin tocar la ayuda se
      cierra sola y 20 s después la pantalla se bloquea; al cerrarlos a mano
      el temporizador vuelve a contar desde cero.
- [ ] 6.9 Cualquier alarma (prueba de alarmas de la motherBoard, incluida una
      de prioridad BAJA) o desconectar la placa cierra menú y tutorial y deja
      la pantalla principal. Con el banner "SIN ENLACE" visible, comprobar
      que el tutorial, si se abre antes de que salte, nunca lo tapa.
- [ ] 6.10 Cambiar de idioma y reabrir: menú, vistas y tutorial en el idioma
      nuevo.

## 7. Documentación y servidor

Commit: `docs: update for hmi-boton-ayuda`.

- [x] 7.1 `docs/hmi.md`: sección del heading (nuevo `?`) y sección "Ayuda"
      (tutorial, vídeo, contacto, vías de envío).
- [x] 7.2 ~~`docs/thingsboard_dashboards.md`: apartado "Peticiones de
      soporte"~~ Escrito y retirado con la vía ThingsBoard: el contacto no
      toca el servidor.
- [x] 7.3 `Display_HMI/README.md`: mención a `SUPPORT_EMAIL` /
      `SUPPORT_TUTORIAL_URL` en la sección de credenciales.
- [x] 7.4 ADR `docs/adr/0001-contacto-soporte-via-thingsboard-y-mailto.md`
      (decisión 1 del design.md) a partir de `docs/adr/0000-template.md`.
