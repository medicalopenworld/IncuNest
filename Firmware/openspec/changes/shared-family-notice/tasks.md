## 1. shared and motherBoard (TDD en `[env:native]`)

- [ ] 1.1 Tests en rojo `test_family_proto`: build con y sin URL, buffer pequeño, ida y vuelta, flag/campos malos, URL inválida (http, coma/espacio/control, 97 vs 96, solo `https://`).
- [ ] 1.2 `shared/family_proto.h/.cpp` (`family_url_valid`, `family_proto_build`, `family_proto_parse`); comprobar que `pre_native.py` compila `shared/src`. `PROTOCOL.md`: sección «CTRL,FAMILY (Aviso «Coge mi mano» para la familia)» y nota de la versión v2.5.0 (no hay tabla de mensajes CTRL).
- [ ] 1.3 Tests en rojo `test_family_notice`: reloj de red y plazo, tope de 30 min, pasado, ventana fija de 5 min, petición de reconexión, duplicado, extensión, URL mala, alta, solo al cambiar, vencimiento, desbordamiento de `millis()`.
- [ ] 1.4 `modules/family/family_notice.*` y `+<modules/family/family_notice.cpp>` en el `build_src_filter` de `[env:native]`.
- [ ] 1.5 `family_tb.h`, `family_state.*`, cableado en `GPRS.cpp`, `Wifi_OTA.cpp` y `CommTask.cpp` (alta, `UI_READY`/`REQ,STATE`, envío solo al cambiar); motherBoard `18.68.0`.
- [ ] 1.6 `pio test -e native` en verde y `pio run -e IncuNest_V18 -e IncuNest_V19` compila.

## 2. Display_HMI

- [ ] 2.1 Tests en rojo `test_family_view` (los 9 casos) y `modules/family/family_view.*` con su `+<...>` en `[env:native]`.
- [ ] 2.2 Rama `CTRL,FAMILY,` en `CommTask.cpp` (una línea mala se descarta y queda la última válida), `ui/FamilyNotice.*`, `FamilyNotice_Init` en `ElementsCreation.cpp`, `FamilyNotice_Poll` en `UITask.cpp`.
- [ ] 2.3 Textos en `i18n_strings.def` (es/en/fr/pt, solo ASCII), tarjeta "Familia" en `HelpDialog.cpp`; Display_HMI `4.13.0`.
- [ ] 2.4 `pio run -e main -e hmi_screenshots` compila y `pio test -e native` en verde.

## 3. Prueba en banco (manual, sin entorno de test)

- [ ] 3.1 **Manual en hardware**: aviso solo texto con `hand_hold_until` a +2 min, y con `family_qr_url` el QR que un móvil lee; ayuda con cuatro tarjetas.
- [ ] 3.2 **Manual en hardware**: con el aviso visible, una alarma, el pulsioxímetro y el deslizador lo ocultan (y vuelve al irse la alarma); no se oye nada ni se roba el toque.
- [ ] 3.3 **Manual en hardware**: reinicio con hora de red (vuelve) y con hora manual (no vuelve); alta del bebé borra QR y aviso; pantalla vieja con placa nueva y al revés, sin errores.
- [ ] 3.4 **Manual en hardware**: con URL y sin aviso, la ayuda ofrece «Familia»; la vista Familia sigue abierta cuando acaba un aviso; borrar o vaciar `family_qr_url` en ThingsBoard y reconectar quita el QR; tras una caída del enlace el display manda `HMI,REQ,STATE` (máx. 3) y el aviso vuelve si sigue vigente.
- [ ] 3.5 **Manual en hardware**: el QR se lee y no lo tapa el icono ámbar de audio en pausa ni su cuenta atrás (abajo a la derecha, capa superior) con una alarma silenciada; los subtítulos FR/PT de las cuatro tarjetas de la ayuda caben.

## 4. Cierre

- [ ] 4.1 Archivar este cambio y abrir el PR a `dev` con el resultado del banco.
