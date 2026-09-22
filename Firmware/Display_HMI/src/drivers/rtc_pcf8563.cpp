#include "rtc_pcf8563.h"

#include "rtc_pcf8563_codec.h"

#include <Wire.h>

// Registro de control 1. Se comprueba al escribir: el bit STOP (0x20) para el
// contador, y un chip parado devolveria siempre la misma hora sin que nada lo
// delate — VL seguiria a 0 porque la pila esta bien.
#define PCF8563_REG_CONTROL1 0x00
#define PCF8563_CONTROL1_STOP 0x20

// ---- Acceso al bus -----------------------------------------------------------
//
// En el port a ESP-IDF esto hablaba con `g_i2c` (I2cBus de platform/plat_i2c.h,
// con su propio cerrojo). En esta linea el HMI usa la `Wire` de Arduino
// directamente desde varias tareas —tactil en UITask, retroiluminacion y
// zumbador en AudioManager/UITask— sin cerrojo comun, y el RTC entra en el
// mismo bus (TOUCH_SDA_PIN/TOUCH_SCL_PIN, Wire.begin() en main.cpp) por la
// misma via. Las cuatro primitivas replican la semantica de I2cBus: cada una
// es una transaccion completa y devuelve false ante cualquier NACK o lectura
// corta, que el llamador trata como "sin hora utilizable", nunca como datos.

static bool i2cProbe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const size_t got = Wire.requestFrom((int)addr, (int)n);
  if (got != n) {
    // Vaciar lo que haya llegado para no dejar bytes huerfanos en el bufer.
    while (Wire.available()) {
      (void)Wire.read();
    }
    return false;
  }
  for (size_t i = 0; i < n; i++) {
    const int b = Wire.read();
    if (b < 0) {
      return false;
    }
    buf[i] = (uint8_t)b;
  }
  return true;
}

static bool i2cWrite(uint8_t addr, const uint8_t *buf, size_t n) {
  Wire.beginTransmission(addr);
  if (Wire.write(buf, n) != n) {
    (void)Wire.endTransmission();
    return false;
  }
  return Wire.endTransmission() == 0;
}

static bool i2cWriteReg8(uint8_t addr, uint8_t reg, uint8_t val) {
  const uint8_t msg[2] = {reg, val};
  return i2cWrite(addr, msg, sizeof(msg));
}

// ------------------------------------------------------------------------------

bool rtcPresent(void) { return i2cProbe(PCF8563_I2C_ADDR); }

// Una lectura cruda de los siete registros de tiempo.
static bool readRaw(uint8_t regs[PCF8563_TIME_REG_COUNT]) {
  return i2cReadReg(PCF8563_I2C_ADDR, PCF8563_TIME_REG_FIRST, regs,
                    PCF8563_TIME_REG_COUNT);
}

bool rtcRead(uint32_t *outEpoch) {
  if (outEpoch == nullptr) {
    return false;
  }
  uint8_t first[PCF8563_TIME_REG_COUNT] = {0};
  if (!readRaw(first)) {
    return false;
  }
  // El PCF8563 no tiene doble bufer: los registros se leen del contador vivo,
  // asi que una lectura puede quedar a caballo de un incremento y devolver el
  // minuto ya actualizado con el segundo aun sin actualizar. Si el segundo
  // cambio entre dos muestreos, el primero no es de fiar; se toma el segundo,
  // que no puede estar a caballo del mismo incremento.
  uint8_t second[PCF8563_TIME_REG_COUNT] = {0};
  if (!readRaw(second)) {
    return false;
  }
  const uint8_t *use = (first[0] == second[0]) ? first : second;

  return pcf8563_decode_time(use, outEpoch);
}

bool rtcWrite(uint32_t epoch) {
  uint8_t regs[PCF8563_TIME_REG_COUNT] = {0};
  if (!pcf8563_encode_time(epoch, regs)) {
    return false; // epoch no representable; el chip no se toca
  }

  // Un solo mensaje: direccion del primer registro seguida de los siete bytes.
  // Escribirlos de uno en uno dejaria el chip con una hora a medias si el bus
  // fallara por el camino, y ademas ocuparia el bus del tactil siete veces.
  uint8_t msg[1 + PCF8563_TIME_REG_COUNT];
  msg[0] = PCF8563_TIME_REG_FIRST;
  for (int i = 0; i < PCF8563_TIME_REG_COUNT; i++) {
    msg[1 + i] = regs[i];
  }
  if (!i2cWrite(PCF8563_I2C_ADDR, msg, sizeof(msg))) {
    return false;
  }

  // El contador tiene que estar en marcha. Si alguien dejo el bit STOP puesto
  // —un arranque a medias, un chip de otra unidad— la hora quedaria congelada
  // en lo que acabamos de escribir, y nada lo delataria: VL seguiria a 0
  // porque la pila esta bien. Se comprueba aqui, que es la unica ruta que
  // escribe el chip.
  uint8_t ctrl1 = 0;
  if (i2cReadReg(PCF8563_I2C_ADDR, PCF8563_REG_CONTROL1, &ctrl1, 1) &&
      (ctrl1 & PCF8563_CONTROL1_STOP) != 0) {
    i2cWriteReg8(PCF8563_I2C_ADDR, PCF8563_REG_CONTROL1,
                 (uint8_t)(ctrl1 & ~PCF8563_CONTROL1_STOP));
  }
  return true;
}
