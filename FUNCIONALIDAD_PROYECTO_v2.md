# IncuNest — Hoja de Requisitos del Sistema
### Versión 2 — Especificación funcional orientada a implementación

> **Propósito**: Definir qué debe hacer el sistema IncuNest, cómo debe comportarse y qué garantías debe ofrecer. Este documento es independiente de cualquier implementación concreta y está pensado para que pueda servir de base en el desarrollo del sistema desde cero o en su refactorización.

---

## 1. Descripción del Producto

IncuNest es una **incubadora neonatal** de bajo coste destinada a entornos hospitalarios con recursos limitados. Su objetivo es proporcionar un ambiente térmico y húmedo controlado para recién nacidos prematuros o de bajo peso, con capacidad de fototerapia, monitorización continua y conectividad remota.

El sistema debe ser:
- **Seguro**: el control y la seguridad nunca dependen de la interfaz de usuario
- **Robusto**: debe seguir funcionando aunque falle la pantalla o la conectividad
- **Accesible**: coste de fabricación inferior a 400 €
- **Mantenible**: actualizable por software sin abrir el dispositivo
- **Auditable**: todos los eventos y mediciones deben poder registrarse remotamente

---

## 2. Arquitectura Requerida

### 2.1 Separación de responsabilidades

El sistema debe estar compuesto por **dos módulos electrónicos independientes**:

1. **Módulo de control** (placa madre): responsable del control en tiempo real, la seguridad, los sensores y los actuadores. Debe funcionar de forma autónoma sin necesidad de la interfaz de usuario.
2. **Módulo de interfaz** (HMI): responsable exclusivamente de mostrar información y recibir entrada del usuario. No debe tomar decisiones de control ni de seguridad.

### 2.2 Comunicación entre módulos

- Los dos módulos deben comunicarse mediante un **protocolo serie** (UART) a través de un enlace físico dedicado.
- La comunicación debe ser **bidireccional**: el módulo de control envía telemetría y el módulo HMI envía comandos del usuario.
- El protocolo debe tener un mecanismo de **sincronización de estado** que permita al HMI reconstruir el estado completo del sistema tras una reconexión.
- El módulo de control no debe enviar alarmas al HMI hasta que el HMI confirme que está listo para recibirlas.
- El protocolo debe incluir un mecanismo para que el HMI detecte y elimine alarmas obsoletas (p.ej. bitmask de alarmas activas en cada mensaje de estado).

### 2.3 Frecuencia de comunicación

- El módulo de control debe enviar telemetría al HMI al menos **una vez por segundo**.
- El HMI debe enviar comandos **solo cuando el usuario realice una acción**, no de forma periódica.

---

## 3. Requisitos de Hardware

### 3.1 Módulo de Control

- Microcontrolador de 32 bits con al menos **dos núcleos** para separar tareas de tiempo real de tareas de red.
- Soporte para **I2C, SPI, UART y ADC** en el mismo microcontrolador.
- Soporte para **WiFi y Bluetooth** integrados o mediante módulo externo.
- Memoria flash suficiente para al menos **dos particiones de firmware** (para OTA).
- Soporte para sistema operativo en tiempo real (**RTOS**).

### 3.2 Módulo HMI

- Microcontrolador de 32 bits con al menos **dos núcleos**.
- **Pantalla a color** de al menos 7 pulgadas con resolución mínima de 800×480 píxeles.
- **Pantalla táctil capacitiva** compatible con uso con guantes médicos (zonas táctiles grandes).
- **PSRAM** de al menos 8 MB para el framebuffer gráfico.
- Soporte para **altavoz** integrado o mediante conector de audio.
- Soporte para **WiFi** para actualizaciones OTA.

### 3.3 Sensores requeridos

