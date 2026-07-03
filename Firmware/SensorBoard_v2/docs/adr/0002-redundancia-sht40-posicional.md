# 0002. Redundancia SHT40 expuesta como arrays posicionales; fusión en la motherboard

**Estado:** Aceptada
**Fecha:** 2026-07-03

## Contexto

El hardware monta **tres** SHT40 (dos en el bus I2C de temperatura IO41/IO42, uno en el bus principal IO4/IO5) para redundancia térmica en un dispositivo médico: el control del calefactor/humidificador de la incubadora depende de estas lecturas. El roadmap original asumía un único sensor (`{"temp":37.5,"hum":60.2}`); hay que decidir cómo se publica la redundancia y quién fusiona.

## Decisión

El SensorBoard publica las tres lecturas **sin fusionar**, como arrays posicionales (`"temp":[t0,t1,t2]`, `"hum":[h0,h1,h2]`), con `null` en la posición de un sensor en fallo (NACK/timeout/CRC). La fusión (mediana, votación 2-de-3, detección de deriva) es responsabilidad de la motherboard, que es quien cierra el lazo de control.

## Consecuencias

- La motherboard ve fallos y discrepancias entre sensores — imprescindible para votación 2-de-3 y para alarmas de deriva; una fusión en origen las ocultaría.
- El contrato posicional (índice = sensor físico: 0=bus temp 0x44, 1=bus temp 0x46, 2=bus principal 0x44) queda fijado en el spec `env-sensors` y documentado en `docs/hardware.md`.
- El payload crece (~120 B), sigue < 256 B.
- El parser de la motherboard debe aceptar `null` en las posiciones.

## Alternativas consideradas

- **Fusionar en el SensorBoard (mediana)** — oculta discrepancias y fallos parciales al sistema que toma decisiones clínicas. Descartada.
- **Un evento por sensor** — triplica frames y obliga a la motherboard a correlacionar por timestamp. Descartada.
