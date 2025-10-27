#include <Wire.h>
#include "buzzer.h"

#define HMI_MXOR_ADDR 0x30  // dirección I2C del Crowpanel

// Activar buzzer
void buzzerOn() {
    Wire.beginTransmission(HMI_MXOR_ADDR);
    Wire.write(246); // comando para encender buzzer
    Wire.endTransmission();
}

// Desactivar buzzer
void buzzerOff() {
    Wire.beginTransmission(HMI_MXOR_ADDR);
    Wire.write(247); // comando para apagar buzzer
    Wire.endTransmission();
}