| Función | Tecnología requerida | Redundancia |
|---------|---------------------|-------------|
| Temperatura del aire de la cabina | Sensor digital de alta precisión (I2C) | Mínimo 2 sensores |
| Temperatura ambiente exterior | Sensor digital temperatura + humedad (I2C) | 1 sensor |
| Humedad relativa de la cabina | Sensor digital (I2C) | 1 sensor |
| Temperatura de la piel del bebé | Termistor NTC analógico (ADC) | 1 sonda externa |
| Corriente y tensión de la alimentación | Sensor de corriente de alta precisión (I2C) | 2 nodos de medida |
| Velocidad del ventilador | Encoder de pulsos (GPIO) | 1 por ventilador |

> El sistema debe tolerar el fallo de un sensor de temperatura de cabina si hay un segundo disponible. En ese caso debe continuar operando y alertar del fallo.

### 3.4 Actuadores requeridos

| Actuador | Control requerido |
|----------|-----------------|
| Calefactor resistivo | PWM con feedback de corriente |
| Ventilador de circulación | PWM con feedback de RPM |
| Humidificador | PWM o protocolo I2C |
| Sistema de fototerapia (LEDs UV) | PWM con temporizador |
| Buzzer de alarma | PWM a frecuencia variable |

### 3.5 Gestión de energía

- El sistema debe monitorizar en tiempo real la tensión y la corriente de la fuente de alimentación principal.
- Debe soportar **batería de respaldo** con carga gestionada electrónicamente.
- El calefactor debe tener limitación dinámica de corriente para proteger la fuente de alimentación.

---

## 4. Requisitos de Control

### 4.1 Control de temperatura — Modo Aire

- El sistema debe mantener la temperatura del aire de la cabina en el **punto de consigna configurado por el usuario**.
- El control debe realizarse mediante un **bucle PID** con anti-windup.
- El rango de consigna aceptado es **30,0 °C a 38,5 °C**, en incrementos de 0,1 °C.
- El actuador de salida es el calefactor resistivo.

### 4.2 Control de temperatura — Modo Piel (Servocontrol)

- En modo piel, el sistema debe regular la temperatura de la **sonda dérmica del bebé** al punto de consigna.
- El rango de consigna aceptado es **35,0 °C a 37,5 °C**, en incrementos de 0,1 °C.
- El actuador de salida es el calefactor (igual que en modo aire).
- Este modo solo puede activarse cuando la sonda de piel esté **validada y conectada**.
- Si la sonda se desconecta o invalida durante la operación en este modo, el sistema debe:
  1. Emitir una alarma de forma inmediata.
  2. Cambiar automáticamente al modo aire.
  3. Mantener la consigna de temperatura de aire anterior (o una segura predefinida).

### 4.3 Control de humedad

- El sistema debe controlar la humedad relativa de la cabina mediante un **bucle PID independiente**.
- El rango de consigna aceptado es **20 % a 90 % HR**, en incrementos de 5 %.
- El actuador de salida es el humidificador.
- El control de humedad debe operar de forma independiente al control de temperatura.

### 4.4 Arranque y parada

- El sistema debe poder activarse y desactivarse (modo standby) desde la interfaz de usuario.
- En standby, todos los actuadores deben desactivarse pero la monitorización de sensores debe continuar.
- El último estado de funcionamiento (consignas, modo, etc.) debe persistir tras un ciclo de alimentación.

---

## 5. Requisitos de la Sonda de Piel

### 5.1 Detección

- El sistema debe detectar automáticamente si la sonda de piel está conectada o desconectada.
- La detección debe realizarse de forma periódica (al menos cada 5 segundos).
- Se debe aplicar un tiempo de debounce para evitar falsos positivos por contacto mecánico inestable.

### 5.2 Validación

- Una sonda conectada no se considera válida hasta que su lectura se estabilice dentro del rango fisiológico esperado.
- El sistema debe rechazar como no válidas las lecturas fuera del rango de temperatura corporal plausible.

### 5.3 Máquina de estados

