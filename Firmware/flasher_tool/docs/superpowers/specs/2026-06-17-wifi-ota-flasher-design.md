# WiFi OTA Flasher — Design Spec

**Date:** 2026-06-17  
**Scope:** `flasher_tool/flasher/`  
**Goal:** Integrar flasheo OTA por WiFi en el GUI existente (IncuNest Firmware Flasher), permitiendo actualizar motherBoard y Display HMI conectados al hotspot sin cable USB.

---

## 1. Contexto

Ambos boards exponen un WebServer en puerto 80 con endpoint `POST /update` para OTA:

- **Motherboard** (`IncuNest-<sn>`): requiere auth (nueva: `incunestadmin/savinglives`; fallback: `in3admin/savinglives`; fallback final: sin auth para versiones muy antiguas)
- **Display HMI** (`IncuNest_Display-<sn>`): requiere auth (nueva: `incunestadmin/savinglives`; fallback: `in3admin/savinglives`)

Limitación vs USB: vía WiFi solo se flashea `firmware.bin`. Bootloader, partitions table y NVS (número de serie) no se tocan — adecuado para actualizaciones de campo.

---

## 2. Arquitectura

```
flasher_tool/flasher/
├── main.py              ← añade ttk.Notebook, _WifiTab, ajuste altura 580→630
├── wifi_flasher.py      ← nuevo: WifiBoard, discover_boards(), flash_board_wifi()
├── flasher.py           ← sin cambios
├── detector.py          ← sin cambios
├── requirements.txt     ← añade requests>=2.28, zeroconf>=0.128
└── flasher.spec         ← añade hiddenimports de zeroconf y requests
```

Flujo general:

```
[Botón "Buscar y flashear"]
        │
        ▼
  discover_boards(timeout_s=5.0)
  ├─ mDNS (zeroconf, ~2.5s): browse _http._tcp.local → filtra "IncuNest*"
  │   └─ parse hostname → Board type + IP + fw_version
  └─ fallback subnet scan (~2.5s): 50 threads × GET /get_fw_version (timeout 0.3s)
      └─ probe GET /get_freq → 200=Display HMI, otro=Motherboard
        │
        ▼ lista WifiBoard(ip, Board, fw_version)
  slot por dispositivo — flash en paralelo
  └─ flash_board_wifi(ip, board, firmware_base, progress_cb)
```

---

## 3. `wifi_flasher.py`

### Tipos

```python
@dataclass
class WifiBoard:
    ip: str
    board: Board
    fw_version: str
    hostname: str = ""   # vacío si viene de subnet scan
```

### `discover_boards(timeout_s=5.0) → list[WifiBoard]`

1. Lanza mDNS via `zeroconf.ServiceBrowser` en `_http._tcp.local`. Espera `timeout_s / 2`.
2. Filtra servicios cuyo nombre empieza por `IncuNest`. Parsea hostname:
   - `IncuNest_Display-*` → `Board.DISPLAY_HMI`
   - `IncuNest-*` → `Board.MOTHERBOARD`
3. Para cada candidato: `GET /get_fw_version` (timeout 2s) → obtiene versión.
4. Si mDNS devuelve ≥1 resultado, retorna. Si no, hace subnet scan.
5. **Subnet scan:** detecta subred local (via `socket`; fallback `192.168.137.0/24`). Lanza 50 threads en paralelo con `GET /get_fw_version` timeout 0.3s. Para cada IP que responda:
   - `GET /get_freq` timeout 0.3s → 200 = `DISPLAY_HMI`, otro = `MOTHERBOARD`
6. Deduplica por IP. Retorna lista.

### `flash_board_wifi(ip, board, firmware_base, progress_cb, timeout_s=120) → None`

