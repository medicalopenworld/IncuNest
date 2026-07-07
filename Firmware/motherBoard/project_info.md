================================================================================
PROYECTO: INCUNEST FIRMWARE (MOTHERBOARD V15) - CONTEXTO MAESTRO INDEXADO
================================================================================

1. DESCRIPCIÓN GENERAL
    1.1 Nombre: IncuNest (Desarrollado por Medical Open World).
                https://github.com/medicalopenworld/IncuNest/tree/master/Firmware
    1.2 Objetivo: Incubadora neonatal de código abierto de alta fiabilidad.
    1.3 Hardware: placa propietaria que utiliza ESP32-S3 y AFE4490 (Revisión actual V15).
    1.4 Framework: SDK Arduino con IDE Antigravity y extensión PlatformIO
	1.5 Extracto de platformio.ini: 
			"platform = espressif32@6.6.0
			framework = arduino
			board = esp32-s3-devkitc-1"
    1.6 SDK: Arduino para lo fácil, 
             ESP-IDF para configurar el hardware a bajo nivel (como el Bluetooth o el Log)
             FreeRTOS para que todas las piezas funcionen a la vez sin bloquearse
    1.7 OS: FreeRTOS (Arquitectura multitarea intensiva).

2. ARQUITECTURA DE SOFTWARE (TASKS FREERTOS)
    2.1 sensors_Task:   Lectura de NTC (piel), sensores digitales STS3x/SHTC3 
                        (aire/ambiente) y monitorización de consumo (INA3221).
    2.2 PID_Task:       Control de lazo cerrado para Calentador (Heater) 
                        y Humidificador.
    2.3 UI_Task:        Interfaz gráfica TFT (TFT_eSPI) y entrada por encoder.
    2.4 Security_Task:  PRIORIDAD CRÍTICA. Vigilancia de alarmas térmicas, 
                        fallos de ventilador y desconexión de sensores.
    2.5 Comm_Tasks:     Gestión WiFi, GPRS (SIM800) y ThingsBoard SDK (IoT).

3. MAPA DE ARCHIVOS DEL REPOSITORIO
    /include/main.h:    DICCIONARIO GLOBAL. Definiciones de pines, umbrales, constantes y flags de debug.
    /src/main.cpp:      Setup de periféricos y lanzamiento de tareas de FreeRTOS
    /src/sensors.cpp:   Capa de abstracción (HAL) para sensores I2C/analógicos.
    /src/PID.cpp:       Algoritmos de control (Proporcional-Integral-Derivativo).
    /src/security.cpp:  Lógica de alarmas y protección del paciente.
    /src/updateData.cpp:Gestión de estados globales y debugs/logs del sistema (logI,LogE)
    /src/GPRS.cpp:      Módulos de conectividad celular y telemetría.

4. ESPECIFICACIONES TÉCNICAS
    4.1 Comunicación:       UART entre placas, I2C para sensores, SPI para pantalla y AFE4490
    4.2 Persistencia:       Uso de EEPROM para calibraciones y estados críticos.
    4.3 Seguridad HW:       Watchdog timer y redundancia en sensores (STS3x/SHTC3).
    4.4 Depuración:         Logs (logI, logE, logAlarm) vía flags en main.h.

5. STACK TECNOLÓGICO Y LIBRERÍAS
    5.1 Lenguaje:           C++ (Arduino/ESP-IDF/FreeRTOS).
    5.2 Gráficos:           TFT_eSPI.
    5.3 IoT/Nube:           ThingsBoard SDK.
    5.4 Celular/GSM:        TinyGSM.
    5.5 Sensor de Potencia: Beastdevices_INA3221.

6. REGLAS DE ORO PARA EL DESARROLLO (IA)
    6.1 SEGURIDAD:          Dispositivo médico. La fiabilidad es prioridad 1.
    6.2 THREAD-SAFE:        Uso de Mutex (log_mutex) en recursos compartidos.
    6.3 NO BLOQUEANTE:      Prohibido delay(). Usar vTaskDelay() para FreeRTOS.
    6.4 CONSISTENCIA:       Seguir estrictamente los pines de main.h.
    6.5 ROBUSTEZ:           Manejar errores de comunicación I2C siempre.
	
7. Elementos de la arquitectura SW:
    7.1 SDK Arduino para lo fácil  (Arduino.h)
    7.2 SDK ESP-IDF para configurar el hardware a bajo nivel (como el Bluetooth o el Log) (esp_log.h)
    7.3 API de FreeRTOS para que todas las piezas funcionen a la vez sin bloquearse (freertos/semphr.h)
	7.4 Lógica (cerebro): regulación de temperatura y alarmas (security.cpp, PID.cpp, updateData.cpp)
	7.5 Librerías externas: 
		Gestión de Sensores: Librerías para leer los chips SHT4x, STS3x e INA3221 (consumo eléctrico).
		Gráficos y Pantalla: TFT_eSPI (la que dibuja los menús) y Adafruit_GFX.
		Comunicaciones: TinyGSM (para que el modem SIM800 hable con internet) y ThingsBoard (para enviar los datos a la nube).
		Datos: ArduinoJson para empaquetar la información antes de enviarla.
	7.6 HAL personalizada (Hardware Abstraction Layer) con diferentes versiones de placa (V13,V14,V15)
		Configuración de pines (include/board.h)
		Driver propio: src/in3ator_humidifier.cpp (driver específico)
	7.7 Sistema de Particiones y Almacenamiento
		NVS (Non-Volatile Storage): Es una mini base de datos dentro del ESP32 donde guardas valores que no se deben borrar al apagar (como el número de serie o el WiFi).
		Particionado (CSV): En la raíz del proyecto tienes archivos como ESP32S3_OTA_partition_16MB.csv. Estos definen cómo se divide la memoria Flash (cuánto para el código, cuánto para actualizaciones OTA, cuánto para datos).

8. Resumen del "Stack" de IncuNest:
	Capa 7: Aplicación Médica (Seguridad, PID, UI).
	Capa 6: Librerías de Componentes (Sensores, Pantalla, Nube).
	Capa 5: HAL y Drivers Propios (board.h, humidifier.cpp).
	Capa 4: Framework Arduino (Abstracción fácil).
	Capa 3: FreeRTOS (Gestión de multitarea).
	Capa 2: ESP-IDF (SDK nativo de Espressif).
	Capa 1: Silicio (ESP32-S3 Hardware).


9. ARQUITECTURA DEL SOFTWARE (FreeRTOS Multitasking)
	El sistema gestiona tareas críticas en paralelo para garantizar la seguridad:
		- sensors_Task: Lectura de sensores (NTC piel, STS3x/SHTC3 aire, consumos INA3221).
		- PID_Task: Control de lazo cerrado para Calentador (Heater) y Humidificador.
		- UI_Task: Gestión de interfaz gráfica (TFT_eSPI) y entrada de usuario (Encoder).
		- Communications: WiFi/GPRS para telemetría mediante ThingsBoard SDK.
		- Security_Task: Tarea de máxima prioridad para monitorizar fallos técnicos y alarmas de paciente.

================================================================================
FIN DEL CONTEXTO
================================================================================
