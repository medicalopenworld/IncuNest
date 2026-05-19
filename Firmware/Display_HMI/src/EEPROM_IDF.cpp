#ifdef USE_IDF_FRAMEWORK
#include "EEPROM_IDF.h"
#include "EEPROM_defines.h"
#include "main.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <cstring>
#include <cmath>
#include <string>

static const char *TAG = "EEPROM_IDF";

EEPROM_IDF_Class EEPROM;

// ---------------------------------------------------------------------------
// EEPROM_IDF_Class — NVS-backed, binary-compatible with Arduino EEPROM blob
// ---------------------------------------------------------------------------

bool EEPROM_IDF_Class::begin(size_t size) {
    _size = size;
    _buf = (uint8_t *)malloc(size);
    if (!_buf) return false;
    memset(_buf, 0, size);

    nvs_handle_t h;
    esp_err_t err = nvs_open("eeprom", NVS_READWRITE, &h);
    if (err == ESP_OK) {
        size_t len = size;
        err = nvs_get_blob(h, "data", _buf, &len);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "NVS read: %s", esp_err_to_name(err));
        }
        nvs_close(h);
    }
    _dirty = false;
    return true;
}

uint8_t EEPROM_IDF_Class::read(int addr) const {
    if (!_buf || addr < 0 || (size_t)addr >= _size) return 0;
    return _buf[addr];
}

void EEPROM_IDF_Class::write(int addr, uint8_t val) {
    if (!_buf || addr < 0 || (size_t)addr >= _size) return;
    if (_buf[addr] != val) { _buf[addr] = val; _dirty = true; }
}

bool EEPROM_IDF_Class::commit() {
    if (!_dirty || !_buf) return true;
    nvs_handle_t h;
    esp_err_t err = nvs_open("eeprom", NVS_READWRITE, &h);
    if (err != ESP_OK) return false;
    err = nvs_set_blob(h, "data", _buf, _size);
    nvs_commit(h);
    nvs_close(h);
    _dirty = false;
    return (err == ESP_OK);
}

float EEPROM_IDF_Class::readFloat(int addr) const {
    float v = 0.0f;
    if (_buf && addr >= 0 && (size_t)(addr + (int)sizeof(float)) <= _size)
        memcpy(&v, _buf + addr, sizeof(float));
    return v;
}

void EEPROM_IDF_Class::writeFloat(int addr, float v) {
    if (!_buf || addr < 0 || (size_t)(addr + (int)sizeof(float)) > _size) return;
    if (memcmp(_buf + addr, &v, sizeof(float)) != 0) {
        memcpy(_buf + addr, &v, sizeof(float)); _dirty = true;
    }
}

int EEPROM_IDF_Class::readInt(int addr) const {
    int v = 0;
    if (_buf && addr >= 0 && (size_t)(addr + (int)sizeof(int)) <= _size)
        memcpy(&v, _buf + addr, sizeof(int));
    return v;
}

void EEPROM_IDF_Class::writeInt(int addr, int v) {
    if (!_buf || addr < 0 || (size_t)(addr + (int)sizeof(int)) > _size) return;
    if (memcmp(_buf + addr, &v, sizeof(int)) != 0) {
        memcpy(_buf + addr, &v, sizeof(int)); _dirty = true;
    }
}

uint16_t EEPROM_IDF_Class::readUShort(int addr) const {
    uint16_t v = 0;
    if (_buf && addr >= 0 && (size_t)(addr + (int)sizeof(uint16_t)) <= _size)
        memcpy(&v, _buf + addr, sizeof(uint16_t));
    return v;
}

void EEPROM_IDF_Class::writeUShort(int addr, uint16_t v) {
    if (!_buf || addr < 0 || (size_t)(addr + (int)sizeof(uint16_t)) > _size) return;
    if (memcmp(_buf + addr, &v, sizeof(uint16_t)) != 0) {
        memcpy(_buf + addr, &v, sizeof(uint16_t)); _dirty = true;
    }
}

std::string EEPROM_IDF_Class::readString(int addr) const {
    if (!_buf || addr < 0 || (size_t)addr >= _size) return "";
    size_t maxlen = _size - (size_t)addr;
    const char *p = (const char *)(_buf + addr);
    size_t len = strnlen(p, maxlen);
    return std::string(p, len);
}

void EEPROM_IDF_Class::writeString(int addr, const std::string &s) {
    if (!_buf || addr < 0 || (size_t)addr >= _size) return;
    size_t maxlen = _size - (size_t)addr;
    size_t wlen = (s.length() < maxlen - 1) ? s.length() : maxlen - 1;
    if (memcmp(_buf + addr, s.c_str(), wlen) != 0 || _buf[addr + wlen] != 0) {
        memcpy(_buf + addr, s.c_str(), wlen);
        _buf[addr + wlen] = '\0';
        _dirty = true;
    }
}

