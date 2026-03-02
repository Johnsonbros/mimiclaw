#pragma once

// Board profiles:
// 0 = Existing ST7789T SPI board profile used by MimiClaw today.
// 1 = Waveshare ESP32-S3-Touch-LCD-1.46B profile.
#define MIMI_BOARD_XIAOZHI_ST7789   0
#define MIMI_BOARD_WAVESHARE_146B   1

#ifndef MIMI_BOARD_PROFILE
#define MIMI_BOARD_PROFILE MIMI_BOARD_XIAOZHI_ST7789
#endif

#if MIMI_BOARD_PROFILE == MIMI_BOARD_XIAOZHI_ST7789

#define BOARD_NAME                  "xiaozhi-st7789"

#define BOARD_LCD_SPI_SCLK          40
#define BOARD_LCD_SPI_MOSI          45
#define BOARD_LCD_SPI_MISO          -1
#define BOARD_LCD_SPI_DC            41
#define BOARD_LCD_SPI_RST           39
#define BOARD_LCD_SPI_CS            42
#define BOARD_LCD_BACKLIGHT         46

#define BOARD_I2C_SCL               47
#define BOARD_I2C_SDA               48

#define BOARD_BUTTON_BOOT           0

#elif MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B

#define BOARD_NAME                  "waveshare-esp32-s3-touch-lcd-1.46b"

// Waveshare 1.46B uses a QSPI LCD path, not the SPI path currently implemented in display.c.
#define BOARD_LCD_QSPI_SCK          40
#define BOARD_LCD_QSPI_D0           46
#define BOARD_LCD_QSPI_D1           45
#define BOARD_LCD_QSPI_D2           42
#define BOARD_LCD_QSPI_D3           41
#define BOARD_LCD_QSPI_CS           21
#define BOARD_LCD_TE                18
#define BOARD_LCD_BACKLIGHT         5

// Touch + IMU + RTC share this I2C bus.
#define BOARD_I2C_SCL               10
#define BOARD_I2C_SDA               11
#define BOARD_TOUCH_INT             4

// RST lines are exposed through onboard TCA9554 (EXIO), not direct ESP32 GPIO.
#define BOARD_EXIO_I2C_ADDR         0x20
#define BOARD_LCD_RST_EXIO          2
#define BOARD_TOUCH_RST_EXIO        1

#define BOARD_BUTTON_BOOT           0

#else
#error "Unknown MIMI_BOARD_PROFILE"
#endif
