#include "AudioManager.h"
#include <SPIFFS.h>
#include <Wire.h>
#include "esp_log.h"
#include <EEPROM.h>
#include "EEPROM_defines.h"

static const char* TAG = "Audio";

AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

AudioManager::AudioManager() {
}

void AudioManager::begin() {
    Serial.println("AudioManager: Initializing...");
    
    // Escáner I2C para identificar hardware de audio
    Serial.println("AudioManager: Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("AudioManager: I2C device found at 0x%02X\n", addr);
        }
    }

    // ACTIVACIÓN DE ENERGÍA v1.3 (STC8 @ 0x30)
    // Según doc v1.3: 246=Buzzer ON, 247=Buzzer OFF.
    // Deducimos 248=Speaker ON, 249=Speaker OFF basado en la secuencia.
    uint8_t wakeup_cmds[] = {247, 248}; // Apagar buzzer (247), encender speaker (248)
    for(uint8_t cmd : wakeup_cmds) {
        Wire.beginTransmission(0x30);
        Wire.write(cmd);
        Wire.endTransmission();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    // Evitamos el comando 0x19 que podría ser inestable

    if(!SPIFFS.begin(true)){
        Serial.println("AudioManager: SPIFFS Mount Failed");
    } else {
        Serial.println("AudioManager: SPIFFS mounted");
        // Verificamos si existe el MP3
        if (SPIFFS.exists("audio")) {
            Serial.println("AudioManager: sapphire.mp3 FOUND in SPIFFS");
        } else {
            Serial.println("AudioManager: WARNING, audio NOT FOUND. Run 'Upload File System Image'.");
        }
    }

    // Configuración Inicial I2S (Pins 4, 5, 6 - Safe zone)
    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
    audio.forceMono(true);
    
    // Leer volumen guardado en EEPROM (0-21). Si es 0 o >21, usar default 15.
    uint8_t savedVol = EEPROM.read(EEPROM_AUDIO_VOLUME);
    if (savedVol == 0 || savedVol > 21) savedVol = 15;
    _volume = savedVol;
    audio.setVolume(_volume);
    Serial.printf("AudioManager: Volume loaded from EEPROM: %d\n", _volume);
    Serial.println("AudioManager: Initialized pins BCLK:5, LRCK:6, DOUT:4 (Safe)");

    // Crear tarea de audio en el Core 0 (Sistemas)
    // NOTA: Prioridad 2 (igual que UI) para evitar contención de DMA con el bus RGB del LCD
    xTaskCreatePinnedToCore(
        audioTask,
        "AudioTask",
        8192,  // Stack 8K para MP3
        NULL,
        2,     // Prioridad igual que UI (evita monopolizar DMA y causar temblor de pantalla)
        NULL,
        0      // Core 0
    );
}

// Tarea estática para manejar el bucle de audio
void AudioManager::audioTask(void* pvParameters) {
    Serial.println("AudioManager: Audio Task started on Core 0");
    for(;;) {
        // BALANCE DMA: 3 loops + 3ms ≈ 1000 ciclos/s.
        // Suficiente para rellenar el buffer I2S del MP3 (vs 15+1ms=15000/s que temblaba pantalla).
        for(int i = 0; i < 3; i++) {
            AudioManager::getInstance().audio.loop();
        }
        
        // 3ms da al DMA del Bus_RGB tiempo para operar sin interferencias.
        vTaskDelay(pdMS_TO_TICKS(3));
    }
}

void AudioManager::loop() {
    // Ya no es necesario llamar a loop() externamente
}

void AudioManager::playTone() {
    Serial.println("AudioManager: Starting LOCAL playback...");
    
    // RE-ACTIVACIÓN DE ENERGÍA v1.3 (Speaker ON: 248)
    Wire.beginTransmission(0x30);
    Wire.write(248);
    Wire.endTransmission();
    vTaskDelay(pdMS_TO_TICKS(10));

    // RE-CONFIGURAR I2S: Asegurar pins safe 5, 6, 4
    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
    audio.setVolume(_volume);

    if(SPIFFS.exists("audio")){
        if(!audio.connecttoFS(SPIFFS, "audio")) {
            Serial.println("AudioManager: Failed to connect to audio");
        }
    } else {
        Serial.println("AudioManager: Error, audio not found!");
    }
}

void AudioManager::playUrl(const char* url) {
    if (url) {
        // RE-ACTIVACIÓN DE ENERGÍA v1.3 (Speaker ON: 248)
        Wire.beginTransmission(0x30);
        Wire.write(248);
        Wire.endTransmission();
        
        audio.connecttohost(url);
    }
}

void AudioManager::stop() {
    audio.stopSong();
    // Apagar speaker físicamente para ahorrar energía/evitar ruidos
    Wire.beginTransmission(0x30);
    Wire.write(249); 
    Wire.endTransmission();
    Serial.println("AudioManager: Playback stopped and speaker OFF (249)");
}

void AudioManager::setVolume(uint8_t volume) {
    if (volume > 21) volume = 21;
    _volume = volume;
    audio.setVolume(_volume);
    // NOTA: El guardado en EEPROM se hace desde la UI con commit diferido
    // para evitar bloquear el bus flash y causar temblor de pantalla.
    Serial.printf("AudioManager: Volume set to %d\n", _volume);
}

uint8_t AudioManager::getVolume() {
    return _volume;
}

bool AudioManager::isPlaying() {
    return audio.isRunning();
}

// Callbacks de la librería Audio
void audio_info(const char *info){
    Serial.print("audio_info: "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data:     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3:     ");Serial.println(info);
}
void audio_showstation(const char *info){
    Serial.print("station :    ");Serial.println(info);
}
void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle: ");Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate:     ");Serial.println(info);
}
void audio_commercial(const char *info){  //length in sec
    Serial.print("commercial:  ");Serial.println(info);
}
void audio_terriblefree(const char *info){
    Serial.print("terriblefree:");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost:    ");Serial.println(info);
}
