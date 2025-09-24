/*
 * pplib - a library for the Pico Held handheld
 *
 * Copyright (C) 2023 Daniel Kammer (daniel.kammer@web.de)
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
 
#ifndef HWCFG_H
#define HWCFG_H

// 0 = dev board      (protoss) - no longer exists
// 1 = old prototype  (marble white) - no longer exists
// 2 = new prototype  (Leo) - no longer exists
// 3 = RC1            (J) - no longer exists
// 4 = RC2            (Dev) - no longer exists
// 5 = RC3            (published version)
// 6 = IPS            (IPS display)
// 7 = SPI testing
// 8 = RP2350
// 9 = DPI Pimoroni PGA2350 w/ Startek display
// 10 = P2 w/ Buydisplay display

#ifndef PPL_CONFIG_NO
#define PPL_CONFIG_NO 10
#endif

#if PPL_CONFIG_NO==10
// ================================== CONFIG 10 (P2) ============================================

/*----------------- buttons --------------- */
#define BUTTON_PULL_MODE (1) // 0 = pulldown, 1 = pullup
#define BUTTON_ACTIVE (false) // false = button pressed when low, true = button pressed when hi

#define ADC_PIN_OFFSET (40) // RP2040 and RP235xA (but different on RP235xB)

#define PIN_ANALOG_X  (45)
#define PIN_ANALOG_Y  (47)

#define PIN_BTN_A  (26)  // A
#define PIN_BTN_B  (25)  // B
#define PIN_BTN_X  (22)  // X
#define PIN_BTN_Y  (24)  // Y
#define PIN_BTN_L  (42)  // L
#define PIN_BTN_R  (27)  // R
#define PIN_BTN_ST  (29)  // St
#define PIN_BTN_SEL  (23)  // Sel
#define PIN_BTN_SW  (46)  // Ana SW

#define PIN_DPAD_LR  (43)  // LR
#define PIN_DPAD_UD  (44)  // UD

/* ------------------------ Power management ------------------------*/
#define PIN_BAT_ADC   (40)
//#define PIN_5V_ADC    (41)
#define PIN_PWR_EN    (28)
#define PIN_CHG_STAT  (30)

/* ------------------------ Sound ------------------------*/
#define PIN_SND_EN    (36)
#define PIN_I2S_LR    (37)  // LR and BCK need to be adjacent pins with LR being the lower GPIO #
#define PIN_I2S_BCK   (38)
#define PIN_I2S_DAT   (39)

/* supported frequencies (kHz): 44.1, 32, 16, 8 */

/* ---------------------- LCD ----------------------*/
#define LCD_IF_DPI

#define LCD_FREQ          (37000000) // (37000000)

// LCD_PIO_SPEED defines the PIO speed (in MHz)
#define LCD_PIO_SPEED    (120)

// Buydisplay
#define LCD_VBP		 (20)
#define LCD_VFP	 	 (15)
#define LCD_VPW		  (5)
#define LCD_HBP		 (80)
#define LCD_HFP		 (15)  // min: 4
#define LCD_HPW	      (5)  // min: 1

#define LCD_PHYS_RES_X      (720)  // max: 1024
#define LCD_PHYS_RES_Y      (720)  // max: 1024

// display pins
#define PIN_LCD_BL_PWM   (1)

// Note: the data pins are PIO hardware configured and driven and need to be in consecutive order
#define PIN_LCD_DB0      (6)    // 1st of the 16 data pins, that's RRRRRGGGGGGBBBBB (from left to right: from LSB to MSB)

#define PIN_LCD_DE       (5)    // data enable
#define PIN_LCD_PCLK     (4)    // pixel clock
#define PIN_LCD_RST      (2)    // reset pin

// LCD SPI
#define PIN_LCD_SCK     (34)
#define PIN_LCD_MOSI    (35)
#define PIN_LCD_CS      (3)

/* ---------------------- SD card ----------------------*/
#define PIN_SD_SCK     (34)
#define PIN_SD_MOSI    (35)
#define PIN_SD_MISO    (32)
#define PIN_SD_CS      (33)
#define PIN_SD_DET     (31)

/* ---------------------- PSRAM ----------------------*/
#define PIN_PSRAM_CS    (0)

#endif  // CONFIG SELECTION

#endif //HWCFG_H
