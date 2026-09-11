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
