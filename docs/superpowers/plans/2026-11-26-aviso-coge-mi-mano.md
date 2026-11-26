# Aviso «Coge mi mano» y QR de familia en la IncuNest — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cuando alguien coge la mano del bebé (atributo compartido `hand_hold_until`), la pantalla de bloqueo de la IncuNest enseña un aviso pequeño, mudo y que desaparece solo, con un QR para la familia si ThingsBoard le manda la URL de la estancia. Además, la ayuda gana una tarjeta «Familia» con el QR grande.

**Architecture:** La placa base recibe dos atributos compartidos nuevos por una suscripción propia (no `runtime_config`), decide con un núcleo puro (`family_notice`) si el aviso está activo y hasta cuándo, y manda a la pantalla una línea UART nueva `CTRL,FAMILY,<0|1>,<url>` solo cuando cambia. La pantalla guarda lo último recibido y otro núcleo puro (`family_view`) decide si se pinta y cómo, según alarmas, enlace, pulsioxímetro y deslizador.

**Tech Stack:** ESP32 Arduino + PlatformIO, ThingsBoard Arduino SDK v0.13.0 (`THINGSBOARD_ENABLE_DYNAMIC`), ArduinoJson 6.21.5, LVGL 8.3 con `lv_qrcode`, Unity en `env:native`.

**Spec:** `C:\Users\Pablo\Documents\IncuTwin\incutwin-app\docs\superpowers\specs\2026-11-26-modo-familias-design.md` (§9 «La IncuNest»; repo `medicalopenworld/incutwin-app`, commit 49cbaf9 y siguiente).

**Rama / worktree:** `feat/aviso-coge-mi-mano` desde `origin/dev` 6be1c2d6, en `C:\Users\Pablo\Documents\IncuNest-coge-mi-mano`. Todos los comandos, desde `Firmware/motherBoard` o `Firmware/Display_HMI`.

## Global Constraints

