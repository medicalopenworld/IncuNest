---
name: arch-embedded-layering
description: Convenciones de límites de componente ESP-IDF y principios SOLID-adyacentes que sí trasladan a C para el firmware SensorBoard. Usar al diseñar dónde vive la lógica de una fase/sensor nuevo, al definir el header público de un componente, o ante dudas sobre qué puede depender de qué entre `usb_comm` y los componentes de fase.
---

# Límites de componente en ESP-IDF (SensorBoard)

## Capas y dependencias

La regla de oro: **nada por encima de `usb_comm` conoce el framing/CRC; `usb_comm` no conoce
ningún sensor**. `usb_comm` es la capa agnóstica de transporte: framing (`Magic+Type+Length+Payload+CRC16-CCITT`),
CRC16-CCITT, y las tareas FreeRTOS de RX/TX. Ningún componente de fase (Phase 2 sensores, Phase 3
micrófono, Phase 4 puerta, Phase 5 cámara) debería necesitar reabrir `usb_comm` — solo llamar a su
API pública.

```
components/
├─ usb_comm/              ── transporte agnóstico: framing, CRC16-CCITT, tareas RX/TX
│   ├─ include/            ── header público = el "puerto": sensorBoard_comm_send_json(),
│   │                          send_binary(), sensorBoard_comm_protocol.h (SB_PROTO_FW_VERSION)
│   └─ (privado)           ── cola interna, parser de frames, detalle de la tarea — no se expone
│
├─ sht40_als/              ── Phase 2: sensor propio, su propia tarea FreeRTOS
├─ i2s_mic/                ── Phase 3: micrófono/dBA, su propia tarea
├─ door_sensor/            ── Phase 4: GPIO+ISR, su propia tarea
└─ camera/                 ── Phase 5: cámara/JPEG, su propia tarea
        │
        ▼ (todas llaman, ninguna es llamada)
   usb_comm: sensorBoard_comm_send_json() / send_binary()
```

| Componente         | Contiene                                                        | NO debe hacer                                                        |
| ------------------- | ---------------------------------------------------------------- | ---------------------------------------------------------------------- |
| `usb_comm`          | framing, CRC16-CCITT, tareas RX/TX, cola de envío               | conocer el formato/semántica de ningún sensor concreto                |
| `<fase>` (p.ej. `sht40_als`) | su tarea FreeRTOS, lectura de hardware, construcción del JSON/payload | acceder a las colas o al parser internos de `usb_comm`; solo llama a su header público |
| `main/`             | arranque, init de componentes, wiring de tareas (composition point) | lógica de negocio del sensor (vive en el componente)                  |

## El header público ES la interfaz

C no tiene interfaces/genéricos al estilo TypeScript — no hay nada equivalente que inventar. La
interfaz real de un componente ESP-IDF **es su header público** en `include/`:

- Lo que se declara en `include/<componente>.h` (`PUBLIC_INCLUDE_DIRS` en `CMakeLists.txt`) es el
  contrato que otros componentes pueden usar.
- Lo que vive en `priv_include/` (`PRIV_INCLUDE_DIRS`) o en `.c` sin exponer en el header es
  interno: nadie fuera del componente lo toca, ni siquiera con un `#include` directo al `.c`.
- Un componente que necesita algo de otro **incluye su header público**, nunca su
  `priv_include/` ni su `.c`. Si hace falta algo que solo existe en lo privado, esa función se
  promueve al header público (decisión de diseño, no un atajo).

## FreeRTOS como mecanismo de composición

No hay contenedor de inyección de dependencias ni "composition root" al estilo apps/Next. La
composición ocurre en `main/` y se expresa con las primitivas de FreeRTOS:

- **Tareas** (`xTaskCreate`) son la unidad de ejecución independiente — un componente = una o
  varias tareas propias, no hilos compartidos con otros componentes.
- **Colas** (`QueueHandle_t`) y **semáforos** son el "cableado": una ISR o una tarea productora
  entrega datos a una cola; la tarea consumidora los procesa. Esto sustituye tanto al event bus
  como a la inyección de dependencias del template original.
- **Ninguna lógica en una ISR** más allá del hand-off a una cola/semáforo (`xQueueSendFromISR` o
  equivalente) — la lógica de negocio vive siempre en la tarea, nunca en el handler.

## Qué de SOLID sí traslada a C

- **S (responsabilidad única)**: un componente = una responsabilidad de hardware/protocolo. Una
  tarea FreeRTOS = un bucle de trabajo coherente, no una tarea "todoterreno".
- **D (depender de abstracciones)**: un componente depende del **header público** de otro, nunca
  de su implementación interna — la inversión de dependencias aquí es "depende del contrato, no
  del `.c`", no un patrón de interfaces inyectadas.
- El resto (Open/Closed vía adaptadores sustituibles, Liskov de implementaciones intercambiables,
  Interface Segregation de puertos pequeños) **no tiene traducción real en C sin frameworks de
  OO** — no se fuerza una analogía artificial. Lo que importa en este stack es el límite de header
  público/privado de arriba, no un mapeo 1:1 de las cinco letras.

## Olores a evitar

- `#include "../otro_componente/algo_privado.h"` — saltarse el header público de otro componente.
- Lógica de negocio (parseo de JSON de un sensor, cálculo de dBA, decisión de fail-safe) dentro de
  `usb_comm` — eso rompe el principio de transporte agnóstico.
- Trabajo pesado o bloqueante dentro de una ISR (debe ser hand-off puro a cola/semáforo).
- Un componente de fase leyendo directamente registros/colas internas de `usb_comm` en vez de
  llamar a `sensorBoard_comm_send_json()`/`send_binary()`.
