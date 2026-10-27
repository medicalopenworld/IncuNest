# Motherboard IncuNest — registro de cambios

Este archivo documenta los cambios de hardware de la Motherboard, los cambios de firmware que exige cada versión y los cambios planificados para la siguiente.

Convenciones:
- **LCSC_PN** es la referencia válida de cada componente. Description, Comment y MPN pueden estar desactualizados.
- Las coordenadas son las de Altium (iguales a las del ODB++), en mm.
- Fabricación y montaje en JLCPCB. Diseño en Altium Designer 24.5.2.
- La revisión automática se hace con `tools/` (ver `CLAUDE.md`, sección "Herramientas").

---

## [V19.A] — 25/10/2026 · tanda de validación (50 uds)

**Estado: solo validación de ingeniería (banco, ensayos térmicos, firmware). No apta para uso clínico.** Faltan el corte del calefactor independiente del firmware (IEC 60601-2-19) y otros puntos listados en [Limitaciones conocidas](#limitaciones-conocidas-de-v19a).

- Ficheros de fabricación: `Project Outputs for IncuNest/V19_A/` (Gerber X2 + taladros, ODB++, BOM, Pick & Place).
- Revisión: `exports/revision_2026-10-25.txt`. Stock: `exports/stock_50uds_2026-10-25.txt`.
- Configuración recomendada para el pedido a JLCPCB: 4 capas, 1,6 mm, stackup JLC04161H-7628 (1 oz exterior / 0,5 oz interior), ENIG, vía mínima de 0,2 mm, montaje en las dos caras. Anotar aquí la configuración final y el número de pedido.

### Alimentación y carga (BQ25730)

| # | Cambio | Motivo / detalle |
|---|---|---|
| 1 | El BQ25730 queda alimentado desde la batería cuando no hay adaptador | Verificado: el chip se alimenta del mayor entre VBUS y SRN (J9 → R59 → R62 → U11-19), arranca solo y enciende el BATFET Q13. Con solo batería entra en bajo consumo (~22–45 µA) y mantiene el I²C activo. |
| 2 | C108 = 10 µF 0805 50 V en el pin VBUS (pin 1), después de R60 (1 Ω) | Filtro RC recomendado por TI en VBUS. |
| 3 | C57 corregido (entre SRP y SRN) | El filtro R61/R62/C57 se queda junto al shunt R59, como en el datasheet y el BQ2573xEVM. |
| 4 | C67 corregido; compensación para 800 kHz / 3,3 µH | R71 = 16k9, C67 = 3,3 nF, C60 = 1,2 nF (R66 = 15k, C65 = 33 pF, C62 = 15 pF). |
| 5 | R69 = 294k (C327357) en ILIM_HIZ | Límite de entrada por hardware de 8 A (V = 1 V + 40 × IDPM × RAC). |
| 6 | CHRG_OK conectado a IO42 con pull-up R26 = 10k (C190095) a 3v3_dig | Salida en drenador abierto. Antes estaba flotante. Permite detectar el adaptador desde el firmware. |
| 7 | Eliminados NT1 y la regla ShortCircuit PGND–DGND | El pin 27 (PGND) de U11 va directo a DGND. Verificado en el ODB++: sin nets abiertas. |
| 8 | Pad expuesto de U11: 5 vías de 0,3/0,5 mm en cruz, tapadas en top; pasta en 4 ventanas de 1 × 1 mm (pad −1,45 mm); máscara de los pines 0,04 mm y del EP 0 mm | Evitar huecos de soldadura y puentes en el WQFN de 0,4 mm. |
| 9 | Vías DGND junto a los source de Q11 y Q12 (3 a ≤0,6 mm y 5 a ≤1 mm) | Retorno corto del lazo de conmutación. |
| 10 | Áreas de SW1/SW2 reducidas a 84,8/102,3 mm² (antes 120/130) | Menos EMI. El objetivo de 50–60 mm² queda para V20. |
| 11 | U11 con LCSC_PN C5219071 restaurado | Se había perdido al actualizar la huella. La pieza viene de la librería privada de JLC. |

### Entradas y protecciones

| # | Cambio | Motivo / detalle |
|---|---|---|
| 12 | **Protección contra inversión de batería, nueva:** Q8 = BSC010N04LS6ATMA1 (C7220836) en el negativo, con el drain en J9-1 (net propia) y el source en DGND; R32 = 73k2 de BAT+ al gate; D2 = BZT52C15S (C17283) de gate a DGND; R33 = 470k de BAT+ a DGND; D13 (SMBJ18CA) de BAT+ a DGND | La nota de V18 decía "quitar la protección de polaridad inversa de batería"; el motivo de aquella retirada no está documentado. La nueva conduce en los dos sentidos cuando está encendida, así que permite cargar. Con la batería al revés, Vgs ≈ 0 (R33 mantiene BAT+ a DGND) y el diodo de cuerpo bloquea. Pierde ~0,14 W a 12 A. |
| 13 | **C109 = 100 µF 50 V electrolítico (Panasonic EEEFK1H101P, C178548) en V_IN_PROTECTED** (top, (11,94; −30,61)) | Amortigua la resonancia del cable de entrada (1–3 µH) con los ~40 µF cerámicos al enchufar en caliente. Sin él, el pico puede llegar a 2 × Vin (~48 V con 24 V), por encima de los 32 V de máximo absoluto del BQ. Su ESR de ~0,3 Ω hace de resistencia. Cubre J8 y J11. |
| 14 | D10 = SMBJ28CA (C78417) en J8 | No conduce con sistemas de 24 V ni con arranques a 28 V. No protege frente a un load dump (ver V20). |
| 15 | Snubber de J11: R50 ∥ R51 = 3R9 1206 0,5 W (C2759145) + C40 1 µF | Cambio de LCSC_PN por stock. Por sí solo apenas amortigua (Cd ≪ Cin); lo complementa C109. |
| 16 | Taladros de montaje unidos a DGND | El de (111,5; −64,3) no tenía net. |

### Actuadores

| # | Cambio | Motivo / detalle |
|---|---|---|
| 17 | U18 (UCC27524AD): calefactor = IO45 → R22 → INB → OUTB → R16 → Q2; fototerapia = IO13 → R9 → INA → OUTA → R29 (4,99 Ω) → Q5 | La nota de V18 decía "FET del pulsioxímetro"; según el esquema es el de **fototerapia (Q5)**, que pasa a excitarse con el UCC27524AD. VDD desde DC_ACTUATORS con C104 10 µF + C106 100 nF. R26 (pull-down) eliminado: INA e INB tienen 120k internos, y R3 (10k) mantiene IO45 a 0. |
| 18 | **Shunt del calefactor R46 = 2 mΩ** (Vishay WSLP12062L000FEA, C844294) con Kelvin rehecho | Menos disipación a 10 A (0,2 W). **Causa raíz de la "lectura negativa del calefactor" de V17/V18:** IN+ del INA3221 estaba conectado al nodo de conmutación del MOSFET, no al pad del shunt. En V19 el sense sale de los pads de R46 (verificado). |
| 19 | Circuito del FET del ventilador (Q4 = AO3400A, R14/R15) corregido | FAN legacy. Sigue sin diodo de flyback (V20). |

### Sensores y AFE4490

| # | Cambio | Motivo / detalle |
|---|---|---|
| 20 | Referencia del termistor de piel: LM4040AIM3X-2.5/NOPB (C111866), **2,500 V grado A (0,1 %)**, con R83 = 470 Ω, alimentada desde IO18 | Antes era LM4040EIM7-2.0 (2,048 V, grado E, 1 %). **Cambia la tensión de excitación: el firmware tiene que usar 2,500 V.** |
| 21 | Protecciones de TX del AFE corregidas: D4/D5 limitan a 5V@2A; D6/D7 = BAV199 (C2934850) | |
| 22 | Eliminados los stubs del AFE4490 y corregidos los condensadores del cristal (C6/C10 = 27 pF) | |
| 23 | Apagado del AFE: AFE_PDN en IO7 (R24) | Reset por software + PDN; RESETZ solo con pull-up (R25). |
| 24 | Desacoplo de RX_ANA_SUP: C101 y C102 (100 nF) a 0,42 mm de los pines 33 y 39, y C100 (1 µF), cada uno con vía a DGND | El AFE no tenía desacoplo local en la alimentación analógica de RX. |
| 25 | Desacoplo de la alimentación TX y del driver de LED (C12/C13/C15, 5V@2A) unido a DGND | Durante el diseño de V19 su pin de masa se desconectó y se recuperó antes de fabricar. |
| 26 | Canal 3 de U8 (INA3221) a GND | Canal sin uso. Ya no se mide la corriente de batería con el INA. |

### Alimentación auxiliar y comunicaciones

| # | Cambio | Motivo / detalle |
|---|---|---|
| 27 | R79 = 45k3: 3V3 = 3,32 V (antes 3,42 V) | |
| 28 | Apagado del SIM800C: EN de U13 (4V@2A) en IO39, con pull-down R31 = 73k2 | **El módem arranca apagado.** El firmware tiene que poner IO39 a nivel alto. |
| 29 | EN de U12 (5V@2A) en IO40, con pull-down R30 = 73k2 | **El raíl de 5 V arranca apagado.** De él cuelgan el display, la sensorboard (VBUS de J14), el driver de LED del AFE, el LDO U15 (3v3_ANA: RX del AFE y ADS1110 de la temperatura de piel) y el humidificador (VUSB). |
| 30 | Pull-down R27 en USB_EN (IO5) | El humidificador (TPS2553 → VUSB) arranca apagado. |
| 31 | R38 = 2k26 (antes 1k) | RXD del SIM800 a 2,69 V. |
| 32 | Condensadores de salida de los TPS563231: C69–C72, C79, C80, C88 y C89 = 22 µF 0805 25 V (Samsung CL21A226MAQNNNE, C45783) | TI pide 20–68 µF efectivos. Los de 10 µF 0402 bajaban a ~3 µF a 5 V. Bobinas L2–L4 = 4,7 µH (C514864), dentro de la tabla de TI. |
| 33 | C105 (reserva del VBAT del SIM800) = 100 µF 10 V tántalo caja C (Vishay 293D107X9010C2TE3, C136005) | Antes de 6,3 V: trabajaba al 63 % de su tensión nominal, y la regla para tántalo es el 50 %. Va en el lado de masa, junto a la alimentación de la PIFA, sin efecto en la antena. |
| 34 | ESP32-S3-WROOM-1-N8 → **N8R2** (C2913204) | PSRAM quad de 2 MB; IO35–37 siguen libres para el SPI del AFE. Pinout idéntico. |

### Layout y DFM

| # | Cambio |
|---|---|
| 35 | Vías de potencia de 0,8/1,2 mm: Q7/Q14 → R48 (6 vías, ≈16 equivalentes de 0,3 mm) y R49 → Q1 (8 vías, ≈19 equivalentes). |
| 36 | Shunt de V_SYS R49 = 2 mΩ (C844294); Kelvin de U7-12 corregido. |
| 37 | Stackup corregido en Altium: 35 µm / 8,284 mil / 17,5 µm / 41,929 mil / 17,5 µm / 8,284 mil / 35 µm. |
| 38 | Máscara: expansión global de 2 mil; Minimum Solder Mask Sliver de 0,1 mm (U11: 0,05 mm); todas las vías libres tapadas. |

### Sensorboard
- USB corregido (ver su propio proyecto).

### Verificación realizada (ODB++ del 25/10, 20:31)
- **Esquema = PCB.** Sin fusiones de nets no intencionadas. Sin nets abiertas. Único corto: el de la PIFA (intencionado).
- **Min-cut de potencia** (objetivo ≥4,7 mm equivalentes a 10 A):

  | Ruta | Resultado |
  |---|---|
  | J11 → Q7 | 5,4 mm |
  | J8 → Q14 | 7,4 mm |
  | J9 → R59 | 7,5 mm |
  | Q13 → R59 | 6,2 mm |
  | Q7/Q14 → R48 | vías de 0,8 mm (OK) |
  | R49 → Q1 | vías de 0,8 mm (OK) |
  | **Vsense+ → R49** | **3,9 mm** |
  | **Rutas del calefactor** | **2,54 mm** |

- **Kelvin correcto:** R46, R48, R59, R17 y U7-12 → R49.
- **Stock para 50 placas:** completo, salvo U11 (librería privada).

### Limitaciones conocidas de V19.A
Conviene medirlas en la validación; todas pasan a V20.

1. **Sin corte del calefactor independiente del firmware.** Si el ESP32 se cuelga con IO45 en alto, o Q2 falla en corto, solo queda Q1, que también controla el ESP32.
2. **R18 (0 Ω) ata DC_DC_EN a V_SYS:** con batería conectada el equipo no se apaga con el pulsador.
3. **Rutas del calefactor de 2,54 mm** (J3-1 → R46 → Q2 y Q1 → J3-2) a 10 A, y Vsense+ → R49 en 3,9 mm.
4. **R59 (1206 1 W) a 12 A:** 0,72 W. J9 (C8445) es de 10 A frente a los ~12 A del calefactor desde batería.
5. **Térmico:** Q2 y Q1 tienen 1 vía térmica cada uno y Q13 ninguna. El EPAD del ESP32 no tiene vías.
6. **Ventilador:** tacómetro (IO38) y PWM (IO11) directos al conector, y FAN− sin flyback.
7. **USB J14:** VBUS directo a 5V@2A, sin ESD en D+/D−, CC sin resistencias. J13 tampoco tiene Rp.
8. **IO6 (USB_FAULT):** sin pull-up externo; se usa el interno.
9. **Holgura entre los bornes de batería:** 0,2 mm en bottom (pad de J9-1 frente al polígono BAT+) y 0,13 mm en top.
10. **Sense del BQ:** ACN a 0,54 mm de SW1; SRN cruza SW2 por L3.
11. **J8:** sin protección frente a load dump.

### Pruebas de validación recomendadas
- **Cámara térmica** con el calefactor a 10 A durante 30 min, desde J11 y desde batería: Q1, Q2, Q13, R46, R49, R59, J9 y las pistas del calefactor.
- **Enchufar en caliente** J11 (24 V) y J8 con el cable real, midiendo V_IN_PROTECTED (disparo único, 10 µs/div). Criterio: pico <30 V.
- **Inversión de batería:** conectar la batería al revés, con y sin adaptador, y comprobar que no circula corriente y que el BQ sigue vivo.
- **Arranque solo con batería:** el BQ enciende Q13 y el I²C responde.
- **Ventilador real:** medir la tensión en IO11 y en IO38 con el ventilador conectado. Si el PWM del ventilador tiene pull-up a 5 V, el pin queda fuera de especificación.
- **SIM800 en transmisión:** comprobar si los picos de 217 Hz aparecen en la señal del AFE, y medir el RSSI (`AT+CSQ`).

---

## Cambios de firmware para V19 (`Firmware/motherBoard`)

Hoy `board.h` solo admite `HW_NUM` 16 y 17, y `platformio.ini` no tiene entorno para V18 ni V19. Propuesta: `HW_NUM = 19`, con un bloque de pines propio para `HW_NUM >= 19`, y revisar todos los `#if (HW_NUM == 17)`.

### 1. Configuración del proyecto
- [ ] `platformio.ini`: entorno `[env:IncuNest_V19]` con `-DHW_NUM=19`. El N8R2 tiene flash de 8 MB (sirve `partitions/ESP32S3_8MB.csv`) y PSRAM **quad**. Si se usa PSRAM: `board_build.arduino.memory_type = qio_qspi` y `-DBOARD_HAS_PSRAM`. **No usar OPI**, que ocuparía IO35–37 (el SPI del AFE).
- [ ] `board.h`: quitar el `#error` que solo admite 16/17 y subir `FWversion`.
- [ ] Consola de depuración por UART0 (J1: DEBUG_TX/RX, EN, IO0), porque D+/D− van a la sensorboard: `ARDUINO_USB_CDC_ON_BOOT=0`.

### 2. Pines (`board.h`, bloque `HW_NUM >= 19`)

| Señal | GPIO | Antes (bloque HW ≥16) | Notas |
|---|---|---|---|
| `AFE44XX_PWDN_PIN` | **7** | `FAKE_PIN` (IO7 era `AFE_LED_ALM`) | AFE_PDN: activo bajo. Quitar `AFE_LED_ALM`. |
| `PWR_5V_EN` (nuevo) | **40** | — | EN de U12. Pull-down de 73k2: **ponerlo a nivel alto en el arranque**. |
| `GSM_PWR_EN` (nuevo) | **39** | — (`GPRS_PWRKEY` = `FAKE_PIN`) | EN de U13 (4V del SIM800). Pull-down de 73k2. Activo alto. |
| `CHRG_OK` (nuevo) | **42** | — | Entrada; pull-up externo de 10k. Alto = adaptador válido. |
| `BABY_TEMP_EN` | 18 | 18 | Ahora alimenta el LM4040 de 2,5 V a través de R83. |
| `USB_EN` / `USB_FAULT` | 5 / 6 | 5 / 6 | Sin cambios. `USB_FAULT` ya usa `INPUT_PULLUP` (`initHardware.cpp`), necesario porque no hay pull-up externo. |
| `HEATER` / `PHOTOTHERAPY` | 45 / 13 | 45 / 13 | Sin cambios (IO45 → INB, IO13 → INA del UCC27524). |
| `I2C2_SDA/SCL` | — | 20 / 19 | **No inicializar `Wire1` en IO19/IO20 en V19:** son D−/D+ del USB hacia la sensorboard. |
| Libres | 41, 3, 46 | — | IO41 reservado para el WDI del TPS3431 (V20). IO46 tiene un pull-down de 10k (strapping): solo salida después del arranque. |

### 3. Secuencia de arranque
1. `PWR_EN` (IO2) a nivel alto lo antes posible (latch de encendido por D14).
2. `PWR_5V_EN` (IO40) a nivel alto. Esperar unos 5 ms (arranque suave de 1,5 ms del TPS563231 más el LDO U15).
3. Inicializar el I²C (BQ25730, INA3221 ×2, ADS1110). El ADS1110 depende de 3v3_ANA, así que no responde sin el paso 2.
4. AFE: `AFE44XX_PWDN_PIN` a nivel alto, reset por software y configuración. El driver de LED necesita 5 V.
5. Módem: `GSM_PWR_EN` (IO39) a nivel alto solo cuando haga falta. Apagarlo poniéndolo a nivel bajo.
6. Humidificador: `USB_EN` (IO5) como ahora.

### 4. Medidas de corriente y temperatura (`board.h`, `sensors_module.cpp`)
- [ ] **Shunts:**

  | Constante | V19 | Antes (HW17) | Notas |
  |---|---|---|---|
  | `HEATER_SHUNT` | **2** | 5 | R46 |
  | `SYSTEM_SHUNT` | **2** | 1000 | R49; en V17 el sense estaba antes del O-ring |
  | `PHOTOTHERAPY_SHUNT` | 5 | 5 | R17 |
  | `FAN_SHUNT` | 100 | 100 | R13 |
  | `USB_SHUNT` | 100 | 100 | R19 |

- [ ] **Quitar `HEATER_CURRENT_CORRECTION_FACTOR`** y la inversión de signo para V19. El Kelvin de R46 ya está corregido. Hoy solo se aplica con `HW_NUM == 17`; comprobar que no se extiende a 19.
- [ ] **Batería:** el canal 3 de U8 está a GND. No leer `BATTERY_SHUNT_CHANNEL`; la corriente y la tensión de batería salen del ADC del BQ25730 (IBAT/IDCHG y VBAT, registros 0x28/0x2C).
- [ ] **Termistor de piel:** en `adcToCelsius()`, `vExc` = **2,500 V** para `HW_NUM >= 19` (hoy, 2,048 V para HW ≥17). `rTop` = 2260 Ω (R44) sin cambios. Recalibrar contra una referencia.
- [ ] Revisar los `#if (HW_NUM == 17)` de `EEPROM.cpp` (`heaterMaxPowerAmps`) y `sensors_module.cpp`, y decidir si aplican a V19.

### 5. BQ25730 (`drivers/BQ25730.cpp`)
- [ ] **ChargeVoltage (0x04):** 14,4 V para carga cíclica de AGM, o 13,6–13,8 V en flotación. **Nunca >15 V:** V_SYS alimenta a los TPS563231, de 17 V máximo, y el gate de Q8 tiene un zéner de 15 V.
- [ ] **ChargeCurrent (0x02):** ≤0,25 C para la AGM de 9 Ah (≈2 A).
- [ ] **PWM_FREQ** (ChargeOption0, bit 9) = 0 (800 kHz). Escribirlo y **leerlo de vuelta**: la compensación y L1 están calculadas para 800 kHz.
- [ ] **RSNS_RAC = RSNS_RSR = 1** (5 mΩ, valor de POR). R48 y R59 son de 5 mΩ; **no cambiar a 2 mΩ**, el BQ solo admite 5 o 10 mΩ.
- [ ] **IIN_HOST (0x0E) según la fuente**, y **reescribirlo en cada flanco de subida de CHRG_OK.** El BQ lo devuelve a 3,25 A al quitar el adaptador y no lo restaura al volver a enchufar; con 3,25 A el calefactor tiraría de la batería estando enchufado. Valores: adaptador de 24 V (VBUS >18 V): 8 A (ILIM_HIZ también limita a 8 A); coche (VBUS <16 V): ≤8 A.
- [ ] **VINDPM (0x0A):** no cargar la AGM si VBUS <12,5 V (entrada de coche con el motor parado).
- [ ] **Watchdog del BQ** (WDTMR_ADJ, ChargeOption0 bits 14:13; 175 s en POR): refrescar ChargeCurrent/ChargeVoltage periódicamente o desactivarlo. Si caduca, la corriente de carga pasa a 0.
- [ ] **Carga solo con VBAT válida (10,5–15 V)**, usando CHRG_INHIBIT. CELL_BATPRESZ está fijo (R64/R7), así que el BQ siempre cree que hay batería: la ausencia de batería se detecta por VBAT.
- [ ] **Con solo batería**, si se quieren el ADC/IBAT del BQ: EN_LWPWR = 0 (modo rendimiento). Volver a 1 al apagar.
- [ ] Leer CHRG_OK (IO42) para saber la fuente y mostrarla en la UI y en la telemetría.

### 6. Límite de potencia del calefactor según la fuente (`security.cpp` / control)
Con el calefactor a 10 A y el resto del sistema a ~2 A, V_SYS llega a ~12 A. En V19 hay cuellos de botella (J9 de 10 A, R59 en 1206, pistas de 2,54 mm), así que hay que limitar:

| Fuente (CHRG_OK, VBUS) | Duty máximo del calefactor | Motivo |
|---|---|---|
| Adaptador de 24 V (CHRG_OK = 1, VBUS >18 V) | 100 % | 8 A × 24 V ≈ 190 W disponibles |
| Coche (CHRG_OK = 1, VBUS <16 V) | **~50 %** | 12 V × 8 A ≈ 96 W; por encima, el BQ completa desde la batería |
| Solo batería (CHRG_OK = 0) | **~40 %** | Corriente eficaz de batería ≲8 A por J9, R59 y Q13 |

- [ ] Además, vigilar la corriente de sistema (U7, canal 1) y recortar el duty si pasa de 10 A sostenidos.

### 7. Ventilador
- [ ] **PWM en IO11:** configurarlo como drenador abierto (`gpio_set_direction(FAN_CTL, GPIO_MODE_OUTPUT_OD)` después de `ledcAttachPin`). La entrada PWM de un ventilador de 4 hilos suele tener pull-up interno a 5 V, y en push-pull el pin queda por encima de 3,6 V.
- [ ] **Tacómetro en IO38:** `INPUT_PULLUP` (pull-up interno a 3,3 V). Comprobar que el ventilador no lleva pull-up propio a 12 V.

### 8. Sensorboard por USB
- [ ] La sensorboard V2 es un dispositivo USB CDC (TinyUSB). La Motherboard tiene que actuar como **host USB** (ESP32-S3 OTG, driver CDC-ACM host) y sustituir el bus I2C2 que se usaba en IO19/IO20 en HW16/17.

### 9. Pendientes de firmware ya existentes
- [ ] Bajo consumo en apagado: INA3221 en power-down y ADS1110 en single-shot. Con IO40 a nivel bajo se apagan el display y la sensorboard.

---

## [V20] — planificado

Prioridades: **C** = crítico (seguridad o normativa), **A** = alto, **M** = medio, **B** = bajo.

### Seguridad y normativa
| P | Cambio | Detalle |
|---|---|---|
| C | **Corte térmico del calefactor independiente del firmware** (IEC 60601-2-19, cl. 201.15.4.2.1 aa) y bb)) | Tres capas, cada una capaz de cortar Q1 sin intervención del ESP32. (1) Bimetálico KSD9700 NC en el conjunto calefactor, en serie entre el drain de Q3 y el gate de Q1, con R8 = 2k2 1206 y conector JST XH de 2 pines. (2) Comparador LMV393 con NTC propia (Semitec 103AT-11; R_b 6,04k 0,1 %, referencia 10k/10k 0,1 %, detección de sonda abierta 100k/13k, histéresis 1 MΩ): su salida corta el gate de Q3 a través de un BAT54 y avisa en IO3. (3) Watchdog TPS3431SDRBR (C702145): WDI en IO41, WDO → BAT54C → gate de Q3 y ESP32_EN, y el zumbador suena. |
| C | Conector hacia la sensorboard con señales extra para la NTC y el comparador analógico | Permite llevar la sonda del corte térmico independiente (nota de V18, punto 3). |
| A | Latch de encendido | Quitar R18 y añadir el OR de "adaptador presente" (V_IN_PROTECTED → 68k → BAT54 → DC_DC_EN). Con batería, el equipo tiene que poder apagarse. |
| A | Protección de J8 frente a load dump | LM74800-Q1 (C3215600) con dos MOSFET de 60 V en antiserie y corte por sobretensión a ~29 V; D10 → SMBJ33CA. |
| A | Detección de presencia de batería | Contacto o divisor que lleve CELL_BATPRESZ a nivel bajo sin batería. Hoy está fijo con R64/R7. |
| M | Holgura entre BAT+ y BAT− ≥0,5 mm | Hoy es de 0,2 mm en bottom (pad de J9-1) y 0,13 mm en top. |

