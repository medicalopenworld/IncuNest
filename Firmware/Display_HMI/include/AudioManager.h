#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include "Audio.h"

class AudioManager {
public:
    static AudioManager& getInstance();
    
    void begin();
    void loop();
    void playTone();
    void playUrl(const char* url);
    void stop();
    void setVolume(uint8_t volume);
    bool isPlaying();
    static void audioTask(void* pvParameters);

private:
    AudioManager();
    Audio audio;
    
    // Pines I2S definitivos para CrowPanel 7.0 Advance
    const int I2S_DOUT = 4;
    const int I2S_BCLK = 5;
    const int I2S_LRCK = 6;
};

#endif
