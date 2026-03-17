#include <Wire.h>
#include "buzzer.h"

#define HMI_MXOR_ADDR 0x30  // dirección I2C del Crowpanel

// Activar buzzer (NEUTRALIZADO para silencio total)
void buzzerOn() {
    // Comentado por petición del usuario: queremos el buzzer OFF
    /*
    Wire.beginTransmission(HMI_MXOR_ADDR);
    Wire.write(246); 
    Wire.endTransmission();
    */
}

// Desactivar buzzer (Forzar OFF)
void buzzerOff() {
    Wire.beginTransmission(HMI_MXOR_ADDR);
    Wire.write(247); // comando para apagar buzzer v1.3
    Wire.endTransmission();
}
