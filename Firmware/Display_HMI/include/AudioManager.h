#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

// Pantalla antigua no tiene hardware I2S — stub vacío
class AudioManager {
public:
    static AudioManager& getInstance() {
        static AudioManager instance;
        return instance;
    }

    void begin() {}
    void loop() {}
    void playTone() {}
    void playUrl(const char* url) { (void)url; }
    void stop() {}
    void setVolume(uint8_t volume) { _volume = volume > 21 ? 21 : volume; }
    bool isPlaying() { return false; }
    bool isLooping() { return false; }
    uint8_t getVolume() { return _volume; }
    static void audioTask(void* pvParameters) { (void)pvParameters; }

private:
    AudioManager() {}
    uint8_t _volume = 15;
};

#endif
