/* Interno de usb_comm; expuesto en include/ solo para los tests Unity. */
#pragma once
#include <stddef.h>

/* Copia src en dst escapando '"' y '\' y sustituyendo caracteres de control
 * (<0x20) por espacio. Lee como máximo src_max bytes de src y nunca escribe
 * más de dst_size-1 caracteres + NUL. Devuelve la longitud escrita (sin NUL).
 * Para cualquier string de origen externo que se interpole en JSON de salida. */
size_t sb_json_escape(char *dst, size_t dst_size, const char *src, size_t src_max);