La sonda debe gestionar los siguientes estados, con transiciones bien definidas:

```
NO_CONECTADA → PENDIENTE_VALIDACIÓN → VÁLIDA ↔ INVÁLIDA
                                          │
                                          ▼
                                DESCONECTADA_EN_USO
```

- `NO_CONECTADA`: No hay señal de la sonda.
- `PENDIENTE_VALIDACIÓN`: Sonda detectada, esperando estabilización.
- `VÁLIDA`: Lectura estable y dentro de rango. Permite activar modo piel.
- `INVÁLIDA`: Lectura inestable o fuera de rango.
- `DESCONECTADA_EN_USO`: Sonda se ha perdido durante operación en modo piel. Genera alarma.

### 5.4 Interacción con la interfaz

- El HMI debe reflejar el estado de la sonda en todo momento.
- El botón de selección del modo piel debe estar **deshabilitado** si la sonda no está en estado `VÁLIDA`.
- Si la sonda no está conectada, la HMI debe mostrar un mensaje informativo (no una alarma de error).

---

## 6. Fototerapia

- El sistema debe disponer de un sistema de LEDs UV para el tratamiento de ictericia neonatal.
- Debe soportar dos modos de operación:
  - **Continuo**: encendido hasta apagado manual.
  - **Temporizador**: apagado automático tras el tiempo configurado (rango: 1–120 minutos).
- El apagado automático debe gestionarse en el módulo de control, no en la interfaz.
- El HMI debe mostrar el tiempo restante en tiempo real.
- El tiempo transcurrido de fototerapia debe comunicarse al HMI en formato MM:SS.

---

## 7. Sistema de Alarmas

### 7.1 Tipos de alarma requeridos

El sistema debe detectar y notificar al menos los siguientes tipos de alarma:

| ID | Alarma | Condición de disparo |
|----|--------|---------------------|
| 1 | Alarma de humedad | Desviación superior a ±12 % respecto a la consigna |
| 2 | Alarma de temperatura | Desviación superior a ±1,0 °C respecto a la consigna |
| 3 | Corte térmico (aire) | Temperatura del aire superior a 38,5 °C absoluto |
| 4 | Corte térmico (piel) | Temperatura de la piel superior a 37,5 °C absoluto |
| 5 | Fallo de sensor de aire | Sin lectura válida del sensor de temperatura de cabina durante más de 20 s |
| 6 | Fallo de sonda de piel | Sonda desconectada durante uso en modo piel |
| 7 | Fallo del ventilador | Sin pulsos de RPM detectados estando el ventilador activo |
| 8 | Fallo del calefactor | Corriente anormalmente baja o nula con PWM activo |
| 9 | Fallo de alimentación | Tensión de alimentación principal fuera de rango |

### 7.2 Comportamiento de las alarmas

- Cada alarma debe tener un **umbral de disparo** y un **umbral de reset** diferentes (histéresis) para evitar oscilaciones.
- Las alarmas de tipo "fallo de hardware" (cortes térmicos, fallo de sensor, fallo de ventilador, etc.) deben **desactivar todos los actuadores** de forma inmediata.
- Las alarmas se deben comunicar al HMI tanto al producirse como mediante un bitmask en cada mensaje de estado periódico.
- El HMI debe auto-eliminar alarmas que el bitmask del módulo de control no incluya (protección contra alarmas fantasma).

### 7.3 Retardo de ignición

- En el arranque en frío, las alarmas de desviación (temperatura, humedad) deben suprimirse durante un período configurable (~30 minutos) para permitir que el sistema alcance el punto de consigna sin generar falsas alertas.

### 7.4 Notificación al usuario

- Toda alarma activa debe generar una **alerta visual** en la pantalla (siempre, sin excepción).
- Toda alarma activa debe generar una **alerta sonora** mediante el buzzer.
- El usuario debe poder **silenciar el sonido** de la alarma activa sin suprimir la alerta visual.
- El silencio debe anularse automáticamente si se activa una nueva alarma distinta.