// ---------------------------------------------------------------------------
// IDF init/recap/defaults — mirrors EEPROM.cpp (Arduino path)
// ---------------------------------------------------------------------------

extern double airTempValue, skinTempValue;
extern int    humValue;
extern ui_lang_t g_lang;
extern bool   darkMode;
extern bool   humidityEnabled;
extern int    photoTimerMinutes;
extern char   wifi_ssid[64];
extern char   wifi_pass[64];

void loaddefaultValues() {
    EEPROM.write(EEPROM_LANGUAGE, LANG_EN);
    EEPROM.writeFloat(EEPROM_DESIRED_AIR_TEMP, (float)DEFAULT_AIR_TEMP);
    EEPROM.writeFloat(EEPROM_DESIRED_SKIN_TEMP, (float)DEFAULT_SKIN_TEMP);
    EEPROM.write(EEPROM_DESIRED_HUMIDITY, DEFAULT_HUMIDITY);
    EEPROM.write(EEPROM_PHOTO_TIMER_MINUTES, PHOTO_TIMER_EEPROM_DEFAULT);
    EEPROM.write(EEPROM_DARK_MODE, 0);
    EEPROM.write(EEPROM_HUMIDITY_ENABLED, 0);
    EEPROM.commit();
}

void recapVariables() {
    g_lang = (ui_lang_t)EEPROM.read(EEPROM_LANGUAGE);
    if (g_lang > LANG_FR || g_lang < LANG_ES) g_lang = LANG_EN;

    airTempValue = EEPROM.readFloat(EEPROM_DESIRED_AIR_TEMP);
    skinTempValue = EEPROM.readFloat(EEPROM_DESIRED_SKIN_TEMP);
    humValue = EEPROM.read(EEPROM_DESIRED_HUMIDITY);
    photoTimerMinutes = EEPROM.read(EEPROM_PHOTO_TIMER_MINUTES);
    darkMode = EEPROM.read(EEPROM_DARK_MODE) == 1;
    humidityEnabled = EEPROM.read(EEPROM_HUMIDITY_ENABLED) == 1;

    std::string ssid = EEPROM.readString(EEPROM_WIFI_SSID);
    std::string pass = EEPROM.readString(EEPROM_WIFI_PASSWORD);
    strncpy(wifi_ssid, ssid.c_str(), sizeof(wifi_ssid) - 1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';
    strncpy(wifi_pass, pass.c_str(), sizeof(wifi_pass) - 1);
    wifi_pass[sizeof(wifi_pass) - 1] = '\0';

    float av = (float)airTempValue, sv = (float)skinTempValue;
    if (std::isnan(av) || av < (float)AIR_TEMP_MIN || av > (float)AIR_TEMP_MAX)
        airTempValue = DEFAULT_AIR_TEMP;
    if (std::isnan(sv) || sv < (float)SKIN_TEMP_MIN || sv > (float)SKIN_TEMP_MAX)
        skinTempValue = DEFAULT_SKIN_TEMP;
    if (humValue < HUM_MIN || humValue > HUM_MAX) humValue = DEFAULT_HUMIDITY;
    if (photoTimerMinutes < PHOTO_TIMER_MIN_MINUTES ||
        photoTimerMinutes > PHOTO_TIMER_MAX_MINUTES)
        photoTimerMinutes = PHOTO_TIMER_EEPROM_DEFAULT;

    in3.serialNumber = EEPROM.readInt(EEPROM_SERIAL_NUMBER);

    ESP_LOGI(TAG, "lang=%d SN=%d airT=%.2f skinT=%.2f hum=%d wifi=%s",
             g_lang, in3.serialNumber, airTempValue, skinTempValue, humValue,
             wifi_ssid);
}

void resetFlash() {
    for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
    EEPROM.commit();
}

void initEEPROM() {
    if (!EEPROM.begin(EEPROM_SIZE)) {
        ESP_LOGE(TAG, "Failed to initialise EEPROM");
        return;
    }
    if (EEPROM.read(EEPROM_FIRST_TURN_ON)) {
        resetFlash();
        loaddefaultValues();
        ESP_LOGI(TAG, "[FLASH] First turn on — loading defaults");
    } else {
        ESP_LOGI(TAG, "[FLASH] Loading stored variables");
        recapVariables();
    }
    ESP_LOGI(TAG, "[FLASH] Variables loaded");
}

#endif // USE_IDF_FRAMEWORK
