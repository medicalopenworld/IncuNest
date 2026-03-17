# 📚 Inventario de Documentación de IncuNest

Este documento sirve como índice centralizado para navegar por la documentación y los recursos del proyecto IncuNest.

## 💻 Firmware

El código fuente para los microcontroladores y la interfaz de usuario.

### [Display HMI](../Firmware/Display_HMI)
Interfaz Hombre-Máquina (Human-Machine Interface). Gestiona la pantalla táctil y la interacción con el usuario.
- **Ruta:** `Firmware/Display_HMI`
- **Componentes clave:** Interfaz gráfica (LVGL/SquareLine), lógica de visualización.

### [Motherboard (Placa Base)](../Firmware/motherBoard)
El "cerebro" de la incubadora. Gestiona la sensórica, el control de temperatura/humedad y la comunicación.
- **Ruta:** `Firmware/motherBoard`
- **Plataforma:** ESP32
- **Hardware Detallado:** [Ver Especificaciones Técnicas](TECNOLOGIA.md)
- **Funciones:**
    - Lectura de sensores (Temperatura, Humedad).
    - Control de actuadores (Calefacción, Ventiladores).
    - Comunicación WiFi/Telemetry (ThingsBoard).

---

## 🛠️ Hardware

Diseños electrónicos, esquemáticos, PCBs y archivos mecánicos.

### Electrónica

#### [Motherboard (Lógica)](../Hardware/Electronics/Motherboard)
Diseño de la PCB principal que aloja el ESP32 y los circuitos de potencia/control.
- **Ruta:** `Hardware/Electronics/Motherboard`

#### [Sensores y Actuadores](../Hardware/Electronics/Ambient_sensor)
Diseños de PCBs satélite y sensores.
- **Sensores Ambientales:** `Hardware/Electronics/Ambient_sensor`
- **Iluminación LED:** `Hardware/Electronics/FullLEDStrip` (`../Hardware/Electronics/FullLEDStrip`)

### Mecánica

#### [Estructura y Chasis](../Hardware/Mechanical)
Archivos CAD y modelos 3D para la construcción física de la incubadora.
- **Ruta:** `Hardware/Mechanical`
- **Archivos principales:** Archivos STEP para impresión 3D o mecanizado.

---

> [!TIP]
> Para detalles específicos de instalación o compilación de código, consulta el `README.md` dentro de cada subdirectorio de Firmware.
