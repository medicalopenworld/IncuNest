# Elementos de la Interfaz de Usuario (HMI) — IncuNest

> **Archivos fuente:** `Firmware/Display_HMI/src/ElementsCreation.cpp` y `UITask.cpp`  
> **Framework gráfico:** LVGL (Light and Versatile Graphics Library)  
> **Resolución pantalla:** 800 × 480 px (Elecrow CrowPanel 7.0, driver ILI9341)

---

## Índice de Pantallas

| Pantalla | Variable | Descripción |
|---|---|---|
| Intro | `ui_ScreenIntro` | Splash de arranque con logotipo |
| Principal | `ui_ScreenMain` | Panel de control principal |
| Ajustes | `ui_ScreenSettings` | Configuración del sistema |
| Alarmas | `ui_ScreenAlarms` | Lista y detalle de alarmas |
| Gráficas | `ui_ScreenCharts` | Gráficas de temperatura y humedad |
| Pulsioximetría | `ui_ScreenPulseOxi` | Pantalla SpO₂ / FC |
| Bloqueo | `ui_ScreenLock` | Pantalla de bloqueo / reposo |

---

## 1. Pantalla de Introducción — `ui_ScreenIntro`

Pantalla de arranque con fondo gris claro (`#BFBFBF`) que muestra el logotipo durante 3 segundos y luego carga la pantalla principal.

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Logotipo | `ui_ImageLogoIncunest` | `lv_img` | Imagen PNG del logo de IncuNest, centrada en pantalla |

---

## 2. Pantalla Principal — `ui_ScreenMain`

Panel de control con tres zonas verticales: **Temperatura** (columna izquierda), **Humedad + Fototerapia** (columna derecha superior) y **Timer de Fototerapia** (columna derecha inferior).

### 2.1 Barra superior

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Título | `ui_Incunest` | `lv_label` | Texto fijo "IncuNest", fuente Montserrat 26, arriba izquierda |
| Botón Bloqueo | `ui_ImgButton1` | `lv_imgbtn` | Icono candado (38×46 px). Navega a `ui_ScreenLock` |
| Botón Gráficas | `ui_ChartButton` | `lv_imgbtn` | Icono chart (52×51 px). Navega a `ui_ScreenCharts`. Oculto por defecto |
| Botón SpO₂ | `ui_SPO2Button` | `lv_imgbtn` | Icono pulso (51×47 px). Navega a `ui_ScreenPulseOxi` |
| Botón Alarmas | `ui_AlarmButton` | `lv_imgbtn` | Icono campana (48×47 px). Navega a `ui_ScreenAlarms`. Se oculta si no hay alarmas |
| Badge alarmas | `ui_Panel10` | `lv_obj` | Fondo rojo (24×27 px) del contador de alarmas |
| Contador alarmas | `ui_NumAlarm` | `lv_label` | Número de alarmas activas. Solo visible cuando `alarmCount > 0` |
| Check OK | `ui_CheckImgMain` | `lv_img` | Icono check verde. Visible cuando no hay alarmas activas |
| Botón Ajustes | `ui_Settings` | `lv_imgbtn` | Icono engranaje (50×48 px), arriba derecha. Navega a `ui_ScreenSettings` |

### 2.2 Zona de Temperatura — Contenedor `ui_TempCont` (381×421 px, columna izquierda)

#### Paneles de fondo

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel principal temperatura | `ui_Panel1` | `lv_obj` | Fondo gris/blanco 376×420 px. Se vuelve blanco al activar el switch |
| Panel cabecera temperatura | `ui_Panel4` | `lv_obj` | Franja superior de la sección temperatura (376×50 px) |

#### Encabezado temperatura

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Etiqueta sección | `ui_Label2` | `lv_label` | Texto "TEMPERATURA" / "TEMPERATURE" / "TEMPERATURE" (multilingüe) |
| Switch temperatura | `ui_Switch1` | `lv_switch` | Activa/desactiva el control de temperatura (100×39 px) |
| Label ON temp | `ui_Label9` | `lv_label` | Texto "ON". Al pulsarlo activa `ui_Switch1` |
| Label OFF temp | `ui_Label15` | `lv_label` | Texto "OFF". Al pulsarlo desactiva `ui_Switch1` |
| Botón invisible | `ui_TempButton` | `lv_btn` | Botón transparente que captura taps en el área del header de temperatura |

