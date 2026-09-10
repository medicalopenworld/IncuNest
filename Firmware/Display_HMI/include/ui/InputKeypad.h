#pragma once
// Teclados en pantalla para los datos del bebe (nombre en letras, numeros en
// digitos), compartidos por el asistente de activacion (BabyWizard) y el
// registro desde la pantalla Bebes (BabyHistory). Un solo sitio para los mapas
// de teclas y la validacion: lo que teclea el operador acaba en una linea CSV
// del protocolo (PROTOCOL.md), asi que las reglas de que caracteres pueden
// entrar viven aqui y no en cada pantalla.
#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

// Crea un teclado que escribe en `ta` y lo devuelve ya dimensionado (letras
// 750x250, digitos 420x250) con la fuente de las teclas puesta. El llamador lo
// alinea donde le convenga. `digits`: 0-9 + borrar; si no, A-Z + espacio +
// borrar, sin ninguna tecla de coma (ver InputKeypad.cpp).
lv_obj_t *InputKeypad_Create(lv_obj_t *parent, lv_obj_t *ta, bool digits);

// Callback para LV_EVENT_VALUE_CHANGED del textarea de nombre: quita cualquier
// coma. Defensa en profundidad: el mapa de letras no tiene coma, pero una coma
// no debe llegar nunca al protocolo delimitado por comas.
void InputKeypad_StripCommasCb(lv_event_t *e);

// Lee el textarea numerico `ta`: falso si esta vacio, no es un entero o queda
// fuera de [lo, hi]. En exito deja el valor en *out.
bool InputKeypad_ReadNumber(lv_obj_t *ta, uint32_t lo, uint32_t hi,
                            uint32_t *out);
