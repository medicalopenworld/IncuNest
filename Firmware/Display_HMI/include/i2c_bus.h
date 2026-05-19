#pragma once
#ifdef USE_IDF_FRAMEWORK
#include <stdint.h>
#include <stddef.h>

void    i2c_bus_init(void);
void    i2c_send(uint8_t dev_addr, const uint8_t *data, size_t len);
#endif