#### Sub-panel Aire — `ui_AirPanelCont` (354×136 px)

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel fondo Aire | `ui_AirPanel` | `lv_obj` | Panel seleccionable (350×127 px). Color blanco si activo, gris si inactivo. Al tocar selecciona modo Aire |
| Etiqueta "AIR" | `ui_Label30` | `lv_label` | Texto "AIRE" / "AIR" / "AIR", fuente Montserrat 20 |
| Imagen ventilador | `ui_Image4` | `lv_img` | Icono ventilador/aire |
| Temperatura Aire detectada (izq) | `ui_TempAirDetected` | `lv_label` | Temperatura medida del aire en grande (Montserrat 26). Ej: "25.5°C" |

**Sub-contenedor barra derecha Aire — `ui_AirTempBarCont` (120×108 px)**

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Temperatura Aire detectada (der) | `ui_TempAirDetectedRight` | `lv_label` | Temperatura detectada (Montserrat 20), a la derecha de la barra |
| Temperatura Aire deseada | `ui_TempAirDesired` | `lv_label` | Temperatura objetivo (Montserrat 20), encima de la barra |
| Barra visual Aire | `ui_AirTempBar` | `lv_bar` | Barra vertical (13×55 px), rango 0-40°C, indicador azul `#0075EE` |
| Imagen termómetro Aire | `ui_Image6` | `lv_img` | Icono termómetro bajo la barra |

#### Sub-panel Piel — `ui_SkinPanelCont` (359×134 px)

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel fondo Piel | `ui_SkinPanel` | `lv_obj` | Panel seleccionable (350×127 px). Mismo comportamiento que AirPanel pero para Piel |
| Etiqueta "SKIN" | `ui_Label31` | `lv_label` | Texto "PIEL" / "SKIN" / "PEAU", fuente Montserrat 20 |
| Imagen bebé | `ui_Image2` | `lv_img` | Icono bebé (`bebe_icon.png`) |
| Temperatura Piel detectada (izq) | `ui_TempSkinDetected` | `lv_label` | Temperatura piel medida (Montserrat 26). Ej: "28.1°C" |

**Sub-contenedor barra derecha Piel — `ui_SkinTempBarCont` (120×108 px)**

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Temperatura Piel detectada (der) | `ui_TempSkinDetectedRight` | `lv_label` | Temperatura piel detectada (Montserrat 20) |
| Temperatura Piel deseada | `ui_TempSkinDesired` | `lv_label` | Temperatura piel objetivo (Montserrat 20) |
| Barra visual Piel | `ui_SkinTempBar` | `lv_bar` | Barra vertical (13×56 px), rango 0-40°C, indicador azul |
| Imagen termómetro Piel | `ui_Image1` | `lv_img` | Icono termómetro bajo la barra |

#### Flechas de ajuste de temperatura

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Fondo flecha abajo temp | `ui_ArrowDownTemp` | `lv_obj` | Contenedor cuadrado (60×60 px) de la flecha BAJAR temperatura |
| Fondo flecha arriba temp | `ui_ArrowUpTemp` | `lv_obj` | Contenedor cuadrado (60×60 px) de la flecha SUBIR temperatura |
| Imagen flecha abajo temp | `ui_ImgArrowDownTemp` | `lv_imgbtn` | Triángulo apuntando hacia abajo (39×42 px). Reduce temperatura en 0.1°C por toque. Solo activo si el switch está encendido |
| Imagen flecha arriba temp | `ui_ImgArrowUpTemp` | `lv_imgbtn` | Triángulo apuntando hacia arriba (39×41 px). Aumenta temperatura en 0.1°C por toque |
| Label "SET" | `ui_Label6` | `lv_label` | Texto "AJUSTAR" / "SET" / "REGLER", entre las dos flechas |

#### Advertencia error calentador (temperatura)

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Contenedor error temp | `ui_HeaterErrorTempCont` | `lv_obj` | Superposición roja (381×421 px) sobre la zona de temperatura. Oculto normalmente. Parpadea si se detecta fallo del calentador. Al tocar navega a la pantalla de Alarmas |
| Imagen error temp | `ui_HeaterErrorTempImg` | `lv_img` | Icono de alarma ampliado (zoom 4x) |
| Label error temp | `ui_HeaterErrorTempLabel` | `lv_label` | Texto multilingüe con instrucciones de error del calentador, fondo rojo, texto blanco |

---

