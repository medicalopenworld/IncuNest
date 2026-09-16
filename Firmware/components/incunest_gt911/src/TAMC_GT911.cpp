// PARCHE INCUNEST: Arduino -> capa de plataforma propia
#include "platform/plat_gpio.h"
#include "platform/plat_num.h"
#include "platform/plat_time.h"
#include "platform/plat_types.h"
#include <TAMC_GT911.h>
#include "platform/plat_i2c.h"  // PARCHE INCUNEST

TAMC_GT911::TAMC_GT911(uint8_t _sda, uint8_t _scl, uint8_t _int, uint8_t _rst,
                       uint16_t _width, uint16_t _height)
    : pinSda(_sda), pinScl(_scl), pinInt(_int), pinRst(_rst), width(_width),
      height(_height) {}

bool TAMC_GT911::begin(uint8_t _addr) {
  addr = _addr;
  // Wire.begin(pinSda, pinScl); // Removed to avoid re-init if already done via
  // main.cpp or just check Better to rely on external Wire.begin() or check if
  // initialized. For safety in this library context, we can keep it OR assume
  // user did it. Given user code does Wire.begin(sda,scl) in setup(), we can
  // skip re-call or keep it. Keeping it is safer for "library standalone" usage
  // but might reset bus speed. Let's call it to be sure pins are set for this
  // driver instance.
  Wire.begin(pinSda, pinScl);

  reset();

  // Validate connection by reading Product ID
  uint8_t temp[4];
  readBlockData(temp, GT911_PRODUCT_ID, 4);

  // Serial.print("Product ID: ");
  // Serial.print((char)temp[0]); Serial.print((char)temp[1]);
  // Serial.print((char)temp[2]); Serial.println((char)temp[3]);

  // Check if ID is somewhat valid (printable ASCII or known "911")
  // GT911 usually returns "911\0"
  if (temp[0] != '9') {
    // Try secondary address if primary failed
    if (addr == GT911_ADDR1)
      addr = GT911_ADDR2;
    else
      addr = GT911_ADDR1;

    reset(); // Reset again with new address intent logic?
    // Actually reset() uses 'addr' to decide INT pin state. So changing addr
    // and calling reset() effectively tries the other strap.

    readBlockData(temp, GT911_PRODUCT_ID, 4);
    if (temp[0] != '9') {
      return false; // Failed both addresses
    }
  }
  return true;
}
void TAMC_GT911::reset() {
  pin_mode(pinInt, PIN_MODE_OUTPUT);
  pin_mode(pinRst, PIN_MODE_OUTPUT);
  pin_write(pinInt, 0);
  pin_write(pinRst, 0);
  delay_ms(10);
  pin_write(pinInt, addr == GT911_ADDR2);
  delay_ms(1);
  pin_write(pinRst, 1);
  delay_ms(5);
  pin_write(pinInt, 0);
  delay_ms(50);
  pin_mode(pinInt, PIN_MODE_INPUT);
  // attachInterrupt(pinInt, TAMC_GT911::onInterrupt, RISING);
  delay_ms(50);
  readBlockData(configBuf, GT911_CONFIG_START, GT911_CONFIG_SIZE);
  setResolution(width, height);
}
void TAMC_GT911::calculateChecksum() {
  uint8_t checksum;
  for (uint8_t i = 0; i < GT911_CONFIG_SIZE; i++) {
    checksum += configBuf[i];
  }
  checksum = (~checksum) + 1;
  configBuf[GT911_CONFIG_CHKSUM - GT911_CONFIG_START] = checksum;
}
// void ARDUINO_ISR_ATTR TAMC_GT911::onInterrupt() {
//   read();
//   TAMC_GT911::onRead();
// }
void TAMC_GT911::reflashConfig() {
  calculateChecksum();
  writeByteData(GT911_CONFIG_CHKSUM,
                configBuf[GT911_CONFIG_CHKSUM - GT911_CONFIG_START]);
  writeByteData(GT911_CONFIG_FRESH, 1);
}
void TAMC_GT911::setRotation(uint8_t rot) { rotation = rot; }
void TAMC_GT911::setResolution(uint16_t _width, uint16_t _height) {
  configBuf[GT911_X_OUTPUT_MAX_LOW - GT911_CONFIG_START] = lowByte(_width);
  configBuf[GT911_X_OUTPUT_MAX_HIGH - GT911_CONFIG_START] = highByte(_width);
  configBuf[GT911_Y_OUTPUT_MAX_LOW - GT911_CONFIG_START] = lowByte(_height);
  configBuf[GT911_Y_OUTPUT_MAX_HIGH - GT911_CONFIG_START] = highByte(_height);
  reflashConfig();
}
// void TAMC_GT911::setOnRead(void (*isr)()) {
//   onRead = isr;
// }
// Lecturas del tactil que el bus no ha contestado. Se expone en
// /debug/state del display (campo touch.read_fail) porque es la unica forma de
// distinguir "la interfaz se mueve sola" de "alguien la esta tocando": sin
// este numero, el sintoma parece del panel y no del I2C.
extern "C" {
uint32_t gt911_read_failures = 0;
// Flancos "sin toque -> toque" y ultimo punto entregado. Con estos tres, la
// pregunta "la interfaz se mueve sola porque alguien la toca o porque el
// tactil se lo inventa" se responde mirando un numero en vez de discutiendo:
// si gt911_press_events sube con nadie delante, son toques fantasma.
uint32_t gt911_press_events = 0;
uint16_t gt911_last_x = 0;
uint16_t gt911_last_y = 0;
uint8_t  gt911_touched_now = 0;
}

