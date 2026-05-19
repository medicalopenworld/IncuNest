#pragma once
#ifdef USE_IDF_FRAMEWORK
#include <cstring>
#include <string>

class EEPROM_IDF_Class {
    uint8_t  *_buf  = nullptr;
    size_t    _size = 0;
    bool      _dirty = false;

public:
    bool begin(size_t size);
    uint8_t read(int addr) const;
    void write(int addr, uint8_t val);
    bool commit();

    float    readFloat(int addr) const;
    void     writeFloat(int addr, float v);
    int      readInt(int addr) const;
    void     writeInt(int addr, int v);
    uint16_t readUShort(int addr) const;
    void     writeUShort(int addr, uint16_t v);
    std::string readString(int addr) const;
    void     writeString(int addr, const std::string& s);

    template<typename T>
    void put(int addr, const T& v) {
        if (!_buf || addr < 0 || (size_t)(addr + sizeof(v)) > _size) return;
        if (memcmp(_buf + addr, &v, sizeof(v)) != 0) {
            memcpy(_buf + addr, &v, sizeof(v)); _dirty = true;
        }
    }
    template<typename T>
    void get(int addr, T& v) {
        if (!_buf || addr < 0 || (size_t)(addr + sizeof(v)) > _size) return;
        memcpy(&v, _buf + addr, sizeof(v));
    }
};

extern EEPROM_IDF_Class EEPROM;
#endif // USE_IDF_FRAMEWORK