### 2.3 Zona de Humedad — Contenedor `ui_HumCont` (378×248 px, columna derecha superior)

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel principal humedad | `ui_Panel3` | `lv_obj` | Fondo gris/blanco 376×236 px. Se vuelve blanco al activar el switch |
| Panel cabecera humedad | `ui_Panel6` | `lv_obj` | Franja superior (376×51 px) |
| Etiqueta sección | `ui_HumidityLabel` | `lv_label` | Texto "HUMEDAD" / "HUMIDITY" / "HUMIDITY" |
| Switch humedad | `ui_Switch2` | `lv_switch` | Activa/desactiva el control de humedad (100×39 px) |
| Label ON hum | `ui_Label13` | `lv_label` | Texto "ON". Al tocar activa `ui_Switch2` |
| Label OFF hum | `ui_Label16` | `lv_label` | Texto "OFF". Al tocar desactiva `ui_Switch2` |
| Botón invisible | `ui_HumidButton` | `lv_btn` | Botón transparente para captura de taps en la cabecera de humedad |

**Sub-contenedor panel humedad — `ui_HumPanelCont` (358×163 px)**

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Humedad detectada (izq) | `ui_HumDetected` | `lv_label` | Humedad medida en grande (Montserrat 26). Ej: "50%" |
| Humedad detectada (der) | `ui_HumDetectedRight` | `lv_label` | Humedad medida (Montserrat 20), a la derecha de la barra |
| Humedad deseada | `ui_HumDesired` | `lv_label` | Humedad objetivo (Montserrat 20), encima de la barra |
| Barra visual Humedad | `ui_HumBar` | `lv_bar` | Barra vertical (47×66 px), rango 0-100% |
| Imagen gota de agua | `ui_Image7` | `lv_img` | Icono gota (`gota.png`) superpuesto a la barra |
| Fondo flecha arriba hum | `ui_ArrowUpHum` | `lv_obj` | Contenedor (60×60 px) flecha SUBIR humedad |
| Fondo flecha abajo hum | `ui_ArrowDownHum` | `lv_obj` | Contenedor (60×60 px) flecha BAJAR humedad |
| Imagen flecha arriba hum | `ui_ImgArrowUpHum` | `lv_imgbtn` | Triángulo arriba (40×38 px). Sube humedad en paso definido |
| Imagen flecha abajo hum | `ui_ImgArrowDownHum` | `lv_imgbtn` | Triángulo abajo (39×45 px). Baja humedad |
| Label "SET" hum | `ui_Label7` | `lv_label` | Texto "AJUSTAR" / "SET" / "REGLER", entre las flechas de humedad |

**Advertencia error calentador (humedad)**

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Contenedor error hum | `ui_HeaterErrorHumCont` | `lv_obj` | Superposición (378×320 px) sobre la zona de humedad. Mismo comportamiento que el de temperatura |
| Imagen error hum | `ui_HeaterErrorHumImg` | `lv_img` | Icono de alarma ampliado |
| Label error hum | `ui_HeaterErrorHumLabel` | `lv_label` | Texto multilingüe de error calentador |

---

### 2.4 Zona de Fototerapia — `ui_PhotoCont` (384×54 px)

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel fondo fototerapia | `ui_Panel2` | `lv_obj` | Fondo de la fila de fototerapia (376×52 px) |
| Etiqueta sección | `ui_PhototherapyLabel` | `lv_label` | Texto "FOTOTERAPIA" / "PHOTOTHERAPY" |
| Switch fototerapia | `ui_Switch3` | `lv_switch` | Enciende/apaga la lámpara de fototerapia (100×39 px) |
| Label ON foto | `ui_Label10` | `lv_label` | Texto "ON". Al tocar activa el switch de fototerapia |
| Label OFF foto | `ui_Label17` | `lv_label` | Texto "OFF". Al tocar desactiva el switch de fototerapia |

---

### 2.5 Panel de Temporizador de Fototerapia — `ui_PhotoTimerCont` (384×120 px)

Solo visible cuando el switch de fototerapia (`ui_Switch3`) está encendido.

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel fondo timer | `ui_PhotoTimerPanel` | `lv_obj` | Fondo gris/blanco (376×110 px) del panel del temporizador |
| Botón Menos `-` | `ui_PhotoTimeMinusBtn` | `lv_btn` | Resta 20 min al timer (rango 20-600 min). Bloqueado si el timer está corriendo |
| Label `-` | `ui_PhotoTimeMinusLabel` | `lv_label` | Signo "–", fuente Montserrat 26 |
| Valor del timer | `ui_PhotoTimeValueLabel` | `lv_label` | Muestra el tiempo configurado en formato "H:MM". Ej: "1:00". Fuente Montserrat 26 |
| Botón Más `+` | `ui_PhotoTimePlusBtn` | `lv_btn` | Suma 20 min al timer. Bloqueado si el timer está corriendo |
| Label `+` | `ui_PhotoTimePlusLabel` | `lv_label` | Signo "+", fuente Montserrat 26 |
| Botón Iniciar | `ui_PhotoStartBtn` | `lv_btn` | Inicia la cuenta regresiva. Texto "EMPEZAR"/"START"/"DEMARRER" (150×30 px) |
| Label iniciar | `ui_PhotoStartLabel` | `lv_label` | Texto del botón iniciar |
| Botón Cancelar | `ui_PhotoCancelBtn` | `lv_btn` | Botón rojo "X" (35×30 px). Solo visible mientras el timer está activo. Cancela el temporizador y vuelve a modo continuo |
| Label cancelar | `ui_PhotoCancelLabel` | `lv_label` | Texto "X" del botón cancelar |