void TAMC_GT911::read(void) {
  // Serial.println("TAMC_GT911::read");
  uint8_t data[7];
  uint8_t id;
  uint16_t x, y, size;

  bool infoOk = false;
  uint8_t pointInfo = readByteData(GT911_POINT_INFO, &infoOk);
  if (!infoOk) {
    // El bus no ha contestado: no se sabe nada del tactil, asi que se declara
    // NO TOCADO y se sale sin escribir el registro de fin de lectura. Antes
    // esta rama no existia y el 0xFF de la lectura fallida se colaba como un
    // toque con coordenadas basura (ver la nota de la cabecera).
    isTouched = false;
    touches = 0;
    gt911_read_failures++;
    return;
  }
  uint8_t bufferStatus = pointInfo >> 7 & 1;
  uint8_t proximityValid = pointInfo >> 5 & 1;
  uint8_t haveKey = pointInfo >> 4 & 1;
  isLargeDetect = pointInfo >> 6 & 1;
  touches = pointInfo & 0xF;
  // Serial.print("bufferStatus: ");Serial.println(bufferStatus);
  // Serial.print("largeDetect: ");Serial.println(isLargeDetect);
  // Serial.print("proximityValid: ");Serial.println(proximityValid);
  // Serial.print("haveKey: ");Serial.println(haveKey);
  // Serial.print("touches: ");Serial.println(touches);
  const bool wasTouched = isTouched;
  isTouched = touches > 0;
  gt911_touched_now = isTouched ? 1 : 0;
  if (isTouched && !wasTouched) {
    gt911_press_events++;
  }
  if (isTouched) {
    if (touches > 5) {
      touches = 5;
    }
    if (bufferStatus == 1) {
      for (uint8_t i = 0; i < touches; i++) {
        if (!readBlockData(data, GT911_POINT_1 + i * 8, 7)) {
          // Media lectura de coordenadas no vale: mejor ningun toque que uno
          // en un punto inventado.
          isTouched = false;
          touches = 0;
          gt911_read_failures++;
          break;
        }
        points[i] = readPoint(data);
        if (i == 0) {
          gt911_last_x = points[0].x;
          gt911_last_y = points[0].y;
        }
      }
    }
  }
  writeByteData(GT911_POINT_INFO, 0);
}
TP_Point TAMC_GT911::readPoint(uint8_t *data) {
  uint16_t temp;
  uint8_t id = data[0];
  uint16_t x = data[1] + (data[2] << 8);
  uint16_t y = data[3] + (data[4] << 8);
  uint16_t size = data[5] + (data[6] << 8);
  switch (rotation) {
  case ROTATION_NORMAL:
    x = width - x;
    y = height - y;
    break;
  case ROTATION_LEFT:
    temp = x;
    x = width - y;
    y = temp;
    break;
  case ROTATION_INVERTED:
    x = x;
    y = y;
    break;
  case ROTATION_RIGHT:
    temp = x;
    x = y;
    y = height - temp;
    break;
  default:
    break;
  }
  return TP_Point(id, x, y, size);
}
void TAMC_GT911::writeByteData(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(highByte(reg));
  Wire.write(lowByte(reg));
  Wire.write(val);
  Wire.endTransmission();
}
uint8_t TAMC_GT911::readByteData(uint16_t reg, bool *ok) {
  if (ok) {
    *ok = false;
  }
  Wire.beginTransmission(addr);
  Wire.write(highByte(reg));
  Wire.write(lowByte(reg));
  if (Wire.endTransmission() != 0) {
    return 0; // 0 = "sin toques" si alguien ignora `ok`
  }
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) {
    return 0;
  }
  const int x = Wire.read();
  if (x < 0) {
    return 0;
  }
  if (ok) {
    *ok = true;
  }
  return (uint8_t)x;
}
void TAMC_GT911::writeBlockData(uint16_t reg, uint8_t *val, uint8_t size) {
  Wire.beginTransmission(addr);
  Wire.write(highByte(reg));
  Wire.write(lowByte(reg));
  // Wire.write(val, size);
  for (uint8_t i = 0; i < size; i++) {
    Wire.write(val[i]);
  }
  Wire.endTransmission();
}
bool TAMC_GT911::readBlockData(uint8_t *buf, uint16_t reg, uint8_t size) {
  Wire.beginTransmission(addr);
  Wire.write(highByte(reg));
  Wire.write(lowByte(reg));
  if (Wire.endTransmission() != 0) {
    return false;
  }
  if (Wire.requestFrom(addr, size) != size) {
    return false;
  }
  for (uint8_t i = 0; i < size; i++) {
    const int b = Wire.read();
    if (b < 0) {
      return false;
    }
    buf[i] = (uint8_t)b;
  }
  return true;
}
TP_Point::TP_Point(void) { id = x = y = size = 0; }
TP_Point::TP_Point(uint8_t _id, uint16_t _x, uint16_t _y, uint16_t _size) {
  id = _id;
  x = _x;
  y = _y;
  size = _size;
}
bool TP_Point::operator==(TP_Point point) {
  return ((point.x == x) && (point.y == y) && (point.size == size));
}
bool TP_Point::operator!=(TP_Point point) {
  return ((point.x != x) || (point.y != y) || (point.size != size));
}
