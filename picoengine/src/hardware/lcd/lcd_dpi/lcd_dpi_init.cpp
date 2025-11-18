/*
 * Pico Engine - a library for the Pico Held 2 handheld
 *
 * Copyright (C) 2023 - 2025 Daniel Kammer (daniel.kammer@web.de)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
 
#include "lcd_dpi_init.h"

#if defined LCD_IF_DPI

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"

// Mode 0 software SPI
// SPI is only used for LCD setup, so no hi speed necessary
// SW SPI for portability reasons
void lcd_spi_send_byte(uint8_t val, int dc) {
  // write command
  gpio_put(PIN_LCD_MOSI, dc == SPI_COMMAND ? 0 : 1);  // 0: cmd; 1: data

  gpio_put(PIN_LCD_SCK, 1); // rising
  gpio_put(PIN_LCD_SCK, 0); // falling

  for (int i = 0; i < 8; i++) {
    if (val & 0x80)
      gpio_put(PIN_LCD_MOSI, 1);
    else
      gpio_put(PIN_LCD_MOSI, 0);
      
    val <<= 1;
    gpio_put(PIN_LCD_SCK, 1); // rising
    gpio_put(PIN_LCD_SCK, 0); // falling
  }
}

void lcd_spi_send(const uint8_t* init_seq) { 
  gpio_put(PIN_LCD_SCK, 0);

  gpio_put(PIN_LCD_CS, 0);  // assert CS

  //lcd_spi_send_byte(0x01); // SW reset
  //sleep_ms(120);

  int pos = 0;
  
  while (init_seq[pos] != END_LIST) {
    int n = 0;
    switch(init_seq[pos]) {
      case WRITE:
        pos++;
        n = init_seq[pos];
      break;
      case DELAY:
        n = 0;
        pos++;
        sleep_ms(init_seq[pos]);
      break;
      default:
        // unkown instruction
        //printf("Error in LCD init sequence: illegal instruction");
        assert(0);
      break;
    }

    
    for (int i = 0; i < n; i++) {
      pos++;
      lcd_spi_send_byte(init_seq[pos], i == 0 ? SPI_COMMAND : SPI_DATA);
    }

    pos++;

  }

  gpio_put(PIN_LCD_CS, 1);  // deassert CS

}

#endif // LCD_IF_DPI