---

## 3. Pantalla de Ajustes — `ui_ScreenSettings`

### 3.1 Barra superior

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Título | `ui_Label8` | `lv_label` | Texto "AJUSTES" / "SETTINGS" / "PARAMETRES", Montserrat 26 |
| Botón volver | `ui_ImgButton2` | `lv_imgbtn` | Icono flecha atrás (50×52 px). Vuelve a `ui_ScreenMain` |

### 3.2 Menú de opciones — `ui_Container3` (331×316 px, columna izquierda)

Cada opción del menú sigue la misma estructura: un contenedor (`Cont`), un panel de fondo (`Panel`), una etiqueta de texto, un botón transparente de tap y una flecha `>` o un switch.

| Contenedor | Panel fondo | Label | Interacción | Descripción |
|---|---|---|---|---|
| `ui_InfoCont` | `ui_InfoPanel` | `ui_InfoLabel` ("INFO") | `ui_InfoButton` + `ui_InfoArrow` | Abre `ui_InfoDetailsCont` con versiones y S/N |
| `ui_WifiCont` | `ui_Panel7` | `ui_WifiLabel` ("WIFI") | `ui_WifiButton` + `ui_Label3` (">") | Muestra `ui_WifiConfigCont` o `ui_WifiConnectedCont` |
| `ui_LanguagesCont` | `ui_Panel8` | `ui_LanguagesLabel` ("IDIOMA") | `ui_LanguagesButton` + `ui_Label1` (">") | Muestra `ui_LanguagesDropDown` |
| `ui_SkinModeCont` | `ui_Panel9` | `ui_SkinOptionLabel` ("MODO PIEL") | `ui_Switch4` | Habilita/deshabilita el panel de temperatura de piel |
| `ui_DarkModeCont` | `ui_PanelDarkMode` | `ui_DarkModeLabel` ("MODO OSCURO") | `ui_SwitchDarkMode` | Activa modo oscuro en toda la UI |

### 3.3 Panel de detalles de información — `ui_InfoDetailsCont` (411×190 px)

Oculto por defecto. Se muestra al pulsar el botón INFO.

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel fondo | `ui_InfoDetailsPanel` | `lv_obj` | Fondo del panel de info |
| Título versión HMI | `ui_HMIVerTitle` | `lv_label` | Texto "VERSION PANTALLA:" / "DISPLAY VERSION:" |
| Valor versión HMI | `ui_HMIVerValue` | `lv_label` | Versión firmware HMI (azul `#2196F3`) |
| Título versión MB | `ui_MBVerTitle` | `lv_label` | Texto "VERSION PLACA:" / "MOTHERBOARD VERSION:" |
| Valor versión MB | `ui_MBVerValue` | `lv_label` | Versión firmware Motherboard (azul) |
| Título S/N | `ui_SNTitle` | `lv_label` | Texto "S/N:" |
| Valor S/N | `ui_SNValue` | `lv_label` | Número de serie de 4 dígitos (azul) |
| Título conectividad | `ui_ConnTitle` | `lv_label` | Texto "CONECTIVIDAD:" / "CONNECTIVITY:" |
| Valor conectividad | `ui_ConnValue` | `lv_label` | Estado de conexión (WIFI+SERVER, 2G, DESCONECTADO…) (azul) |

### 3.4 Panel de configuración WiFi — `ui_WifiConfigCont` (770×361 px)

