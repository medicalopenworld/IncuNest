# Protocolo de Comunicación IncuNest (v1.5.0)

Este documento describe el protocolo de comunicación serie utilizado entre la Motherboard (MCU) y el Display (HMI).

## Especificaciones de Capa Física
- **Interfaz**: UART (vía chip puente USB-Serie CH340C).
- **Baudios**: 115200.
- **Formato**: 8N1.

## Tipos de Mensajes

### 1. Mensajes de la Motherboard (MCU → HMI)

#### CTRL,STATE
Enviado cada 1 segundo o bajo petición (`HMI,REQ,STATE`).
**Formato**: `CTRL,STATE,act,mode,airSet,skinSet,humSet,photo,mute,sn,hwNum,hwRev,fwVer,numAlarms,skinE,commStatus,photoTimeRem,lang,alarmBitmask`

- `alarmBitmask`: (Hexadecimal, ej: `0x60`) Indica qué IDs de alarma están activos. Requerido para sincronización robusta.

#### CTRL,TEL (Telemetría en tiempo real)
Enviado cada 1 segundo (intercalado con STATE).
**Formato**: `CTRL,TEL,airDet,skinDet,humDet,serverStatus`

#### CTRL,ALM (Evento de Alarma)
Enviado cuando una alarma cambia de estado.
**Formato**: `CTRL,ALM,id,short_text,long_text,active`
- `active`: `1` (Activa), `0` (Eliminada).

### 2. Mensajes del Display (HMI → MCU)

#### HMI,UI_READY (Handshake Crítico)
Enviado una sola vez cuando la interfaz gráfica del HMI está completamente cargada.
**Efecto**: La Motherboard responde reenviando inmediatamente el estado de todas las alarmas activas.

#### HMI (Comandos de Configuración)
Enviado cuando el usuario cambia un parámetro.
**Formato**: `HMI,act,skinE,mode,airSet,skinSet,humSet,photo,mute,lang,photoMin`

#### HMI,REQ,STATE
Solicitud manual de sincronización completa.

---

## Lógica de Sincronización Robusta

Para garantizar que el sistema siempre muestre el estado correcto, se siguen estas reglas:

1.  **Handshake**: Tras un reinicio o reconexión del HMI, este debe enviar `HMI,UI_READY`. La Board no disparará el reenvío de alarmas hasta recibir este comando o detectar tráfico válido.
2.  **Auto-Corrección vía Bitmask**: Si el HMI recibe un mensaje de `CTRL,STATE` con un `alarmBitmask` que no coincide con su lista interna de alarmas pintadas en pantalla, el HMI limpiará visualmente las alarmas que no figuren en el bitmask.
3.  **Filtrado de Etiquetas**: El HMI implementa un filtro de cambios en `update_labels()` para ignorar actualizaciones de texto idénticas, mejorando la respuesta táctil.
