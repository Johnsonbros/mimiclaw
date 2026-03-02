#include "io_expander/tca9554.h"
#include "imu/I2C_Driver.h"
#include "esp_log.h"

#define TCA9554_REG_OUTPUT      0x01
#define TCA9554_REG_DIRECTION   0x03

static const char *TAG = "tca9554";

esp_err_t tca9554_init(uint8_t i2c_addr)
{
    /* Set all pins as outputs (direction register: 0 = output) */
    uint8_t dir = 0x00;
    esp_err_t ret = I2C_Write(i2c_addr, TCA9554_REG_DIRECTION, &dir, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set direction register: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Default all outputs high */
    uint8_t out = 0xFF;
    ret = I2C_Write(i2c_addr, TCA9554_REG_OUTPUT, &out, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set output register: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t tca9554_set_pin(uint8_t i2c_addr, uint8_t pin, uint8_t level)
{
    if (pin > 7) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t current = 0;
    esp_err_t ret = I2C_Read(i2c_addr, TCA9554_REG_OUTPUT, &current, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read output register: %s", esp_err_to_name(ret));
        return ret;
    }

    if (level) {
        current |= (1 << pin);
    } else {
        current &= ~(1 << pin);
    }

    ret = I2C_Write(i2c_addr, TCA9554_REG_OUTPUT, &current, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write output register: %s", esp_err_to_name(ret));
    }
    return ret;
}