Oculto por defecto. Aparece al pulsar el botón WiFi cuando no hay conexión.

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel SSID | `ui_SSIDPanel` | `lv_obj` | Contenedor del campo SSID (400×55 px, bordes redondeados) |
| Label SSID | `ui_SSIDLabel` | `lv_label` | Etiqueta "SSID" |
| Campo texto SSID | `ui_TextArea1` | `lv_textarea` | Input de texto para el SSID de la red WiFi |
| Panel contraseña | `ui_PassPanel` | `lv_obj` | Contenedor del campo contraseña (400×55 px) |
| Label contraseña | `ui_PassLabel` | `lv_label` | Etiqueta "CONTRASENA" / "PASSWORD" |
| Campo texto contraseña | `ui_TextArea2` | `lv_textarea` | Input cifrado para la contraseña (modo password) |
| Teclado virtual | `ui_Keyboard1` | `lv_keyboard` | Teclado LVGL (750×185 px). Aparece al tocar los TextAreas y se oculta al confirmar |
| Botón Conectar | `ui_WifiConnectButton` | `lv_btn` | Botón azul "CONECTAR" (130×45 px). Inicia la conexión WiFi |
| Label Conectar | `ui_ConnectLabel` | `lv_label` | Texto del botón conectar |

### 3.5 Panel WiFi conectado — `ui_WifiConnectedCont`

Oculto por defecto. Aparece al pulsar WiFi cuando ya hay conexión activa.

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel fondo | `ui_WifiConnectedPanel` | `lv_obj` | Fondo del panel de "WiFi conectado" |
| Icono flecha | `ui_ArrowWifiConnected` | `lv_label` | Flecha decorativa |
| Label "WiFi conectado a" | `ui_WifiConnectedToLabel` | `lv_label` | Texto "WIFI CONECTADO A" |
| Label SSID activo | `ui_WifiSSIDLabel` | `lv_label` | Muestra el SSID al que está conectado |

### 3.6 Selector de idioma — `ui_LanguagesDropDown`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Dropdown idioma | `ui_LanguagesDropDown` | `lv_dropdown` | Selector desplegable con opciones: Español / Inglés / Francés. Al cambiar aplica el idioma a toda la UI y lo persiste en EEPROM |

---

## 4. Pantalla de Alarmas — `ui_ScreenAlarms`

### 4.1 Barra superior

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Botón volver | `ui_ImgButton7` | `lv_imgbtn` | Icono flecha atrás (50×52 px). Vuelve a `ui_ScreenMain` |
| Botón silenciar | `ui_MuteAlarm` | `lv_imgbtn` | Icono mute (44×45 px). Silencia el sonido de alarmas. Se oculta tras pulsarlo |

### 4.2 Panel de fondo

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel fondo | `ui_Panel5` | `lv_obj` | Panel de fondo (768×396 px) del área de alarmas |

### 4.3 TabView de alarmas — `ui_AlarmsTabview` (743×364 px)

TabView con dos pestañas:
- **Pestaña 1** — "ALARMAS" / "ALARMS": Lista de alarmas activas
- **Pestaña 2** — "VER DETALLES" / "VIEW DETAILS": Descripción detallada

#### Pestaña 1 — Lista de alarmas (`ui_TabPage1`)

Hasta 4 alarmas pueden mostrarse simultáneamente. Cada entrada sigue el mismo patrón:

| Conjunto | Contenedor | Panel | Label | Descripción |
|---|---|---|---|---|
| Alarma 1 | `ui_Alarm1Cont` (712×50 px) | `ui_Alarm1Panel` (fondo rojo `#CD3C3C`) | `ui_Alarm1Label` (Montserrat 30) | Primera alarma activa. Parpadea. Al tocar muestra detalles |
| Alarma 2 | `ui_Alarm2Cont` | `ui_Alarm2Panel` | `ui_Alarm2Label` | Segunda alarma |
| Alarma 3 | `ui_Alarm3Cont` | `ui_Alarm3Panel` | `ui_Alarm3Label` | Tercera alarma |
| Alarma 4 | `ui_Alarm4Cont` | `ui_Alarm4Panel` | `ui_Alarm4Label` | Cuarta alarma |

> **Nota:** Cada contenedor de alarma está oculto (`LV_OBJ_FLAG_HIDDEN`) hasta que la alarma correspondiente se activa. Al activarse, el panel parpadea mediante animación de opacidad.

#### Pestaña 2 — Descripción (`ui_TabPage2`)

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Descripción de alarma | `ui_AlarmDetailLabel` | `lv_label` | Texto descriptivo de la alarma seleccionada. Se vacía al volver a la pestaña de lista |

---

## 5. Pantalla de Gráficas — `ui_ScreenCharts`

### 5.1 TabView principal — `ui_TabViewMainCharts` (800×430 px)

Dos pestañas:
- **Pestaña "TIEMPO REAL"** / "REAL TIME": Gráficas en tiempo real
- **Pestaña "HISTORIAL"** / "HISTORY": Gráficas históricas con buffer circular de hasta 2 horas

### 5.2 Sub-TabView tiempo real — `ui_TabView1` (769×403 px)

