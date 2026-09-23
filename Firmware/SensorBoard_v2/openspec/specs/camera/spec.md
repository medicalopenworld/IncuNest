# camera Specification

## Purpose
TBD - created by archiving change sb-phase5-camera. Update Purpose after archive.
## Requirements
### Requirement: Captura bajo demanda

Al recibir `capture`, `camera_sensor` SHALL capturar un JPEG (OV2640, QVGA por defecto), responder `{"type":"resp","cmd":"capture","id":N,"status":"ok","size":<bytes>,"ts":…}` y enviar a continuación los bytes JPEG como frame `TYPE=0x01`. El handler del comando solo notifica a `camera_task`; la captura nunca corre en el contexto del RX.

#### Scenario: Resp de captura ok (builder)

- **WHEN** se construye la resp con id=7, ok, size=12345, ts=99
- **THEN** el resultado es exactamente `{"type":"resp","cmd":"capture","id":7,"status":"ok","size":12345,"ts":99}`

#### Scenario: Resp de error (builder)

- **WHEN** se construye la resp de fallo con id=7 y msg="capture failed"
- **THEN** el resultado es `{"type":"resp","cmd":"capture","id":7,"status":"error","msg":"capture failed","ts":…}`

#### Scenario: Captura en vuelo

- **WHEN** llega un `capture` mientras otro está en curso
- **THEN** se responde `status:"error"` con `msg:"busy"` sin encolar una segunda captura

#### Scenario: Fallo de cámara

- **WHEN** `esp_camera_fb_get` falla o la cámara no inició
- **THEN** se responde `error` y `sensors.cam` es false; el resto del firmware sigue operativo

### Requirement: SCCB comparte el bus I2C principal

La cámara SHALL usar `sccb_i2c_port` apuntando al bus I2C principal ya creado por `env_sensors` (IO4/IO5), sin crear un segundo bus en el mismo puerto.

#### Scenario: Bus no disponible

- **WHEN** el bus principal no llegó a inicializarse (`sb_env_get_main_i2c_bus()` es NULL)
- **THEN** el init de cámara falla limpio con `sensors.cam:false`, sin tocar el puerto

