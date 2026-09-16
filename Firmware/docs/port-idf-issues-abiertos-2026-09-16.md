# Port a ESP-IDF: issues abiertos a 2026-09-16 (noche)

Estado congelado antes de volver a PlatformIO (`112a2e9`) para la tanda de
fabricación del 2026-09-17. Todo lo de aquí se midió en banco con la unidad
SN 353 (**HW 18** según el usuario; el `17` del campo `hwNum` de `CTRL,STATE`
es el `HW_NUM` con el que se compiló, no una lectura del hardware), firmware
`dev` = `db71994` (`v17.0.0-754-gdb71994`) y las ramas indicadas. Sirve para
abrir los issues formales en otra sesión.

Evidencia (logs de las 7 fases y el coredump) copiada fuera del temporal a
`C:\Users\Pablo\Documents\IncuNest_dev\bench-logs-2026-09-16\`.

---

## 1. [CRÍTICO] La motherBoard aborta por heap interno agotado (COMM_TASK_RX)

**Síntoma.** `abort()` a los 24,7 min de uptime. Coredump extraído y decodificado:

```
Crashed task: 'COMM_TASK_RX' (Communication_Receiver, motherBoard/src/main.cpp:386)
#6 operator new (sz=141)                      ← no cabe una reserva de 141 B
#5 __cxa_allocate_exception (thrown_size=100) ← ni la excepción bad_alloc
#4 std::terminate()  →  abort()
rst:0xc (RTC_SW_CPU_RST)
```

El sitio del abort es la concatenación de 8 `String` del `logI()` de comandos
del HMI (1-2 veces/s). Es el canario, no la causa — y ese `logI` está
**compilado fuera** (`LOG_INFORMATION` = false en `main.h`): se construye el
String para no imprimir nada.

**Causa raíz: capacidad, no fuga.** Serie de heap interno (commit `8d07c11`,
`DIAG` cada 60 s):

| estado | heap_int libre | bloque mayor | mín. histórico |
|---|---|---|---|
| arranque, sin red | 41-77 KB | 32 KB | — |
| WiFi + PPP + MQTT/TLS arriba | **~11 KB** (plano 45 min) | 2,5-3,4 KB | 0,3-1,7 KB |
| durante una reconexión TLS | **~6 KB** | **1 KB** | — |

Cadena observada entera (fase 7, `t` = uptime):

```
t=484  transport_base: tcp_write error, errno=11 (EAGAIN)   ← un publish no consigue memoria
       mqtt_client: Client force reconnect requested
t=490  heap 5 984 B, bloque mayor 1 024 B                    ← la reconexión TLS se lleva el resto
t=498  esp-tls: getaddrinfo() returns 202                     ← el resolver no reserva ni la consulta
       [TB] Subscribing the given topic ... failed            → reintento cada 15-30 s → bucle