### 7.5 Audio del sistema

- El buzzer debe emitir tonos diferenciados para:
  - Alarma activa
  - Confirmación de acción del usuario
  - Estado de standby (señal periódica suave)
- Los tonos deben controlarse mediante PWM a frecuencia variable.

---

## 8. Calibración de Sensores

### 8.1 Calibración de dos puntos (manual)

- El sistema debe permitir una calibración de dos puntos para los sensores de temperatura (aire y piel).
- El proceso requiere introducir dos temperaturas de referencia conocidas y registrar las lecturas brutas del sensor en cada punto.
- La corrección debe aplicarse de forma lineal: `Temperatura_corregida = m × Lectura_bruta + b`

### 8.2 Auto-calibración

- El sistema debe ser capaz de realizar una calibración automática cuando alcance el estado estacionario en la consigna de aire.
- La auto-calibración debe comparar la lectura del sensor con la referencia calculada por la calibración de dos puntos y ajustar los offsets de forma incremental.

### 8.3 Ajuste fino (fine-tune)

- El sistema debe permitir ajustes incrementales de ±0,5 °C sobre la calibración existente.
- Estos ajustes deben poder realizarse desde la interfaz de usuario.

### 8.4 Persistencia

- Todos los datos de calibración deben almacenarse en memoria no volátil y sobrevivir a ciclos de alimentación.

---

## 9. Persistencia de Estado (Memoria No Volátil)

El sistema debe almacenar de forma persistente al menos:

- Estado de activación (encendido/apagado)
- Modo de control activo (aire o piel)
- Consignas de temperatura (aire y piel)
- Consigna de humedad
- Estado y configuración de fototerapia (activo, duración)
- Datos de calibración de todos los sensores
- Idioma seleccionado
- Credenciales WiFi
- Credenciales de la plataforma IoT (token de dispositivo)
- Número de serie y versión de hardware
- Contadores de horas de operación por modo y por actuador
- Configuración de audio (volumen)
- Preferencias de interfaz (modo oscuro, retroiluminación)

---

## 10. Conectividad y Monitorización Remota

### 10.1 WiFi

- El sistema debe soportar conectividad WiFi 802.11 b/g/n (2,4 GHz).
- Las credenciales WiFi deben configurarse desde la interfaz de usuario y persistir en memoria.
- El WiFi debe usarse para actualizaciones de firmware OTA y para envío de telemetría a la plataforma IoT.

### 10.2 GPRS/GSM (Celular)

- El sistema debe soportar conectividad celular mediante un módulo GPRS externo como canal alternativo al WiFi.
- Debe soportar APN configurable.
- Debe soportar actualizaciones OTA también por esta vía.

### 10.3 Plataforma IoT (ThingsBoard)

- El sistema debe publicar telemetría a una plataforma IoT mediante **protocolo MQTT**.
- Los datos publicados deben incluir al menos: temperaturas (aire, piel, ambiente), humedad, consignas activas, estado de alarmas, tiempos de operación acumulados y estado de la batería/alimentación.
- La frecuencia de publicación debe adaptarse al modo de operación:
  - Standby: baja frecuencia (p.ej. cada hora)
  - Control activo: frecuencia media (p.ej. cada minuto)
  - Fototerapia activa: frecuencia media-alta (p.ej. cada 3 minutos)
- Las credenciales del dispositivo deben almacenarse en memoria no volátil y provisionarse de forma segura.

---

## 11. Actualizaciones de Firmware (OTA)

- Ambos módulos (control e HMI) deben soportar actualizaciones de firmware **sin necesidad de hardware externo** (sin programador físico).
- Las actualizaciones deben poder realizarse por WiFi y por GPRS.
- El firmware descargado debe verificarse con un checksum antes de aplicarse.
- El sistema debe usar **partición dual** de firmware: en caso de fallo en la nueva versión, puede revertir a la anterior.
- El resultado de la actualización debe reportarse a la plataforma IoT.