Dos pestañas: Temperatura y Humedad.

#### Página Temperatura — `ui_TempChartPage1`

| Contenedor | Gráfica | Label título | Color | Rango | Descripción |
|---|---|---|---|---|---|
| `ui_AirTempChartCont` | `ui_AirTempChart` | `ui_Label37` | Verde `#00FF00` | 20-40°C | Gráfica línea temp. Aire. Zona segura sombreada 34-37°C |
| `ui_SkinTempChartCont` | `ui_SkinTempChart` | `ui_Label38` | Cian `#00E0E0` | 20-40°C | Gráfica línea temp. Piel. Oculta si Aire está activo. Zona segura 36-37.5°C |

#### Página Humedad — `ui_HumChartPage2`

| Contenedor | Gráfica | Label título | Color | Rango | Descripción |
|---|---|---|---|---|---|
| `ui_HumChartCont` | `ui_HumChart` | `ui_Label36` | Azul `#3B82F6` | 0-100% | Gráfica línea humedad. Zona segura sombreada 40-70% |

> Todas las gráficas en tiempo real almacenan 50 puntos. Se actualizan cada segundo. Usan el estilo sparkline con gradiente y sin puntos individuales.

### 5.3 Sección Historial

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Dropdown rango | `ui_HistoryDropdown` | `lv_dropdown` | Selección de ventana temporal: 5 min / 30 min / 1 h / 2 h |
| Label rango | `ui_HistoryTimeLabel` | `lv_label` | Texto "RANGO:" / "RANGE:" |
| **Temperatura Aire** | | | |
| Label Aire hist | `ui_HistoryChartAireLabel` | `lv_label` | Texto "HISTORIAL TEMP AIRE" / "AIR TEMP HISTORY" |
| Valor actual Aire | `ui_HistoryValueAire` | `lv_label` | Último valor de temperatura aire (Ej: "36.5°C") |
| Gráfica hist Aire | `ui_HistoryChartAire` | `lv_chart` | Gráfica histórica temp. Aire; verde. 720 puntos máx (2h@10s/punto) |
| **Temperatura Piel** | | | |
| Label Piel hist | `ui_HistoryChartSkinLabel` | `lv_label` | Texto "HISTORIAL TEMP PIEL" / "SKIN TEMP HISTORY" (oculto si AIR visible) |
| Valor actual Piel | `ui_HistoryValueSkin` | `lv_label` | Último valor temp. piel |
| Gráfica hist Piel | `ui_HistoryChartSkin` | `lv_chart` | Gráfica histórica temp. Piel; cian. Oculta por defecto |
| **Humedad** | | | |
| Label Hum hist | `ui_HistoryChartHumLabel` | `lv_label` | Texto "HISTORIAL TEMP HUM" / "HUMIDITY HISTORY" |
| Valor actual Hum | `ui_HistoryValueHum` | `lv_label` | Último valor humedad detectada |
| Gráfica hist Hum | `ui_HistoryChartHum` | `lv_chart` | Gráfica histórica humedad; azul |

### 5.4 Gráfica SpO₂ (reservada)

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Contenedor Ox | `ui_OxChartCont` | `lv_obj` | Oculto. Reservado para futura gráfica de oximetría |
| Gráfica Ox | `ui_OxChart` | `lv_chart` | Gráfica de oximetría, 637×336 px, rango 0-100% |
| Label Ox | `ui_Label35` | `lv_label` | Texto "GRAFICO OXIMETRIA" / "OXIMETRY CHART" |

### 5.5 Botón volver gráficas

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Botón volver | `ui_ImgButton8` | `lv_imgbtn` | Icono flecha atrás, arriba izquierda. Vuelve a `ui_ScreenMain` |

---

## 6. Pantalla de Pulsioximetría — `ui_ScreenPulseOxi`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Botón volver | `ui_ImgButton9` | `lv_imgbtn` | Flecha atrás (50×52 px). Vuelve a `ui_ScreenMain` |
| Contenedor Ox | `ui_OxCont` | `lv_obj` | Área principal de la pantalla SpO₂ (785×443 px) |
| Panel info | `ui_Panel15` | `lv_obj` | Panel de datos numéricos de SpO₂ (336×129 px) |
| Label pulsioximetría | `ui_Label39` | `lv_label` | Texto "PULSIOXIMETRIA" / "PULSE OXIMETRY" |
| Label dato SpO₂ | `ui_Label5` | `lv_label` | Valor numérico de SpO₂ (Montserrat 26) |
| Label FC detectada | `ui_DetectOxi` | `lv_label` | Frecuencia cardíaca detectada (Montserrat 26) |
| Botón invisible | `ui_OxiButton2` | `lv_btn` | Botón transparente para interacción en el área SpO₂ |

