# Análisis de Comunicación Motherboard <-> Display_HMI

## 1. Situación Actual del Firmware

### Arquitectura de Comunicación
- **Motherboard (Host):** Actúa como USB Host utilizando la pila USB de ESP-IDF (`usb_host`, `cdc_acm_host`). Esto le permite alimentar y leer datos del Display a través de USB.
- **Display (Device):** Actúa como un dispositivo USB Serial (CDC), enviando comandos y recibiendo telemetría.

### Estado de los Drivers (CH340 vs CP210x)
Actualmente, el firmware de la Motherboard (`communication_host.cpp`) está diseñado para ser **universal**. En la función `CommunicationHost_Init`, se registran explícitamente tres tipos de drivers:
```cpp
VCP::register_driver<CH34x>();
VCP::register_driver<CP210x>(); // <--- Aquí se usa el driver de CP210x
VCP::register_driver<FT23x>();
```

**Respuesta a tu pregunta:**
¿Sobran los archivos de `CP210X`?
- **Físicamente:** Sí, si tu hardware (Display) usa exclusivamente el chip **CH340C**, el código y lógica para CP210x y FTDI no se utilizan en tiempo de ejecución.
- **En el Código:** No "sobran" en el sentido de que si borras los archivos ahora mismo, **la compilación fallará** porque `communication_host.cpp` incluye sus cabeceras y llama a sus funciones de registro.
- **Solución Recomendada:** Podemos refactorizar el código para eliminar las referencias a CP210x y FTDI, haciendo el firmware más ligero y específico para tu hardware real (CH340C).

## 2. Diagnóstico de Problemas de Conexión ("A veces sí, a veces no")

He identificado una posible causa en la secuencia de inicialización específica para el CH340C en `communication_host.cpp`:

```cpp
// --- HMI BOOT FIX: Ensure stable state before enabling DTR/RTS ---
vcp->set_control_line_state(false, false);
vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to allow HMI to boot safely

// Enable DTR/RTS (CH340C requirement) - INSIDE MUTEX
vcp->set_control_line_state(true, true);
```

### Factores de Inestabilidad:
1.  **Dependencia de DTR/RTS:** El chip CH340 a menudo requiere que las señales DTR/RTS estén en alto para funcionar correctamente y alimentar la lógica interna o permitir la transmisión. Si esta secuencia falla o se hace demasiado pronto/tarde respecto al arranque eléctrico del Display, la conexión falla.
2.  **Tiempo de Espera (1000ms):** El delay fijo de 1 segundo puede ser insuficiente si el Display tarda más en arrancar eléctricamente al ser conectado, o excesivo si causa timeouts en la pila USB.
3.  **Conflictos de Energía:** Al conectar el Display, hay un pico de corriente (inrush current). Si la Motherboard no estabiliza la tensión USB rápido, el CH340 puede entrar en un estado indefinido (brownout) y no enumerarse correctamente hasta un reinicio físico completo.

## 3. Recomendaciones y Plan de Acción

### A. Limpieza de Drivers (Prioridad Baja - Mantenimiento)
Eliminar el soporte de CP210x y FTDI para limpiar el proyecto.
- **Acción:** Modificar `communication_host.cpp` para quitar `VCP::register_driver<CP210x>` y borrar los archivos correspondientes.

### B. Mejora de Robustez de Conexión (Prioridad Alta - Crítico)
Para solucionar el "a veces no conecta":

1.  **Reintento de Inicialización de Línea:** Si la conexión se establece pero no hay datos, intentar conmutar DTR/RTS nuevamente sin cerrar todo el puerto USB.
2.  **Aumentar Logs de Depuración:** Habilitar logs detallados en la fase de `VCP::open` para distinguir si el problema es "No se detecta dispositivo USB" (problema eléctrico/cable) o "Se detecta pero no responde" (problema de driver/lógica).
3.  **Verificar Hardware:** Asegurarse de que el rail de 5V USB de la Motherboard sea estable. El CH340 es sensible a caídas de tensión.

### C. Conflicto de Archivos
No hay "conflicto" técnico entre tener los archivos de CP210x y usar un CH340, siempre que el sistema detecte el PID correcto. El problema actual es probablemente puramente de temporización o eléctrico con el CH340.

## Conclusión
El archivo `CP210X` no está causando el fallo, pero podemos quitarlo para limpiar. El problema de intermitencia reside probablemente en la **secuencia de arranque (boot sequence)** y la gestión de las señales de control (DTR/RTS) del CH340C. Recomiendo refactorizar `communication_host.cpp` para centrarlo solo en CH340 y ajustar los tiempos de espera.
