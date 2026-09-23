#include <Wire.h>
#include "buzzer.h"
#include "main.h"

// Zumbador del propio display (STC8H1K28 por I2C, 0x30). Es hardware DISTINTO
// del zumbador de la motherBoard: aquel da las senales de alarma de la Tabla 3
// y este solo da el chasquido de confirmacion al pulsar. No se pisan.
//
// buzzerOn() estuvo neutralizado un tiempo ("queremos el buzzer OFF"), lo que
// dejaba el display mudo del todo. Se reactiva porque sin el no habia forma de
// saber si una pulsacion se habia registrado — el caso que lo destapo fue el
// boton de SILENCIAR, cuyo efecto tarda en verse porque la placa es la que
// confirma el estado.
void buzzerOn() {
    Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
    Wire.write(I2C_CMD_BUZZER_ON);
    Wire.endTransmission();
}

// Desactivar buzzer (Forzar OFF)
void buzzerOff() {
    Wire.beginTransmission(I2C_ADDR_BACKLIGHT);
    Wire.write(I2C_CMD_BUZZER_OFF);
    Wire.endTransmission();
}