### Potencia y térmico
| P | Cambio | Detalle |
|---|---|---|
| A | R59 → 2512 5 mΩ 3 W (RLP25FEGMR005, C393074) | Mismo valor (el BQ solo admite 5 o 10 mΩ); cambia la huella. Rehacer el Kelvin. |
| A | J9 → MX2EDGVC-7.62-02P de 15 A (C19724644, como J8 y J11) | Cambia la huella. |
| A | Rutas del calefactor ≥4,7 mm equivalentes | J3-1 → R46 → Q2, source de Q2 → GND, Q1 → J3-2. Polígono o pistas en las dos caras con vías. También Vsense+ → R49. |
| A | Vías térmicas y cobre en bottom | Q2 y Q1: 8–12 vías bajo la pestaña. Q13: ≥300 mm² de NetC56_2 con vías. |
| A | EPAD del ESP32 | 9–16 vías a GND (al menos 4 en los huecos entre pads). |
| M | Kelvin de R49 (U7-11) y separación de los sense del BQ | U7-11 se une al polígono a 0,65 mm del pad. ACN/SRP/SRN a ≥2 mm de SW1/SW2, y SRN sin cruzar SW2 por L3. |
| M | Etapa de potencia del BQ | SW1/SW2 a 50–60 mm²; C44–C47 en fila junto a Q9/Q12 (bucle de entrada <3 mm). |
| B | Q1 → R17 (fototerapia) | 1 vía basta a 0,7 A; añadir otra por redundancia. |

