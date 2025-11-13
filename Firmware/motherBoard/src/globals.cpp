// globals.cpp

// Variables de encoder
volatile unsigned long lastEncPulse = 0;
volatile unsigned long lastEncMove = 0;
bool EncMoveOrientation = false;
bool EncMove = false;

// Variables de pantalla / backlight
unsigned long lastbacklightHandler = 0;
int ScreenBacklightMode = 0;

// Variables de sensores
float previousHumidity = 0;
float humidityPercentage = 0;
float temperaturePercentage = 0;
unsigned long lastSuccesfullSensorUpdate = 0;
int errorTemperature = 0;

// Variables de PID / humidificador
bool humidifierState = false;
bool humidifierStateChange = false;

// Variables EEPROM
float presetTemp = 0;
float RawTemperatureLow = 0;
float RawTemperatureRange = 0;
float ReferenceTemperatureRange = 0;
float ReferenceTemperatureLow = 0;
float fineTuneSkinTemperature = 0;
float fineTuneAirTemperature = 0;

// Variables UI
const char* textToWrite = "";
int tempBarPosY = 0;
int humBarPosY = 0;
bool state_blink = false;
float temperatureAtStart = 0;
float humidityAtStart = 0;
char* print_text = nullptr;
int pos_text = 0;
int menu_rows = 0;
int bar_pos = 0;
int ypos = 0;
float minDesiredTemp = 0;
float maxDesiredTemp = 0;

// Variables de calibración
bool autoCalibrationProcess = false;
float provisionalReferenceTemperatureLow = 0;
float provisionalRawTemperatureLow = 0;

// Variables de interfaz gráfica
uint16_t screenTextColor = 0xFFFF;
uint16_t screenTextBackgroundColour = 0x0000;
int humidityX = 0, humidityY = 0;
int temperatureX = 0, temperatureY = 0;
int barWidth = 0, barHeight = 0;
int tempBarPosX = 0, humBarPosX = 0;
int separatorTopYPos = 0, separatorMidYPos = 0;
const char* helpMessage = "";
int length = 0;
bool blinking = false;
const char* cstring = "";
bool blinkSetMessageState = false;
int initialSensorPosition = 0;

// GPIO encoder
int encoderpinA = 0;
int encoderpinB = 0;

// Otros
unsigned long lastGraphicSensorsUpdate = 0;
unsigned long lastBlinkSetMessage = 0;
bool enableSetProcess = false;
