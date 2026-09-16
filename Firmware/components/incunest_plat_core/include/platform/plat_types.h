#pragma once

// Los alias de tipo escalar que definia Arduino y que el firmware usa sin
// pensarlos. No son una dependencia: son typedefs de una linea sobre tipos
// estandar.
//
// Se conservan los nombres por lo de siempre: `byte` aparece en decenas de
// firmas de driver y de buffers de protocolo, y cambiarlo a uint8_t seria una
// edicion enorme sin ningun efecto sobre el binario.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
using byte = uint8_t;
using word = uint16_t;
using boolean = bool;
#else
typedef uint8_t byte;
typedef uint16_t word;
typedef bool boolean;
#endif

// Constantes y tipos sueltos que Arduino definia y que usan algunas librerias
// de terceros vendorizadas (Adafruit BusIO, impresion con base numerica).
// Son valores, no comportamiento.
// F() metia la cadena en flash en AVR. En Arduino-ESP32 ya era la identidad
// (todo va a flash de serie) y en ESP-IDF la macro no existe. Se define como
// identidad, que es exactamente lo que hacia antes en esta plataforma.
// lowByte/highByte: macros de Arduino para partir un entero de 16 bits.
#ifndef lowByte
#define lowByte(w) ((uint8_t)((w) & 0xFF))
#define highByte(w) ((uint8_t)(((w) >> 8) & 0xFF))
#endif

#ifndef F
#define F(x) (x)
#endif

#ifndef HEX
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
#endif

#ifdef __cplusplus
// Adafruit BusIO usa el TIPO BitOrder de Arduino en firmas de funcion, pero
// define sus propios enumeradores (BusIOBitOrder, con SPI_BITORDER_MSBFIRST).
// Por eso aqui solo se aporta el alias del tipo: declarar tambien los
// enumeradores chocaba con los suyos.
#ifndef PLAT_BITORDER_DEFINED
#define PLAT_BITORDER_DEFINED
using BitOrder = uint8_t;
#endif
#endif
