# ADRs (Architecture Decision Records)

Registro de decisiones de arquitectura significativas y difíciles de revertir para el firmware del SensorBoard: por ejemplo "por qué `usb_comm` es transport-agnostic", "por qué CRC16-CCITT (poly 0x1021, init 0xFFFF)", o "por qué el framing reserva `TYPE=0x01` para JPEG desde la Fase 1". No se registran decisiones triviales o fácilmente reversibles — para eso está el código y su historial de commits.

Las escribe el agente `scribe`, típicamente en el stage `design` del loop, cuando una decisión relevante necesita quedar justificada más allá del propio commit.

## Convención de nombres

`docs/adr/NNNN-slug-en-minusculas.md`, numeración secuencial de 4 dígitos empezando en `0001` (el `0000-template.md` de esta carpeta no cuenta como ADR real, es la plantilla).

## Índice

| Nº | Título | Estado |
|---|---|---|
| [0001](0001-tinyusb-cdc-framing-binario.md) | Transporte USB: TinyUSB CDC-ACM con framing binario propio | Aceptada |