---

## 7. Pantalla de Bloqueo — `ui_ScreenLock`

La pantalla de bloqueo se activa automáticamente por inactividad o manualmente. Muestra los valores actuales de forma resumida. Cualquier toque en la pantalla muestra el botón de desbloqueo.

### 7.1 Zona de valores detectados

#### Temperatura Aire — `ui_AirTempLockCont`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Label "TEMPERATURA AIRE:" | `ui_Label11` | `lv_label` | Etiqueta traducida del parámetro |
| Valor Temp Aire | `ui_Label18` | `lv_label` | Temperatura Aire detectada actualmente |
| Imagen viento | `ui_ImageWindLS` | `lv_img` | Icono visual de la variable Aire en la pantalla de bloqueo |

#### Temperatura Piel — `ui_SkinTempLockCont`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Label "TEMPERATURA BEBE:" | `ui_Label12` | `lv_label` | Etiqueta de temperatura de piel |
| Valor Temp Piel | `ui_Label14` | `lv_label` | Temperatura piel detectada actualmente |
| Imagen bebé | `ui_ImageBabyLS` | `lv_img` | Icono visual del bebé |

#### Humedad — `ui_HumLockCont`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Label "HUMEDAD:" | `ui_Label19` | `lv_label` | Etiqueta de humedad |
| Valor Humedad | `ui_Label20` | `lv_label` | Humedad detectada actualmente |
| Imagen agua | `ui_ImagenWaterLS` | `lv_img` | Icono de gota de agua |

### 7.2 Zona de valores objetivo (visible tras desbloqueo parcial)

#### Temperatura Aire objetivo — `ui_TargetAirTempCont`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Label "TEMP. OBJETIVO:" | `ui_TargetAirTempLabel` | `lv_label` | Etiqueta traducida |
| Valor Temp Aire deseada | `ui_TargetAirTempNumLabel` | `lv_label` | Temperatura objetivo del Aire |
| Flecha ajuste Aire | `ui_ArrowAirLock` | `lv_label` | Indicador visual de flecha en pantalla de bloqueo |

#### Temperatura Piel objetivo — `ui_TargetSkinTempCont`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Label "TEMP. OBJETIVO:" | `ui_TargetSkinTempLabel` | `lv_label` | Etiqueta traducida |
| Valor Temp Piel deseada | `ui_TargetSkinTempNumLabel` | `lv_label` | Temperatura objetivo de Piel |
| Flecha ajuste Piel | `ui_ArrowSkinLock` | `lv_label` | Indicador visual |

#### Humedad objetivo — `ui_HumLockDesiredCont`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Label "HUMEDAD OBJETIVO:" | `ui_Label23` | `lv_label` | Etiqueta traducida |
| Valor Humedad deseada | `ui_Label24` | `lv_label` | Humedad objetivo |
| Flecha ajuste Hum | `ui_ArrowHumLock` | `lv_label` | Indicador visual |

### 7.3 Contenedor de desbloqueo — `ui_UnlockCont`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Panel desbloqueo | `ui_Panel11` | `lv_obj` | Fondo del área de desbloqueo |
| Texto instrucción | `ui_Label4` | `lv_label` | Texto "PRESIONA 2 SEG PARA DESBLOQUEAR" / "PRESS 2 SEC TO UNLOCK" |
| Botón desbloqueo | `ui_LockButton2` | `lv_obj` | Área pulsable que al mantener 2 segundos desbloquea y navega a `ui_ScreenMain` |
| Arco de progreso | `ui_Spinner1` | `lv_arc` | Arco circular que muestra el progreso de desbloqueo (0-100% en 2 segundos). Oculto hasta que se presiona |

### 7.4 Estado y Fototerapia en pantalla de bloqueo

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Label estado | `ui_StatusLabel` | `lv_label` | Texto "ESTADO:" / "STATUS:" |
| Contenedor fototerapia | `ui_PhotoLockCont` | `lv_obj` | Muestra información del timer/estado de fototerapia |
| Label "FOTOTERAPIA:" | `ui_PhotoLockLabel` | `lv_label` | Etiqueta traducida |
| Valor timer foto | `ui_PhotoLockTimeLabel` | `lv_label` | Tiempo restante del timer ("H:MM") o "ON" si está en modo continuo |