```

**OJO con el 202:** `lwip/src/api/netdb.c` devuelve `EAI_FAIL` (202) ante
cualquier error del resolver, incluido `ERR_MEM`. Un 202 NO significa "el DNS
no contesta". Esto invalidó dos hipótesis intermedias de la sesión (DNS pisado
por PPP; resolver del hotspot caído) — descartadas con observación directa
(`DIAG: NET`: ruta = `WIFI_STA_DEF`, DNS 0 correcto) y 45 min sin una sola
desasociación WiFi (no es RF).

**Lo que ya hay en `feat/mb-heap-diag` (3 commits, sin mergear):**
- `8d07c11` feat: serie de heap interno en `loop()`.
- `64208f8` feat: netif por defecto + tabla DNS global en `DIAG: NET`.
- `bb8321e` fix: DNS de respaldo **por netif** (8.8.8.8) +
  `CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF`. El `dns_setserver(1,…)` global
  de `configWifiServer()` se borraba en la restauración por netif (se veía
  `dns=[192.168.137.1 - -]`). Efecto: la conexión MQTT WiFi aguantó 8 min con
  el OTA-check respondido cada minuto (antes 0). **Mejora, no cura**: la
  tormenta volvió en la primera reconexión.

**Lo que falta y es decisión de diseño (ADR):**
- ¿Deben convivir MQTT por WiFi y PPP arriba a la vez? Hoy PPP se mantiene
  siempre; el celular publica solo sin WiFi (`GPRS.cpp:57`).
- Búferes del cliente ThingsBoard: 2×4352 B contiguos, dimensionados para OTA
  (`TB_MQTT_BUFFER_WIFI`).
- Servidor web residente: su pila de 10 KB **ya no arranca**
  (`plat_http: httpd_start(80) -> ESP_ERR_HTTPD_TASK`, 5/5 arranques con WiFi).
  La página de flasheo web no existe en el port hoy.
- Retirar el String del `logI` muerto en `Communication_Receiver`.
- `tb_mqtt: sin RAM para el cliente MQTT` (10 veces seguidas en una sesión con
  WiFi conectado): el parche INCUNEST (7) evita el pánico, pero deja la placa
  sin telemetría WiFi.

## 2. [ALTO] Sin telemetría por WiFi en la práctica

Consecuencia directa del 1: en 45 min con WiFi asociado (RSSI -33 dBm, 0
desconexiones) hubo **0** publicaciones útiles por WiFi, 133 fallos de
resolución y 89 reconexiones forzadas. Con el fix `bb8321e`, 8 min y luego lo
mismo. En campo un equipo con WiFi bueno puede estar sin telemetría sin que
nada lo delate.

## 3. [MEDIO] El HMI también va justo de heap interno

`E DIAG: LOW RESOURCES heap_int=9892 heap_int_min=9316 heap_psram=7429592
ui_hwm=12608 B comm_hwm=12556 B` cada 60 s, **plano** (no fuga), ~9,8 KB
libres con 7,4 MB de PSRAM sin usar. Descriptores DMA de WiFi y búferes de
dibujo no pueden ir a PSRAM.

## 4. [MEDIO] MQTT WiFi: `transport_read(): EOF, errno=128` ~1 s tras crear el cliente

En los 5 arranques: cliente creado a t≈41 s, a t≈42 s el servidor cierra la
conexión (`errno=128`), esp-mqtt reconecta solo. Sin explicar. Puede ser el
broker rechazando la primera sesión o la NAT del hotspot.

## 5. [BAJO] WiFi `STA_DISCONNECTED reason=204` a los 19 s en cada arranque

`WIFI_REASON_HANDSHAKE_TIMEOUT` a t≈19 s, IP a t≈40 s. 5/5 arranques.
Probablemente el hotspot de Windows del banco; verificar con un AP normal.

## 6. [BAJO] `logI()` no llega al puerto serie

`LOG_INFORMATION` = false: ninguna línea `[WIFI]`, `[BOOT]`, `[GPRS]` de
`logI` aparece en un log de banco, y hay diagnósticos que solo existen ahí
(p. ej. `WIFI MQTT PUBLISH TELEMETRIES SUCCESS`). Otra sesión está pasando
algunos a `ESP_LOGW` (`EEPROM.cpp`, `initHardware.cpp`, sin commitear a las
22:30). La prueba de MQTT WiFi vivo hoy es `[TB] No new firmware assigned on
the given device` cada 60 s (logger del SDK).

## 7. [BAJO] Menores vistos en los logs

- `MODEM: NTP over PDP failed`, `NITZ clock not valid yet` en cada arranque.
- `USBH: Dev 0 EP 0 Error` / `ENUM: CHECK_SHORT_DEV_DESC FAILED` al arrancar
  (sin SensorBoard conectada, probablemente esperado).
- `mqtt_client: Publish: Losing qos0 data when client not connected` (ruido
  del 1).

## 8. Herramientas y banco (para no volver a perder una hora)

- **UART compartida MB↔HMI**: leer el coredump de la MB con `esp_coredump`
  se corrompe si el HMI emite (`Serial data stream stopped`). Callar la otra
  placa antes (bootloader / RTS). Flashear el HMI con la MB en bootloader
  (`--after no-reset`) funciona sin cortar corriente.
- Si el HMI se queda en download mode, `esptool run` no sincroniza por la
  línea compartida: reset por **pulso de RTS** (DTR=0, RTS=1→0).
- Puertos del banco hoy: **HMI = COM30** (CH340K, flash 16 MB + PSRAM),
  **MB = COM32** (FTDI, flash 8 MB). Identificar por `esptool flash-id`, no
  por memoria: ya bailaron una vez.
- `pktmon` (captura DNS en el adaptador del hotspot) necesita consola
  elevada; quedó sin hacer.
- `Firmware/tools/host_tests` y `.claude/rules` siguen citando `pio test`;
  con la vuelta a PlatformIO parte de eso vuelve a ser cierto — revisar.

## 9. Pendientes heredados (ver memoria del proyecto)

- Zona horaria solo en RAM: al perderla el display pinta UTC sin avisar.
- El banco vuelve solo a una imagen anterior por OTA de ThingsBoard
  (firmware asignado en el servidor): comprobar la asignación antes de
  culpar al flasheo.
- Lista de pendientes tras el porte de la rama `test/plat-contract-apps`
  (`c9c2942`).

## 10. Estado del repo al congelar

- `dev` = `db71994` (port IDF). Ramas con trabajo sin mergear:
  `feat/mb-heap-diag` (3 commits, arriba), `feat/mb-baby-count`,
  `refactor/afe4490-submodulo-v092`, `test/plat-contract-apps`.
- Worktree `Firmware/.worktrees/pio-stable` en `112a2e9` (último `dev`
  PlatformIO, 2026-09-07) con `Credentials.h` copiados a mano: es lo que se
  compila y flashea para fabricación.
- `motherBoard/sdkconfig` (no versionado) tiene
  `CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF=y` aplicado localmente.
