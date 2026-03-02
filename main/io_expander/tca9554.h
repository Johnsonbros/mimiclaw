#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t tca9554_init(uint8_t i2c_addr);
esp_err_t tca9554_set_pin(uint8_t i2c_addr, uint8_t pin, uint8_t level);

#ifdef __cplusplus
}
#endif