---

## 12. Interfaz de Usuario (HMI)

### 12.1 Pantalla principal

La pantalla principal debe mostrar en todo momento:
- Temperatura del aire: valor actual y consigna
- Temperatura de la piel: valor actual y consigna (solo visible si la sonda está conectada)
- Humedad: valor actual y consigna
- Estado del sistema (activo / standby)
- Modo de control activo (aire / piel)
- Indicadores de conectividad (WiFi, servidor, celular)
- Alertas activas de forma visible y prioritaria

### 12.2 Pantalla de alarmas

- Debe existir una pantalla dedicada que liste todas las alarmas activas.
- Cada alarma debe mostrar un texto corto y una descripción detallada.
- Debe indicarse el tiempo desde que se activó.

### 12.3 Pantalla de fototerapia

- Debe mostrar el tiempo restante en formato MM:SS.
- Debe permitir al usuario activar/desactivar y configurar el temporizador.

### 12.4 Pantalla de configuración

- Debe permitir configurar:
  - Credenciales WiFi
  - Plataforma IoT
  - Calibración de sensores
  - Idioma
  - Preferencias de pantalla (brillo, modo oscuro)
  - Volumen de audio

### 12.5 Requisitos de usabilidad

- La interfaz debe diseñarse para ser **usable con guantes médicos** (botones y zonas táctiles grandes).
- Debe soportar al menos los idiomas: **español, inglés, francés y portugués**.
- Las fuentes deben ser legibles a distancia para personal médico.
- Los cambios en consignas realizados por el usuario deben mostrarse como "pendientes" hasta que el módulo de control los confirme.
- Si el módulo de control no confirma un cambio, la interfaz debe revertir al valor anterior.

### 12.6 Histórico de valores

- La interfaz debe mostrar **gráficas en tiempo real** del histórico reciente de temperatura (aire y piel).
- El histórico debe mantenerse en memoria mientras el sistema esté encendido.

### 12.7 Retroiluminación

- La intensidad de la retroiluminación debe ser ajustable.
- Debe soportar un modo de apagado automático por inactividad (configurable).

---

## 13. Tests de Hardware en Arranque

- Al encender, el sistema debe realizar un **autotest de hardware** antes de comenzar a operar.
- El autotest debe verificar el consumo de corriente de cada actuador por separado.
- Si el consumo de un actuador está fuera de los límites esperados (demasiado alto o nulo), el sistema debe generar un error y bloquear el funcionamiento hasta que se resuelva.
- El autotest debe verificar también que todos los sensores responden correctamente.
- Si un sensor no responde y hay un sensor redundante disponible, el sistema debe continuar operando con el sensor alternativo y notificar el fallo.

---

## 14. Seguimiento de Mantenimiento

- El sistema debe registrar **contadores de tiempo acumulado** de operación por:
  - Horas totales en standby
  - Horas totales en control activo
  - Horas totales de calefactor activo
  - Horas totales de ventilador activo
  - Horas totales de humidificador activo
  - Horas totales de fototerapia
- Estos contadores deben persistir en memoria no volátil.
- Deben reportarse periódicamente a la plataforma IoT para posibilitar el mantenimiento preventivo basado en uso real.

---

## 15. Requisitos de Seguridad del Sistema

### 15.1 Cortes térmicos

- El sistema debe tener **cortes térmicos por hardware** independientes del software para el calefactor.
- Por software, el sistema debe aplicar cortes térmicos adicionales:
  - Temperatura del aire > 38,5 °C → apagado inmediato del calefactor
  - Temperatura de la piel > 37,5 °C → apagado inmediato del calefactor
- Los cortes deben tener histéresis y no reactivarse automáticamente hasta que la temperatura baje por debajo del umbral de reset.

