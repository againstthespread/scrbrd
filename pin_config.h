/**
 * pin_config.h - Hardware Pin Definitions
 * 
 * Board: ESP32-S3 + ST7789 TFT (240x280)
 */

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// Color definitions (RGB565 format)
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F

// Display size
#define LCD_WIDTH   240
#define LCD_HEIGHT  280

// SPI pins for ST7789
#define LCD_MOSI    39    // Data out
#define LCD_SCK     38    // Clock
#define LCD_CS      14    // Chip select
#define LCD_DC      10    // Data/Command
#define LCD_RST     3     // Reset
#define LCD_BL      13    // Backlight

#endif
