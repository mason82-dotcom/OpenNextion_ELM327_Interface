#pragma once
#include <Arduino.h>
// ONX3248G035 official 09_outofbox_demo wiring, as verified in sibling project.
static constexpr uint8_t NEXTION_IIC_SCL_PIN = 7;
static constexpr uint8_t NEXTION_IIC_SDA_PIN = 8;
static constexpr uint16_t LCD_H_RES = 320;
static constexpr uint16_t LCD_V_RES = 480;
static constexpr int8_t LCD_SCLK_PIN = 5;
static constexpr int8_t LCD_MOSI_PIN = 1;
static constexpr int8_t LCD_MISO_PIN = -1;
static constexpr uint8_t LCD_DC_PIN = 3;
static constexpr uint8_t LCD_CS_PIN = 2;
static constexpr uint8_t LCD_BL_PIN = 6;
static constexpr uint8_t CST826_ADDR = 0x15;
static constexpr uint8_t PCF8574_ADDR = 0x38;
// PCF8574 bits are 0-indexed (vendor schematic U4). The vendor Arduino demo
// used 6, an unconverted 1-indexed value that actually drives SD CS#.
static constexpr uint8_t PCF8574_PIN_LCD_RST = 5;
// MicroSD CS#/DATA3; must stay high so the card answers in SD (not SPI) mode.
static constexpr uint8_t PCF8574_PIN_SD_CS = 6;
// MicroSD slot, SDMMC 1-bit: only DATA0 is wired, no card-detect line.
static constexpr int8_t SD_CLK_PIN = 11;
static constexpr int8_t SD_CMD_PIN = 10;
static constexpr int8_t SD_D0_PIN = 9;
static_assert(PCF8574_PIN_LCD_RST != PCF8574_PIN_SD_CS, "LCD reset must not pulse SD CS#");
