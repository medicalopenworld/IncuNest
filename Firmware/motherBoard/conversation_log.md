# IncuNest Firmware (motherBoard) — Conversation Log

---

## Sesión 2026-03-18

**Tema:** Inicialización en el proyecto
**Rama activa:** `ppg_tests`

**Contexto al iniciar:**
- Proyecto: Incubadora neonatal open-source de Medical Open World (IncuNest).
- Hardware: ESP32-S3 (V15) + AFE4490, placa propietaria.
- Framework: PlatformIO + Arduino + ESP-IDF + FreeRTOS.
- Dos entornos de build: `in3ator_UP_TO_V14` (FireBeetle32) y `in3ator_V15` (ESP32-S3-DevKitC-1).
- Rama `ppg_tests`: trabajo en curso sobre fototerapia (límites 2-10h, guardado en EEPROM) y timers guardados en EEPROM.

**Últimos commits en esta rama:**
- `404ae97` Phototherapy limits (2-10h) established and saving logic into EPROM implemented
- `2c27474` Timer variable saving into EPROM. Still need to fix the limits
- `19aa31c` Info labels correctly centered on panel
- `c17c957` EPROM timer established as the lock screen inactivity timer
- `81d42ea` Effective communication with motherboard with new elements (stop button, phototherapy logic)

**Decisiones / acciones:**
- Creado `conversation_log.md` (este fichero) al no existir.
- Creado sistema de memoria en `~/.claude/projects/C--PRJ-MOW-IncuNest/memory/`.

---

## Sesión 2026-06-03

**Tema:** Organización git y creación de rama de tests AFE4490 vs fototerapia
**Rama activa al cerrar:** `photo_vs_afe_tests`

**Decisiones / acciones:**
- Creado `CLAUDE.md` en `motherBoard/` con contexto del proyecto (arquitectura, ficheros clave, reglas de oro)
- Aclarado el rol de `CLAUDE.md` (carga automática de contexto) vs `conversation_log.md` (diario manual) vs memoria auto (personal, no versionada)
- Reseteado `dev` local a `origin/dev` (fetch reveló commits nuevos en remoto: tag `v17.0.0`, rama `refactor/modular-architecture`)
- Eliminada rama local `ppg_tests` (era de otro colaborador, `jlacostaarp-unav`)
- Creada rama `photo_vs_afe_tests` desde `dev` para estudiar interferencia de fototerapia en medidas del AFE4490
- `CLAUDE.md` commiteado en `photo_vs_afe_tests` (`a3a6e96`) para que no se pierda en futuros resets
- Restaurada URL de push (`origin` tenía `no-push` configurado) — push bloqueado por falta de permisos de escritura en `medicalopenworld/IncuNest` (usuario `acuesta-mow`); pendiente solicitar acceso al admin (`jlacostaarp-unav`)

- Compilado `motherBoard` (entorno `IncuNest_V17`): SUCCESS — RAM 23.2%, Flash 48.0%
- Compilado `Display_HMI` (entorno `main`): SUCCESS — RAM 34.5%, Flash 62.7%
- Flasheado `motherBoard` vía OTA: `curl POST http://192.168.137.157/update` (sin auth) — HTTP 200 OK
- Flasheado `Display_HMI` vía OTA: `curl POST http://192.168.137.70/update` (auth: in3admin/savinglives) — HTTP 200 OK
- Credenciales OTA Display_HMI en `src/Wifi_OTA.cpp`: usuario `in3admin`, pass `savinglives`
- Compilado `Display_HMI` con título "IncuNest [test]" en `ElementsCreation.cpp:790`
- Flash serie Display_HMI fallido: ESP32-S3 USB nativo requiere driver "USB Serial/JTAG" en Windows (no instalado)
- Ambos dispositivos perdieron WiFi tras OTA: firmware nuevo (`origin/dev`) usa namespaces NVS distintos (`mb_wifi`, `hmi_wifi`) vs firmware anterior
- Usuario corrigió SSID/password desde pantalla Settings; credenciales se guardan en NVS (sobreviven OTA)
- Segundo OTA con título "IncuNest [test]": motherBoard HTTP 200 OK, Display_HMI HTTP 200 OK
- Confirmado: NVS es partición separada, OTA nunca la sobreescribe

---

## Sesión 2026-05-30

**Tema:** Inicio de sesión — revisión de estado
**Rama activa:** `ppg_tests`

**Contexto al iniciar:**
- `main.h` y `main.cpp` modificados sin commitear
- Trabajo previo: fototerapia (límites 2-10h, EEPROM), timers en EEPROM, UI labels

**Decisiones / acciones:**
- (pendiente de tarea)

---
