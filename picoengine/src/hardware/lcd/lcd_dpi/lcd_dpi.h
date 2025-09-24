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

#if defined LCD_IF_DPI

#pragma once

#include "lcd.h"

#include <typedefs.h>
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"

#define LCD_SCALE_DEFAULT (3)
#define LCD_NUM_FB_DEFAULT (2)
#define NUM_FB_MAX (3)  // triple buffering (at most)
#define LCD_DMA_IRQ (0)  // 0 or 1

class LCD_DPI : public LCD
{
  private:   
    /******************************* PIO ****************************/
    bool m_pio_enabled = false;

    PIO m_pio = pio0;
    int8_t m_pio_sm_scanout;
    int8_t m_pio_sm_rot;
    int8_t m_pio_sm_clut;

    int m_program_scanout_entry;

    /******************************* DMA ****************************/
    bool m_dma_enabled = false;

    semaphore_t m_pageflip_pending;

    #if LCD_COLORDEPTH==16
    #define NUM_LCD_DMA (4)
    #elif LCD_COLORDEPTH==8
    #define NUM_LCD_DMA (6)
    #endif

    int32_t m_dma_chan[NUM_LCD_DMA];
    dma_channel_config m_dma_config[NUM_LCD_DMA];

    int m_dma_ring_buffer_size;

    semaphore_t dma_tx_done;

    /******************************* LCD ****************************/
    bool m_lcd_enabled = false;

    int m_lcd_scale;

    int m_rotation = 1;
    int m_screen_width;
    int m_screen_height;

    int m_refresh_mode;

    /******************************* FB ****************************/
    int m_num_fb;
    volatile int m_fb_cur = 0;
    //mutex_t mut_update_fb;

    // frame buffers
    color_t m_fb[2][256 /* SNES_WIDTH */ * (224 + 2) /* SNES_HEIGHT*/] __attribute__((aligned(4)));
    //color_t m_black_line[256 /* SNES_WIDTH */] __attribute__((aligned(4)));

    // rotation LUT
    uint32_t* m_fb_rlut; //[1024] __attribute__((aligned(4096));
    uint32_t* m_fb_rlut_backup[NUM_FB_MAX]; //[1024];
      
    // color LUT (aka palette)
    #if LCD_COLORDEPTH==8
    //color_palette_t m_palette[256] __attribute__((aligned(512), section(".scratch_x.parity"));
    color16_t m_palette[256] __attribute__((aligned(512)));
    #endif

    // hw functions
    void display_sleep();
    void display_wake();
    void display_init();
    void display_start();

    // interface setup
    lcd_error_t dma_init();
    lcd_error_t pio_init();

    // LUT
#if LCD_COLORDEPTH==8
    void build_default_palette();
#endif
    void build_fb_rot_lut();

    // buffer handling
    void retrace();
    void refresh_exec();
    void cycle_bufs();
    

    void calc_div(uint32_t freq, uint16_t *div_int, uint16_t *div_frac);

    friend void __isr dma_handler();

  public:
    lcd_error_t deinit();
    lcd_error_t init(uint16_t m_num_fb = LCD_NUM_FB_DEFAULT, int auto_refresh = LCD_VRR_REFRESH, int scale = LCD_SCALE_DEFAULT);

    /*
    LCD_DPI(uint16_t m_num_fb = LCD_NUM_FB_DEFAULT, int auto_refresh = LCD_VRR_REFRESH, int scale = LCD_SCALE_DEFAULT) {
      init(m_num_fb, auto_refresh, scale);
    };
    */

    LCD_DPI() {
    };

    ~LCD_DPI() {
      deinit();
    };

    // buffer/refresh functions
    int get_screen_width() override;
    int get_screen_height() override;
    int get_screen_bpp() override;

    lcd_error_t request_pageflip() override;
    lcd_error_t refresh() override;
#if LCD_COLORDEPTH==8
    lcd_error_t get_palette_ptr(color_palette_t* palette_ptr) override;
#endif
    lcd_error_t get_backbuffer(gbuffer_t* lcd_buffer) override;
    lcd_error_t get_fb(gbuffer_t* lcd_buffer) override;
    bool is_ready() override;

};

// setup



#endif // LCD_IF_DPI