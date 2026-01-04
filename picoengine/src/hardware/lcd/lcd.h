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
 
#include <hwcfg.h>

#pragma once

#include <typedefs.h>

#define LCD_SCALE_DEFAULT (3)
#define LCD_NUM_FB_DEFAULT (2)
#define NUM_FB_MAX (3)  // triple buffering (at most)

enum {
  LCD_UNKNOWN = 0,
  LCD_VRR_REFRESH = 1,  // variable refresh rate
  LCD_FRR_REFRESH = 2,  // fixed refresh rate
};

typedef enum {
  LCD_SUCCESS = 0,  		/**< @brief Command completed successfully */
  LCD_UNKNOWN_ERROR = -1,   /**< @brief An unknown error occured. */
  LCD_NOT_INIT = -3,   		/**< @brief The LCD interface has not yet
                             * been setup.*/
  LCD_IS_INIT = -4,   		/**< @brief The LCD interface has already
                             * been setup.*/
  LCD_DMA_ERR = -5,			/**< @brief An error has occcured setup up or
                             * using the DMA.*/
  LCD_PIO_ERR = -6,          /**< @brief An error has occcured setup up or
                             * using the PIO.*/
  LCD_MEM_ERR = -7,          /**< @brief An error has occcured (de-)allocating
                             * memory.*/
  LCD_NFB_ERR = -8,          /**< @brief Framebuffer error.*/
  LCD_RMOD_ERR = -9,          /**< @brief An action was requested that is
                             * not supported by the current refresh mode.*/
  LCD_BAD_PARAM = -10,          /**< @brief A bad parameter has beend
                             * passed.*/
  LCD_UNS_ERR = -11,          /**< @brief The command is unspported by the
                             * current setup/driver.*/
} lcd_error_t ; 


class LCD
{
  public:
    LCD() {};
    virtual ~LCD() {};

    // buffer/refresh functions
    virtual int get_screen_width() { return -1; }
    virtual int get_screen_height() { return -1; }
    virtual int get_screen_bpp() { return -1; }
    virtual lcd_error_t request_pageflip() { return LCD_NOT_INIT; }
    virtual lcd_error_t refresh() { return LCD_NOT_INIT; }
    virtual lcd_error_t get_palette_ptr(color_palette_t* palette_ptr) { return LCD_NOT_INIT; }
    virtual lcd_error_t get_backbuffer(gbuffer_t* lcd_buffer) { return LCD_NOT_INIT; }
    virtual lcd_error_t get_psram_backbuffer(gbuffer_t* lcd_buffer) { return LCD_UNS_ERR; };
    virtual bool lcd_is_vsync() { return false; }
    virtual lcd_error_t get_fb(gbuffer_t* lcd_buffer) { return LCD_NOT_INIT; }
    virtual bool is_ready() { return false; }

};