### Interfaces
| P | Cambio | Detalle |
|---|---|---|
| A | Ventilador (J5) | 1k en serie con el tacómetro (IO38), 2N7002 en el PWM (IO11) y diodo de flyback en FAN− (D3 con el cátodo a FAN+), o eliminar el FAN legacy (Q4/R13/R14/R15). |
| A | USB J14 | Decidir el papel (host hacia la sensorboard o device para programar). Si es host: TPS2553 en VBUS, Rp de 56k en CC1/CC2. Si es device: Rd de 5k1. USBLC6-2SC6 (C7519) en D+/D−. J13: Rp de 56k a VUSB. |
| M | IO6 (USB_FAULT) | Pull-up externo de 10k a 3v3_dig. |
| M | Bajo consumo en apagado | Con IO40 ya se cortan el display y la sensorboard. Revisar el resto de consumos con el equipo apagado. |
| B | Nivel lógico del display (J10) | Pregunta abierta. |

### Layout, DFM y documentación
| P | Cambio |
|---|---|
| M | L3 sin pistas bajo el termistor ni bajo el ESP32; DGND en L3 bajo los tramos USB en bottom. |
| M | Reglas Silk to Mask y Silk to Silk ≠ 0 (hoy hay 57 elementos en top y 110 en bottom a <0,15 mm de una apertura). |
| B | Vías de 0,2 mm (20) → 0,3 mm, para no pagar la opción de 0,2 mm. |
| B | Tapar las vías térmicas de U4, U7 y U8 en la cara opuesta. |
| B | Prepregs con εr 4,4 en Altium (JLC 7628). |
| B | Comment y MPN coherentes con el LCSC_PN de todos los shunts (`tools/altium/UpdateLCSC.pas`): R46/R49 "0.002R 1206 1W" WSLP12062L000FEA; R17/R48/R59 "0.005R 1206 1W" PA1206FRE470R005Z. |
| B | Etiquetas de net con nombre (CHRG_OK, BAT−, 5V_EN, MODEM_PWR_EN) en lugar de NetR26_1, NetJ9_1, etc. |

### Futuro (más allá de V20)
- Migrar el SIM800C (2G) a un módulo LTE.
