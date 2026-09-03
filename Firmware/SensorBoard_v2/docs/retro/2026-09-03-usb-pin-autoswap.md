# Retro — Autoswap D+/D- del PHY USB (`usb_comm`, conector invertido)

**Fecha de cierre:** 2026-09-03
**Change OpenSpec:** `openspec/changes/archive/2026-09-03-sb-usb-pin-autoswap/`
**Rama:** `feat/sb-usb-pin-autoswap` (desde `dev`, worktree `Firmware/.worktrees/sb-usb-autoswap`)
**ADR:** 0003

## Qué se hizo

Para el **SensorBoard HW_NUM 4** (la V5, ya en fabricación, corrige el conector en hardware). Política pura `sb_usb_orient_*` (6 `TEST_CASE` en `comm_test`) + ejecución en `usb_tx_task`: si no hay host activo (`tud_mounted() || tud_connected()`) en 2 s, `tud_disconnect()` → `usb_wrap_ll_phy_enable_pin_exchg()` → 250 ms → `tud_connect()`, alternando hasta enumerar; estado en `status` como `sensors.usb_swap`. Kconfig `SB_USB_AUTOSWAP`. Descartado el fallback UART/I2C que se había planteado. De paso se corrigió un fallo latente del transporte: DTR pegajoso (escrituras al CDC exigen ahora `tud_cdc_n_connected()`).

## Aprendizajes

1. **Antes de diseñar un fallback, buscar si el silicio ya resuelve el problema.** La pregunta original era "¿UART o I2C si USB no conecta?"; diez minutos de `grep exchg` en los headers del ESP-IDF mostraron el bit `exchg_pins` del PHY y el helper de v6. La alternativa habría duplicado la pila de transporte en dos placas de un dispositivo médico. Mismo patrón que la Fase 5 (leer `sccb-ng.c` antes de diseñar).
2. **Los `TEST_CASE` de Unity en un fichero sin `app_main` no entran al binario sin `WHOLE_ARCHIVE`.** El primer build "rojo" enlazó en verde: `libmain.a` es estática y nada referenciaba `test_usb_orient.c.obj`, así que el linker lo descartó junto con sus constructores. Un rojo que no falla es la señal de alarma; la verificación fue buscar el objeto en el `.map`. Sistematizado: `test_apps/comm_test/main/CMakeLists.txt` lleva `WHOLE_ARCHIVE` con comentario; regla propuesta para `.claude/rules/testing.md` cuando se retargetee al SensorBoard.
3. **`idf.py` no corre desde Git Bash (MSYS) en Windows**: imprime "MSys/Mingw is no longer supported" y sale con código 0 sin compilar — un falso verde silencioso para cualquier hook o script que lo invoque desde bash. En esta máquina hay que lanzarlo desde PowerShell (launcher `idf-exe`). Afecta al diseño del Stop hook de build de este proyecto.
4. **Una premisa sobre la pila USB que no se ha leído en su fuente es una hipótesis, no un hecho — y la revisión de seguridad la cazó.** El diseño descartó `tud_connected()` "porque el bus reset (SE0) es simétrico ante el cruce y lo pondría a true". Falso: TinyUSB lo pone al recibir el **primer SETUP** (imposible con D+/D- cruzados) y lo borra en reset/unplug. La guarda correcta es `mounted || connected`, que además elimina el riesgo real que la premisa ocultaba: un host lento en configurar habría sufrido intercambios (y potencialmente un batido sin enumerar nunca). Patrón repetido con el aprendizaje 1 y con la Fase 5: **verificar en el código del vendor antes de fijar una convención en un ADR**; una premisa errónea en un ADR se reutiliza en la siguiente decisión. Se enmienda en el propio ADR-0003 dejando rastro de la corrección.
5. **Separar política pura de ejecución sobre hardware** volvió a pagar: la lógica de plazos (incluido el wrap de `uint32`) se prueba sin host y sin PHY; lo único que queda para on-target es la secuencia de 4 líneas con TinyUSB. Y el cambio de guarda (punto 4) no tocó ni la política ni sus tests: solo el `bool` que le pasa el caller.
6. **Los flags "de estado del enlace" derivados de callbacks son pegajosos si nadie los limpia.** `s_cdc_ready` (DTR) solo lo escribía `line_state_cb`, que TinyUSB no invoca en reset/unplug; sin VBUS sensing, un host que se apaga dejaba al firmware escribiendo en un FIFO muerto y descartando telemetría en silencio (fallo latente desde Fase 1, visible ahora porque el swap dependía de la retención). Regla: la condición para escribir se deriva del estado vivo de la pila (`tud_cdc_n_connected()`), no de un flag recordado.
7. **Revisar en paralelo con dos roles distintos merece la pena**: el `code-reviewer` dio el cambio por bueno (con recomendaciones) y el `security-reviewer`, mirando el mismo diff con lente de fail-safe, encontró dos bloqueantes. No son redundantes.

## Diferido

- Verificación on-target (checklist en el `design.md` del change): orientación correcta, invertida, re-enchufe en caliente, arranque sin host.
- El flasher (`Firmware/flasher_tool`) debe indicar "gira el cable" si no detecta el SensorBoard: el ROM no aplica el intercambio.
- Persistir la orientación acertada en NVS para arrancar sin los 2 s de espera (no hay NVS inicializado hoy).
- Hardware: corregir el cableado del conector en la siguiente revisión de PCB (nota en `docs/hardware.md`).
