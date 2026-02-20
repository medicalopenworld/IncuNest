# Análisis de Comunicación Motherboard <-> Display_HMI

## 1. Arquitectura de Comunicación

El sistema utiliza una conexión USB Serial entre la **Motherboard** (Host) y el **Display HMI** (Device).

- **Capa Física:** Conexión USB a través de un chip **CH340C** en el Display.
- **Host (Motherboard):** Utiliza la pila USB de ESP-IDF (`cdc_acm_host`) para actuar como Host y alimentar/comunicar con el Display.
- **Drivers:** El firmware ha sido optimizado para el CH340C. Se han eliminado los drivers de CP210x y FTDI para reducir el tamaño del binario y evitar conflictos.

## 2. Protocolo de Mensajes (Line-based ASCII)

La comunicación se basa en líneas de texto terminadas en `\n`. Todos los comandos válidos comienzan con un prefijo específico.

### Mensajes de Motherboard a HMI (`CTRL`)

- **`CTRL,TEL,air,skin,hum,status`**: Envío periódico (1s) de telemetría.
  - `air / skin`: Temperaturas detectadas (float).
  - `hum`: Humedad detectada (int).
  - `status`: Estado de conectividad (WiFi/Server).
- **`CTRL,STATE,act,mode,airS,skinS,humS,photo,mute,sn,hw,rev,fw,alms,skinE,comm,photoRem`**: Estado completo del sistema enviado tras handshake.
  - Incluye setpoints, versiones de firmware, número de serie y tiempo restante de fototerapia.
- **`CTRL,ALM,id,title,desc,state`**: Notificación de cambio de estado de una alarma.
  - `state`: 1 para activada, 0 para desactivada.

### Mensajes de HMI a Motherboard (`HMI`)

- **`HMI,REQ,STATE`**: Handshake inicial. El HMI lo envía al arrancar para sincronizar su UI con el estado real del hardware.
- **`HMI,act,skinE,mode,air,skin,hum,photo,mute,lang,photoMin`**: Comando de cambio de configuración desde la UI.
- **`HMI,WIFI,ssid,pass`**: Envío de credenciales WiFi configuradas en la pantalla.

## 3. Mecanismo de Robustez

1. **Boot Fix:** La Motherboard realiza una secuencia de conmutación de las líneas DTR/RTS al detectar el chip CH340C para asegurar un arranque estable del Display.
2. **Cola de Alarmas:** La Motherboard mantiene una cola de hasta 10 alarmas pendientes. Si el HMI se desconecta y reconecta, la Motherboard reenvía todas las alarmas activas automáticamente.
3. **Sincronización de Estado:** El HMI no permite cambios hasta que ha recibido el primer `CTRL,STATE`, asegurando que el usuario vea valores reales del hardware.