### 7.5 Indicador de alarmas en pantalla de bloqueo — `ui_AlarmLockCont`

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Contenedor alarma lock | `ui_AlarmLockCont` | `lv_obj` | Panel de alarma en pantalla de bloqueo. Oculto si no hay alarmas. Al tocar navega a `ui_ScreenAlarms` |
| Imagen alarma lock | `ui_AlarmLockImg` | `lv_img` | Icono de campana/alarma. Es el elemento tappable |
| Panel fondo alarma | `ui_PanelLockAlarm` | `lv_obj` | Fondo rojo del indicador de alarma |
| Contador alarmas lock | `ui_AlarmLockNumLabel` | `lv_label` | Número de alarmas activas |
| Check OK lock | `ui_CheckImg` | `lv_img` | Icono check verde. Visible cuando no hay alarmas |

---

## 8. Elementos de Audio (Ocultos en producción)

Los siguientes elementos fueron creados para depuración del sistema de audio. Están **ocultados (`LV_OBJ_FLAG_HIDDEN`)** e integrados en `ui_ScreenSettings` pero no son accesibles por el usuario final.

| Elemento | Variable | Tipo | Descripción |
|---|---|---|---|
| Botón Play Audio | `ui_AudioPlayBtn` | `lv_btn` | Reproduce el tono de prueba |
| Label Play | `ui_AudioPlayLabel` | `lv_label` | Texto "Play Audio" |
| Botón Stop Audio | `ui_AudioStopBtn` | `lv_btn` | Para el audio. Fondo rojo. Oculto por defecto |
| Label Volumen | `ui_VolumeLabel` | `lv_label` | Texto "Vol: XX". Oculto |
| Botón Volumen + | `ui_VolumeUpBtn` | `lv_btn` | Sube el volumen (rango 1-21). Oculto |
| Botón Volumen - | `ui_VolumeDownBtn` | `lv_btn` | Baja el volumen. Oculto |

---

## 9. Resumen de Variables Globales de Estado de la UI

| Variable | Tipo | Descripción |
|---|---|---|
| `airTempValue` | `double` | Temperatura del Aire deseada (objetivo) |
| `skinTempValue` | `double` | Temperatura de Piel deseada (objetivo) |
| `humValue` | `int` | Humedad deseada (%) |
| `airTempValueDetected` | `double` | Temperatura del Aire medida por el sensor |
| `skinTempValueDetected` | `double` | Temperatura de Piel medida por el sensor |
| `humValueDetected` | `int` | Humedad medida por el sensor |
| `selectedPanel` | `int` | Panel activo: `AIR_PANEL_SELECTED`, `SKIN_PANEL_SELECTED` o `NO_PANEL_SELECTED` |
| `switchTemp` | `bool` | Estado del switch de temperatura |
| `switchHum` | `bool` | Estado del switch de humedad |
| `darkMode` | `bool` | Modo oscuro activado |
| `locked` | `bool` | Pantalla bloqueada |
| `photoTimerActive` | `bool` | Timer de fototerapia en marcha |
| `photoTimerMinutes` | `int` | Minutos configurados para el timer de fototerapia |
| `g_lang` | `ui_lang_t` | Idioma activo: `LANG_ES`, `LANG_EN`, `LANG_FR` |
| `alarmActive` | `bool` | Hay al menos una alarma activa |
| `alarmsMuted` | `bool` | Alarmas silenciadas por el usuario |
| `skinPanelEnabled` | `bool` | El modo piel está habilitado desde Ajustes |

---

## 10. Funciones de Actualización de la UI (Referencia rápida)

| Función | Descripción |
|---|---|
| `update_labels()` | Actualiza todos los labels con los valores de temperatura, humedad y timer |
| `UI_ApplyLanguage(lang)` | Aplica el idioma a todos los elementos de texto. Guarda en EEPROM |
| `UI_ApplyTheme()` | Aplica el tema claro/oscuro a todos los paneles y fondos |
| `UI_SyncAll()` | Sincroniza el estado de la UI con las variables globales (switches, colores, visibilidad) |
| `update_alarm_panels()` | Actualiza las filas de alarmas: muestra/oculta, asigna textos y activa parpadeo |
| `update_history_charts()` | Recarga las gráficas históricas según el rango seleccionado en el dropdown |
| `chart_add_air_temp(v)` | Agrega un punto a la gráfica de temperatura Aire en tiempo real |
| `chart_add_skin_temp(v)` | Agrega un punto a la gráfica de temperatura Piel en tiempo real |
| `chart_add_hum_value(v)` | Agrega un punto a la gráfica de humedad en tiempo real |
| `chart_save_history()` | Guarda una muestra en el búfer circular histórico (cada ~10 s) |