```
firmware_path = firmware_base / {"motherboard"|"display_hmi"} / "firmware.bin"
Abre con _ProgressFile (wrapper → progress_cb en cada read())

Intento 1: POST /update con HTTPBasicAuth('incunestadmin', 'savinglives')
Intento 2 (si 401): POST /update con HTTPBasicAuth('in3admin', 'savinglives')
Intento 3 (si 401 y MOTHERBOARD): POST /update sin auth

Si respuesta.text == "FAIL" → RuntimeError
Si respuesta.text == "OK"  → éxito (device reinicia solo)
```

### `_ProgressFile`

Wrapper file-like sobre el `firmware.bin` abierto. Implementa `read(n)`: llama `progress_cb(chunk_text, pct)` tras cada lectura. Permite a `requests` reportar progreso en uploads multipart sin dependencias extra.

---

## 4. Cambios en `main.py`

### Layout

```
┌────────────────────────────────────────┐
│              [Logo]                    │
│  ─────────────────────────────────     │
│  Status banner (compartido)            │
│  ─────────────────────────────────     │
│  [  USB  ] [  WiFi  ]  ← ttk.Notebook │
│ ┌──────────────────────────────────┐   │
│ │  contenido de la pestaña activa  │   │
│ └──────────────────────────────────┘   │
│  ─────────────────────────────────     │
│  > log compartido (ScrolledText)       │
└────────────────────────────────────────┘
```

- Ventana: `480×630` (antes 580; +50px para tab bar y botón WiFi)
- Log y status banner permanecen **fuera** del Notebook, compartidos entre tabs

### `_WifiTab`

Clase nueva que recibe el frame de la pestaña WiFi, el log callback y la firmware_base.

Contenido:
- Botón `"🔍  Buscar y flashear"` + label de estado
- Hasta 3 `_Slot` reutilizados (el campo `port` muestra la IP)

Flujo al pulsar el botón:
1. Deshabilita el botón, label → `"Escaneando red…"`
2. Thread background: `discover_boards()`
3. En el hilo principal (`root.after`): si 0 resultados → label `"No se encontraron dispositivos"`, re-habilita botón
4. Si ≥1: asigna slots, label → `"N dispositivo(s) encontrado(s)"`, lanza un thread de flash por dispositivo en paralelo
5. Al terminar cada flash: `slot.set_done()` / `slot.set_error()`, log
6. Cuando todos terminan: re-habilita botón, limpia slots tras `SLOT_CLEAR_DELAY_S`

### `_UsbTab`

El contenido actual de `FlasherApp` (slots 1–3, hotplug, serial dialog) se mueve sin cambios semánticos a un frame dentro de la pestaña USB.

---

## 5. Dependencias nuevas

| Librería | Uso | Versión mínima |
|---|---|---|
| `requests` | HTTP upload OTA | >=2.28 |
| `zeroconf` | mDNS discovery | >=0.128 |

Añadir a `requirements.txt`. Añadir a `hiddenimports` en `flasher.spec`:
```python
'zeroconf', 'zeroconf._utils', 'zeroconf._dns', 'zeroconf._services',
'zeroconf._services.browser', 'requests', 'urllib3',
```

---

## 6. Manejo de errores

| Situación | Comportamiento |
|---|---|
| mDNS falla (excepción) | Log warning, procede con subnet scan |
| Subnet scan: 0 dispositivos | Label "No se encontraron dispositivos", botón re-habilitado |
| `/get_fw_version` timeout | IP descartada silenciosamente |
| Auth falla en todos los intentos | `slot.set_error()` + log con IP |
| Upload timeout (>120s) | `slot.set_error()` + log |
| `firmware.bin` no existe | `RuntimeError` antes de intentar upload |

---

## 7. Fuera de scope

- Escritura de número de serie vía WiFi (requiere acceso NVS, no expuesto por HTTP)
- Flash de bootloader/partitions vía WiFi
- Re-scan automático periódico (el usuario pulsa el botón manualmente)
- Autenticación configurable desde UI (credenciales hardcodeadas como en el firmware)