- Claves de ThingsBoard (atributos compartidos de la IncuNest): `hand_hold_until` (número, epoch en **ms**, ya lo escribe ThingsBoard) y `family_qr_url` (texto; vacío o ausente = sin QR). La placa recibe la URL completa, no un código: el dominio lo decide el servidor sin tocar firmware.
- URL válida: empieza por `https://`, 9–96 caracteres, solo ASCII imprimible 0x21–0x7E y **sin coma** (viaja en una línea CSV). Inválida = como vacía: sin QR.
- Duración del aviso: con hora de red (`PROTO_TIME_SOURCE_NTP` o `PROTO_TIME_SOURCE_NITZ`) hasta `hand_hold_until`, **máximo 30 min**; con cualquier otra fuente o sin hora, **5 min** desde que llega el valor, y nunca por una respuesta a la petición de reconexión.
- Un `hand_hold_until` igual o anterior al último aplicado se ignora siempre (ThingsBoard reenvía el último valor en cada conexión).
- `PROFILE_DISCHARGE` confirmado en la placa: se olvida la URL y se apaga el aviso en el acto.
- UART: `CTRL,FAMILY,<0|1>,<url>` (url puede ir vacía: `CTRL,FAMILY,1,`). Se manda **solo al cambiar** y al recibir `HMI,UI_READY` o `HMI,REQ,STATE`; nunca periódico (`known_issues.md` #2). Una línea mal formada se descarta entera.
- Pantalla: el aviso vive en `ui_ScreenLock`, franja inferior (la del pulsioxímetro, 792 × 117 px). Se pinta solo si: hay aviso, no hay ninguna alarma (`UI_IsAnyAlarmActive()`), el enlace con la placa está vivo (`!Display_IsBoardLinkLost()`), el pulsioxímetro no se ve (`ui_LockPulseOxFrame` oculto) y el deslizador de desbloqueo no se ve (`ui_UnlockCont` oculto). No modal, no se toca, no suena.
- Colores: fondo `0x054E92`, texto y QR blanco/negro sobre blanco. **Nada rojo, amarillo, naranja ni cian.**
- Textos en `include/ui/i18n_strings.def`, columnas es/en/fr/pt, **solo ASCII** (sin tildes, eñes ni cedillas).
- Versiones: motherBoard `18.67.0` → `18.68.0` (`Firmware/motherBoard/include/main.h`), Display_HMI `4.12.2` → `4.13.0` (`Firmware/Display_HMI/include/main.h`). Si `dev` sube antes, rebasar y subir sobre lo que haya.
- Commits como en el repo: `feat(mb): …`, `feat(hmi): …`, `feat(shared): …`, `docs(spec): …`, con `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Contrato con el servidor** (revisión final, 26-nov): para quitar el QR, el servidor escribe `family_qr_url` = `""`; **nunca borra el atributo** (el SDK 0.13 entrega el borrado en vivo como `{"deleted":[…]}`, que la placa no ve; solo se nota al reconectar, cuando la respuesta a la petición no trae la clave y la placa olvida la URL). La URL se mantiene en ≲ 40 caracteres para que el QR de 105 px de la pantalla de bloqueo salga a ≥ 3 px por módulo.

## Review Focus

1. **Reconexión que reenvía un `hand_hold_until` viejo**: no debe volver a salir el aviso. → test `test_duplicate_until_ignored` y `test_request_with_untrusted_clock_ignored` (Task 3).
2. **Hora puesta a mano** (fuente MANUAL o RTC): el epoch no es UTC; el aviso dura 5 min fijos y no se fía de `until`. → `test_untrusted_clock_uses_fixed_window` (Task 3).
3. **URL hostil o rota desde ThingsBoard** (coma, `http://`, espacios, 200 caracteres): sin QR y sin romper la línea UART. → `test_url_rejects_*` (Task 2) y `test_bad_url_clears_qr` (Task 3).
4. **Alarma o pérdida de enlace con el aviso en pantalla**: se esconde en el acto y vuelve si sigue vigente cuando se van. → `test_alarm_hides_then_returns` (Task 5).
5. **Pulsioxímetro puesto o deslizador visible**: no se pinta (no hay sitio sin tapar algo clínico). → `test_pulseox_hides`, `test_unlock_slider_hides` (Task 5).

---

### Task 1: Cambio de openspec `shared-family-notice`

Es la pieza del control de cambios del equipo: lo que se cambia, por qué, y el riesgo (que el aviso no pueda ocultar ni retrasar una alarma). Formato y idioma (inglés) como `Firmware/openspec/changes/shared-baby-sex/`.

**Files:**
- Create: `Firmware/openspec/changes/shared-family-notice/proposal.md`
- Create: `Firmware/openspec/changes/shared-family-notice/specs/family-notice/spec.md`
- Create: `Firmware/openspec/changes/shared-family-notice/tasks.md`

- [ ] **Step 1: Escribir `proposal.md`** con Why (IncuTwin: familias y socios; la madre al lado haciendo piel con piel; la enfermera nunca interrumpida), What Changes (las dos claves, `CTRL,FAMILY`, aviso en bloqueo, tarjeta Familia en ayuda; no rompe en ninguna dirección: placa vieja no manda la línea, pantalla vieja la descarta), Risk (no modal, sin sonido, sin colores IEC 60601-1-8, cede ante cualquier alarma, enlace caído, pulsioxímetro y deslizador; no toca control, alarmas ni telemetría), Capabilities (`family-notice`, nueva).
- [ ] **Step 2: Escribir `spec.md`** con un requisito por cada línea de Global Constraints que afecte al comportamiento (duración, duplicados, URL válida, alta, línea UART, condiciones de pintado, ayuda), cada uno con un escenario WHEN/THEN tomado de los tests de las tareas 2–7.
- [ ] **Step 3: `tasks.md`** con las tareas 2–8 de este plan como checklist.
- [ ] **Step 4: Validar**

Run: `openspec validate shared-family-notice` (desde `Firmware/`; si el CLI no está instalado, revisar a mano contra `shared-baby-sex`)
Expected: `Change 'shared-family-notice' is valid`

- [ ] **Step 5: Commit** — `docs(spec): propose shared-family-notice`

---

### Task 2: Protocolo compartido `CTRL,FAMILY`

**Files:**
- Create: `Firmware/shared/include/family_proto.h`
- Create: `Firmware/shared/src/family_proto.cpp`
- Test: `Firmware/motherBoard/test/test_family_proto/test_family_proto.cpp`
- Modify: `Firmware/PROTOCOL.md` (sección nueva «Family notice», tabla de mensajes y notas de la versión del protocolo)

**Interfaces:**
- Produces (C, sin Arduino ni I/O, mismo estilo que `service_hours_proto.h`):
  - `#define FAMILY_URL_MAX 96`
  - `struct FamilyMsg { bool notice; char url[FAMILY_URL_MAX + 1]; };`
  - `bool family_url_valid(const char *url);`
  - `int family_proto_build(char *buf, size_t len, const FamilyMsg *m);` → caracteres escritos con `\n`, 0 si no cabe o si `m->url` no es vacía ni válida.
  - `bool family_proto_parse(const char *line, FamilyMsg *out);` → línea sin `\n`; en fallo deja `out->notice = false` y `out->url[0] = '\0'`.

- [ ] **Step 1: Tests que fallan**

```cpp
void test_build_with_url(void) {
  FamilyMsg m = {true, "https://incutwin.org/f/7KQ2M9"};
  char buf[160];
  TEST_ASSERT_GREATER_THAN(0, family_proto_build(buf, sizeof buf, &m));
  TEST_ASSERT_EQUAL_STRING("CTRL,FAMILY,1,https://incutwin.org/f/7KQ2M9\n", buf);
}
void test_build_without_url(void) {
  FamilyMsg m = {false, ""};
  char buf[32];
  family_proto_build(buf, sizeof buf, &m);
  TEST_ASSERT_EQUAL_STRING("CTRL,FAMILY,0,\n", buf);
}
void test_build_too_small_returns_0(void);          // buf de 10 -> 0
void test_roundtrip(void);                          // build -> parse (sin '\n') -> mismos campos
void test_parse_rejects_bad_flag(void);             // "CTRL,FAMILY,2,", "CTRL,FAMILY,x,"
void test_parse_rejects_missing_field(void);        // "CTRL,FAMILY,1"
void test_parse_rejects_extra_field(void);          // "CTRL,FAMILY,1,https://a.org/f/1,x"
void test_url_rejects_http(void);                   // "http://a.org/f/1" -> false
void test_url_rejects_comma_space_ctrl(void);       // "https://a.org/f,1", "https://a.org/ f", "https://a.org/\t"
void test_url_rejects_too_long(void);               // 97 caracteres -> false; 96 -> true
void test_url_rejects_bare_scheme(void);            // "https://" -> false
```

- [ ] **Step 2: Comprobar que fallan**

Run: `pio test -e native -f test_family_proto`
Expected: FAIL (no existe `family_proto.h`)

- [ ] **Step 3: Implementar** `family_url_valid`, `family_proto_build` y `family_proto_parse` en `shared/src/family_proto.cpp`. `pre_native.py` ya compila `shared/src` para los dos `env:native`; comprobar que la recoge.
- [ ] **Step 4: Comprobar que pasan**

Run: `pio test -e native -f test_family_proto`
Expected: 11 tests, 0 failures

- [ ] **Step 5: `PROTOCOL.md`** — sección «Family notice» (formato, cuándo se manda, compatibilidad: nueva línea, una pantalla antigua la descarta) y la fila en la tabla de mensajes CTRL.
- [ ] **Step 6: Commit** — `feat(shared): protocolo CTRL,FAMILY para el aviso de Coge mi mano`

---

### Task 3: Núcleo de la placa base `family_notice`

**Files:**
- Create: `Firmware/motherBoard/src/modules/family/family_notice.h`
- Create: `Firmware/motherBoard/src/modules/family/family_notice.cpp`
- Modify: `Firmware/motherBoard/platformio.ini` (`[env:native]` `build_src_filter`: `+<modules/family/family_notice.cpp>`)
- Test: `Firmware/motherBoard/test/test_family_notice/test_family_notice.cpp`

**Interfaces:**
- Consumes: `FamilyMsg`, `family_url_valid` (Task 2).
- Produces:
  - `#define FAMILY_HOLD_MAX_S (30u * 60u)` y `#define FAMILY_HOLD_UNTRUSTED_S (5u * 60u)`
  - `struct FamilyNotice` (opaca para los demás; campos a elección: activo, plazo en `millis`, último `until` aplicado como `double`, URL, último `FamilyMsg` enviado).
  - `void family_notice_init(FamilyNotice *f);`
  - `void family_notice_on_hand_hold(FamilyNotice *f, double untilEpochMs, bool fromRequest, uint32_t nowEpochS, bool clockFromNetwork, uint32_t nowMs);`
  - `void family_notice_set_url(FamilyNotice *f, const char *url);` (NULL o inválida = vacía)
  - `void family_notice_on_discharge(FamilyNotice *f);`
  - `bool family_notice_take_msg(FamilyNotice *f, uint32_t nowMs, bool force, FamilyMsg *out);` → true si hay que mandar `*out`: el estado visible (aviso activo a `nowMs`, URL) difiere del último mandado, o `force`.
- Plazos con resta sin signo (`(int32_t)(deadline - nowMs) > 0`), para sobrevivir al desbordamiento de `millis()`.

- [ ] **Step 1: Tests que fallan** (`E = 1789000000u`, 2026-09-08, como `test_time_source`)

```cpp
void test_trusted_clock_shows_until(void) {
  // until = ahora + 120 s, hora de red: activo a los 119 s, apagado a los 121 s
  family_notice_on_hand_hold(&f, (E + 120) * 1000.0, false, E, true, 1000);
  TEST_ASSERT_TRUE(take(&f, 1000 + 119000).notice);
  TEST_ASSERT_FALSE(take(&f, 1000 + 121000).notice);
}
void test_trusted_clock_caps_at_30_min(void);          // until = ahora + 2 h -> apagado a 1800 s + 1
void test_past_until_ignored(void);                     // until = ahora - 1 s -> sin aviso
void test_untrusted_clock_uses_fixed_window(void);      // clockFromNetwork=false, until=ahora+2 h -> 300 s exactos
void test_request_with_untrusted_clock_ignored(void);   // fromRequest=true, hora no fiable -> sin aviso
void test_request_with_trusted_clock_shows(void);       // fromRequest=true, hora de red, until futuro -> aviso
void test_duplicate_until_ignored(void);                // mismo until tras expirar -> sigue apagado
void test_newer_until_extends(void);                    // until mayor durante el aviso -> nuevo plazo
void test_bad_url_clears_qr(void);                      // set_url("https://a.org/f/1") y luego set_url("http://x") -> url ""
void test_discharge_clears_all(void);                   // aviso activo + url -> on_discharge -> notice false, url ""
void test_take_msg_only_on_change(void);                // 1ª llamada true; 2ª igual false; force -> true
void test_take_msg_on_expiry(void);                     // al vencer el plazo, true con notice=false
void test_millis_wraparound(void);                      // nowMs = 0xFFFFF000, ventana de 5 min sigue activa tras el desbordamiento
```

`take()` es un ayudante del test que llama a `family_notice_take_msg(&f, now, true, &m)` y devuelve `m`.

- [ ] **Step 2: Comprobar que fallan**

Run: `pio test -e native -f test_family_notice`
Expected: FAIL (no existe `family_notice.h`)

- [ ] **Step 3: Implementar** las cinco funciones en `family_notice.cpp`. `on_hand_hold` apunta el `until` como último aplicado aunque lo ignore por hora no fiable, para que un duplicado posterior tampoco cuente.
- [ ] **Step 4: Comprobar que pasan**

Run: `pio test -e native -f test_family_notice`
Expected: 13 tests, 0 failures

- [ ] **Step 5: Commit** — `feat(mb): núcleo del aviso de Coge mi mano (family_notice)`

---

### Task 4: Placa base — ThingsBoard, alta y envío a la pantalla

**Files:**
- Create: `Firmware/motherBoard/src/modules/family/family_tb.h` (plantilla, mismo patrón que `modules/config/runtime_config_tb.h`)
- Create: `Firmware/motherBoard/src/modules/family/family_state.h` / `.cpp` (envoltorio con `portMUX_TYPE` sobre un `FamilyNotice` global; toca Arduino y la hora, no entra en `env:native`)
- Modify: `Firmware/motherBoard/src/tasks/GPRS.cpp` (junto a `runtime_config_tb_connected`, ~l. 1457)
- Modify: `Firmware/motherBoard/src/tasks/Wifi_OTA.cpp` (junto a `runtime_config_tb_connected`, ~l. 2053)
- Modify: `Firmware/motherBoard/src/tasks/CommTask.cpp` (`BABY_MSG_DISCHARGE` ~l. 243; `HMI,UI_READY` / `HMI,REQ,STATE` ~l. 735; bucle junto a `--- SERVICE` ~l. 1606)
- Modify: `Firmware/motherBoard/include/main.h` (`FWversion "18.68.0"`)

**Interfaces:**
- Consumes: Task 2 y Task 3.
- Produces:
  - `template <typename TB> void family_tb_connected(TB &tb, void (*on_update)(JsonObjectConst const &), void (*on_request)(JsonObjectConst const &), bool *subscribed);` — suscripción una sola vez por cliente, petición de `hand_hold_until` y `family_qr_url` en cada conexión.
  - `void family_state_apply_json(JsonObjectConst const &data, bool fromRequest);` — lee `hand_hold_until` como `double` y `family_qr_url` como `const char *` (una clave ausente no toca su parte), toma la hora con `time(nullptr)` y la fuente con `systemClockSource()` (de red = NTP o NITZ).
  - `void family_state_on_discharge(void);`
  - `bool family_state_take_msg(uint32_t nowMs, bool force, FamilyMsg *out);`
- Los callbacks del SDK solo llaman a `family_state_apply_json` (nunca publican nada: mismo motivo que en `runtime_config_tb.h`). Cada tarea (GPRS y WiFi) tiene sus dos callbacks y su `static bool s_familySubscribed`.

- [ ] **Step 1: Escribir `family_tb.h` y `family_state.*`** con las firmas de arriba.
- [ ] **Step 2: Cablear GPRS y WiFi**: `family_tb_connected(...)` justo después de `runtime_config_tb_connected(...)`.
- [ ] **Step 3: Cablear `CommTask.cpp`**:
  - en `BABY_MSG_DISCHARGE`, si `ok`, `family_state_on_discharge()`;
  - con `HMI,UI_READY` y `HMI,REQ,STATE`, `s_familySendPending = true`;
  - en el bucle, después del bloque SERVICE: si `family_state_take_msg(millis(), s_familySendPending, &m)`, `family_proto_build` + `CommunicationHost_Send`, y `s_familySendPending = false`.
- [ ] **Step 4: Subir la versión** a `18.68.0`.
- [ ] **Step 5: Compilar y pasar todos los tests de host**

Run: `pio run -e IncuNest_V18 -e IncuNest_V19 && pio test -e native`
Expected: las dos compilaciones `SUCCESS`; todos los tests de `native` pasan (también los de antes).

- [ ] **Step 6: Commit** — `feat(mb): aviso de Coge mi mano desde ThingsBoard hasta la pantalla (motherBoard 18.68.0)`

---

### Task 5: Núcleo de la pantalla `family_view`

**Files:**
- Create: `Firmware/Display_HMI/src/modules/family/family_view.h` / `.cpp`
- Modify: `Firmware/Display_HMI/platformio.ini` (`[env:native]` `build_src_filter`: `+<modules/family/family_view.cpp>`)
- Test: `Firmware/Display_HMI/test/test_family_view/test_family_view.cpp`

**Interfaces:**
- Produces:
  - `enum FamilyViewMode : uint8_t { FAMILY_VIEW_HIDDEN = 0, FAMILY_VIEW_TEXT = 1, FAMILY_VIEW_TEXT_QR = 2 };`
  - `struct FamilyViewInputs { bool notice; bool hasUrl; bool onLockScreen; bool anyAlarm; bool linkLost; bool pulseOxShown; bool unlockSliderShown; };`
  - `FamilyViewMode family_view_mode(const FamilyViewInputs *in);`
  - `bool family_help_entry_visible(bool hasUrl, bool linkLost);`

- [ ] **Step 1: Tests que fallan**

```cpp
static FamilyViewInputs ok() { return {true, true, true, false, false, false, false}; }
void test_all_clear_with_url_shows_qr(void) { auto in = ok(); TEST_ASSERT_EQUAL(FAMILY_VIEW_TEXT_QR, family_view_mode(&in)); }
void test_without_url_text_only(void);        // hasUrl=false -> FAMILY_VIEW_TEXT
void test_no_notice_hidden(void);             // notice=false -> HIDDEN (aunque haya url)
void test_not_on_lock_hidden(void);           // onLockScreen=false -> HIDDEN
void test_alarm_hides_then_returns(void);     // anyAlarm=true -> HIDDEN; vuelta a false -> TEXT_QR
void test_link_lost_hides(void);              // linkLost=true -> HIDDEN
void test_pulseox_hides(void);                // pulseOxShown=true -> HIDDEN
void test_unlock_slider_hides(void);          // unlockSliderShown=true -> HIDDEN
void test_help_entry(void);                   // (true,false)->true; (false,false)->false; (true,true)->false
```

- [ ] **Step 2: Comprobar que fallan**

Run: `pio test -e native -f test_family_view`
Expected: FAIL (no existe `family_view.h`)

- [ ] **Step 3: Implementar** las dos funciones.
- [ ] **Step 4: Comprobar que pasan**

Run: `pio test -e native -f test_family_view`
Expected: 9 tests, 0 failures

- [ ] **Step 5: Commit** — `feat(hmi): núcleo del aviso de familia (family_view)`

---

### Task 6: Pantalla — recibir `CTRL,FAMILY` y pintar el aviso en bloqueo

**Files:**
- Modify: `Firmware/Display_HMI/src/tasks/CommTask.cpp` (globales junto a `g_pendingService` ~l. 84; rama `CTRL,FAMILY,` en `parse_line`, junto a `CTRL,SERVICE,` ~l. 1263)
- Modify: `Firmware/Display_HMI/include/tasks/CommTask.h` (`extern volatile bool g_pendingFamily; extern FamilyMsg g_familyMsg;`)
- Create: `Firmware/Display_HMI/include/ui/FamilyNotice.h` y `Firmware/Display_HMI/src/ui/FamilyNotice.cpp` (widgets del aviso, último mensaje recibido y su pasada; mismo patrón de módulo que `HelpDialog`)
- Modify: `Firmware/Display_HMI/src/ui/ElementsCreation.cpp` (llamar a `FamilyNotice_Init(ui_ScreenLock)` después de crear `ui_LockVitCont`)
- Modify: `Firmware/Display_HMI/src/tasks/UITask.cpp` (llamar a `FamilyNotice_Poll()` en la pasada de UI, junto a los `*_Poll()` de los diálogos)
- Modify: `Firmware/Display_HMI/include/ui/i18n_strings.def`

**Interfaces:**
- Consumes: `family_proto_parse` (Task 2), `family_view_mode` (Task 5), `UI_IsAnyAlarmActive()`, `Display_IsBoardLinkLost()`.
- Produces (en `FamilyNotice.h`): `void FamilyNotice_Init(lv_obj_t *lockScreen);`, `void FamilyNotice_Poll(void);`, `bool Family_HasUrl(void);`, `const char *Family_Url(void);` — los dos últimos los usa la Task 7.

- [ ] **Step 1: Parseo.** Rama `strncmp(line, "CTRL,FAMILY,", 12) == 0`: si `family_proto_parse` acepta, copia a `g_familyMsg` y `g_pendingFamily = true`; si no, `COMM_LOG` y se descarta (queda el último válido). Comprobar que ningún prefijo anterior de la cadena de `else if` se come `CTRL,FAMILY,`.
- [ ] **Step 2: Textos** (cuatro columnas, ASCII):

```
UI_STR(STR_FAMILY_HOLDING, "Alguien esta cogiendo la mano de tu bebe", "Someone is holding your baby's hand", "Quelqu'un tient la main de votre bebe", "Alguem esta a segurar a mao do seu bebe")
UI_STR(STR_FAMILY_SCAN,    "Escanea para verlo", "Scan to see", "Scannez pour voir", "Leia o codigo para ver")
```

- [ ] **Step 3: Widgets.** En `ui_ScreenLock`, oculto al crear: contenedor 792 × 117 alineado `LV_ALIGN_BOTTOM_LEFT` en (4, −1) como `ui_LockPulseOxFrame`, fondo `0x054E92`, radio 8, sin clic (`LV_OBJ_FLAG_CLICKABLE` quitado, para no robar el toque que despierta el deslizador); etiqueta `STR_FAMILY_HOLDING` en `lv_font_montserrat_26` blanca; etiqueta `STR_FAMILY_SCAN` en `montserrat_18`; `lv_qrcode` de 105 px a la derecha, negro sobre blanco, con 6 px de margen blanco. En modo `TEXT`, QR y `SCAN` ocultos y el texto centrado.
- [ ] **Step 4: Pasada de UI (`FamilyNotice_Poll`).** Al consumir `g_pendingFamily`, guardar el mensaje y `lv_qrcode_update` solo si la URL cambió. En cada pasada, calcular `FamilyViewInputs` (`onLockScreen = lv_scr_act() == ui_ScreenLock`, `pulseOxShown = !lv_obj_has_flag(ui_LockPulseOxFrame, LV_OBJ_FLAG_HIDDEN)`, `unlockSliderShown` igual con `ui_UnlockCont`) y mostrar/ocultar según `family_view_mode`. Al cambiar de idioma, refrescar los textos.
- [ ] **Step 5: Compilar y tests**

Run: `pio run -e main -e hmi_screenshots && pio test -e native`
Expected: `SUCCESS` en las dos; todos los tests de `native` pasan.

- [ ] **Step 6: Commit** — `feat(hmi): aviso de Coge mi mano en la pantalla de bloqueo`

---

### Task 7: Ayuda — tarjeta «Familia» con el QR grande

**Files:**
- Modify: `Firmware/Display_HMI/src/ui/HelpDialog.cpp` (`enum View` + `VIEW_FAMILY`; `buildMenu`; `buildFamily`; `showView`)
- Modify: `Firmware/Display_HMI/include/ui/i18n_strings.def`

**Interfaces:**
- Consumes: `Family_HasUrl()`, `Family_Url()` (Task 6), `family_help_entry_visible` (Task 5).

- [ ] **Step 1: Textos**

```
UI_STR(STR_HELP_FAMILY_TITLE, "FAMILIA", "FAMILY", "FAMILLE", "FAMILIA")
UI_STR(STR_HELP_FAMILY_SUB,   "QR para la madre o el padre", "QR for the mother or father", "QR pour la mere ou le pere", "QR para a mae ou o pai")
UI_STR(STR_HELP_FAMILY_HINT,  "Que la familia escanee este codigo con su movil", "Ask the family to scan this code with their phone", "Demandez a la famille de scanner ce code avec leur telephone", "Peca a familia para ler este codigo com o telemovel")
```

- [ ] **Step 2: Menú de ayuda.** Con `family_help_entry_visible(Family_HasUrl(), Display_IsBoardLinkLost())`: cuatro tarjetas de 178 con 16 de hueco (la geometría de `MenuDialog.cpp`), la cuarta «Familia» en azul `0x054E92`; si no, las tres de 236 de hoy, sin cambios.
- [ ] **Step 3: `buildFamily()`** como `buildVideo()`: título, `makeQr(QR_SIZE, 20, 56)` con `Family_Url()`, y `STR_HELP_FAMILY_HINT` a la derecha. Si al abrir la vista ya no hay URL (alta entre medias), volver a `VIEW_MENU`.
- [ ] **Step 4: Compilar**

Run: `pio run -e main -e hmi_screenshots`
Expected: `SUCCESS`

- [ ] **Step 5: Subir la versión** de Display_HMI a `4.13.0` y commit — `feat(hmi): tarjeta Familia con el QR de la estancia en la ayuda (Display_HMI 4.13.0)`

---

### Task 8: Prueba en banco y cierre

Sin código nuevo: comprobar en una IncuNest de banco (V18 o V19) con ambas placas flasheadas y conectada a ThingsBoard, en el tenant de pruebas, poniendo los atributos compartidos a mano desde la interfaz de ThingsBoard.

- [ ] **Step 1:** `hand_hold_until` = ahora + 2 min (en ms), sin `family_qr_url` → a los 20 s sin tocar, bloqueo con el aviso solo texto; desaparece a los ~2 min.
- [ ] **Step 2:** `family_qr_url` = `https://example.org/f/PRUEBA1` y otro `hand_hold_until` → aviso con QR; un móvil lo lee y abre esa dirección. Ayuda con cuatro tarjetas; «Familia» enseña el QR grande.
- [ ] **Step 3:** con el aviso visible: provocar una alarma (p. ej. abrir la puerta o la que se use en banco) → el aviso desaparece; al retirarla vuelve si sigue vigente. Poner el pulsioxímetro → desaparece. Tocar la pantalla → sale el deslizador y el aviso se oculta.
- [ ] **Step 4:** reiniciar la placa base con el aviso vigente → vuelve solo si la hora es de red; con hora manual (Ajustes) no vuelve.
- [ ] **Step 5:** dar de alta al bebé en la IncuNest → el QR y el aviso desaparecen en el acto y la tarjeta «Familia» se va.
- [ ] **Step 6:** pantalla vieja (4.12.2) con placa nueva → nada nuevo en pantalla y ningún error visible; placa vieja con pantalla nueva → sin aviso.
- [ ] **Step 6b:** con una URL y sin «Coge mi mano», abrir la ayuda → la tarjeta «Familia» está; dejar abierta la vista «Familia» hasta que acabe un aviso → sigue abierta.
- [ ] **Step 6c:** borrar o vaciar `family_qr_url` en ThingsBoard y reconectar la IncuNest → el QR desaparece.
- [ ] **Step 6d:** comprobar que el QR se lee y que no lo tapa el icono ámbar de audio en pausa ni su cuenta atrás (abajo a la derecha, capa superior) si hay una alarma silenciada.
- [ ] **Step 6e:** comprobar que los subtítulos FR/PT de las cuatro tarjetas de la ayuda caben.
- [ ] **Step 7:** archivar el cambio de openspec según `Firmware/openspec/config.yaml` y abrir el PR a `dev` con el resultado del banco en la descripción.