### 15.2 Fallo de ventilador

- Si no se detectan pulsos de RPM del ventilador estando este activo, el sistema debe apagar el calefactor de forma inmediata (riesgo de sobrecalentamiento por falta de circulación).

### 15.3 Watchdog

- El sistema debe incorporar un watchdog por hardware (o software sobre RTOS) que reinicie el microcontrolador si el bucle principal se bloquea durante más de un tiempo configurado.

### 15.4 Fallo de sensor

- Si todos los sensores de temperatura de cabina fallan simultáneamente, el sistema debe apagar el calefactor y emitir una alarma crítica.

### 15.5 Independencia de la interfaz

- El módulo de control debe ser capaz de **detectar alarmas, emitir alertas sonoras y desactivar actuadores** aunque el módulo HMI esté desconectado o inoperativo.

---

## 16. Requisitos de Configuración y Ajuste

| Parámetro | Rango permitido | Incremento | Valor por defecto |
|-----------|----------------|-----------|-------------------|
| Consigna temperatura aire | 30,0–38,5 °C | 0,1 °C | 32,0 °C |
| Consigna temperatura piel | 35,0–37,5 °C | 0,1 °C | 36,0 °C |
| Consigna humedad | 20–90 % HR | 5 % | 60 % |
| Duración fototerapia | 1–120 min | 1 min | — |
| Idioma | ES / EN / FR / PT | — | ES |
| Volumen audio | 0–21 | 1 | 10 |
| Retardo de ignición alarmas | 0–60 min | 1 min | 30 min |

---

## 17. Requisitos No Funcionales

| Requisito | Descripción |
|-----------|-------------|
| **Coste** | El coste de los componentes electrónicos no debe superar los 400 € |
| **Disponibilidad** | El sistema de control debe operar de forma continua 24/7 sin reinicios periódicos |
| **Latencia de control** | El bucle PID de temperatura debe ejecutarse al menos cada 4 segundos |
| **Latencia de alarmas** | Una condición de alarma debe detectarse y activar el buzzer en menos de 2 segundos |
| **Persistencia** | Todos los ajustes deben sobrevivir a un ciclo de alimentación completo |
| **Actualización** | El firmware debe poderse actualizar sin herramientas externas ni apertura del equipo |
| **Idiomas** | La interfaz debe soportar al menos 4 idiomas desde el arranque |
| **Ergonomía** | Operable con guantes médicos estándar |
| **Registro** | Toda la telemetría debe poder trazarse de forma remota mediante una plataforma IoT |

---

## 18. Glosario

| Término | Definición |
|---------|-----------|
| **HMI** | Human-Machine Interface. Módulo de interfaz de usuario (pantalla táctil) |
| **PID** | Controlador Proporcional-Integral-Derivativo para control en lazo cerrado |
| **OTA** | Over-The-Air. Actualización de firmware de forma inalámbrica |
| **NTC** | Negative Temperature Coefficient. Tipo de termistor para medir temperatura |
| **Servocontrol** | Modo de control donde la variable regulada es la temperatura de la piel del bebé |
| **Corte térmico** | Mecanismo de seguridad que apaga el calefactor si se supera un límite de temperatura |
| **Bitmask** | Representación de múltiples estados booleanos en un solo valor numérico |
| **Telemetría** | Conjunto de mediciones enviadas periódicamente al sistema de monitorización remota |
| **Debounce** | Tiempo de espera para confirmar una señal y evitar falsos positivos por ruido mecánico |
| **Anti-windup** | Técnica para evitar la saturación del término integral en un controlador PID |
| **Histéresis** | Diferencia entre el umbral de disparo y el umbral de reset de una alarma |
| **Standby** | Estado de reposo del sistema: sensores activos, actuadores desactivados |

---

*Versión 2 — Hoja de Requisitos IncuNest*
*Basada en la funcionalidad existente y objetivos del producto — Marzo 2026*
