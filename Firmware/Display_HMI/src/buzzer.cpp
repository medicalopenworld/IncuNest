#include <Wire.h>
#include "buzzer.h"
#include "main.h"

// Activar buzzer (NEUTRALIZADO para silencio total)
void buzzerOn() {
    // Comentado por petición del usuario: queremos el buzzer OFF
    /*
    Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
    Wire.write(I2C_CMD_BUZZER_ON);
    Wire.endTransmission();
    */
}

// Desactivar buzzer (Forzar OFF)
void buzzerOff() {
    Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
    Wire.write(I2C_CMD_BUZZER_OFF);
    Wire.endTransmission();
}
