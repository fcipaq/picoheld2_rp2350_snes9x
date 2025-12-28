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
 


 /*
  * VRR is variable refresh rate and the dev is responsible for delivering
  * a frame in time. Advantage: custom refreshrate possible and if you're
  * just a few ms short, then the display will tolerate this and one does
  * not have to wait for the next frame.
  * Disadvantage: if the frame rate drops too low, there will be visible
  * glitches
  * With FRR the framerate is fixed. Advantage: no visible artifacts will
  * occur, no matter how low the framerate is.
  * Disadvante: if only using double buffering, then if you miss a frame
  * by only a few us, then you'll have to wait an entire frame. So
  * triple buffering is almost mandatory, eating up more ram...
 */
 
#include <hwcfg.h>

#ifdef LCD_IF_DPI

#include "lcd_dpi.h"
#include <stdio.h>  // overclock
#include <stdlib.h>
#include "pico/stdlib.h"  // overclock
#include "hardware/vreg.h"
#include "hardware/resets.h"
#include "hardware/clocks.h"
#include "pico/platform.h"

#include "lcd_dpi_init.h"
#include "../backlight/backlight.h"

#include "lcd_dpi_pio_scanout_3x.pio.h"
#include "lcd_dpi_pio_scanout_2x.pio.h"

#if LCD_COLORDEPTH==8
#include "lcd_dpi_pio_addr_dec_8bit.pio.h"
#include "lcd_dpi_pio_clut.pio.h"
#elif LCD_COLORDEPTH==16
#include "lcd_dpi_pio_addr_dec.pio.h"
#else
#error "Only 8bit and 16 bit color depth supported"
#endif

#define LCD_HBLANK      (LCD_HBP + LCD_HFP + LCD_HPW)
#define LCD_VBLANK      (LCD_VBP + LCD_VFP + LCD_VPW)

LCD_DPI* lcd_this = NULL;

/******************************* Implementation ****************************/
void LCD_DPI::display_sleep() {
  lcd_spi_send(nv3052c_generic_sleep_operations);
}

void LCD_DPI::display_wake() {
  lcd_spi_send(nv3052c_generic_wake_operations);
}

void LCD_DPI::display_init() {
  //lcd_spi_send(nv3052c_init_operations);
  //lcd_spi_send(st7701_type1_init_operations);
  //lcd_spi_send(nv3052c_buydisplay_init_operations);

  lcd_spi_send(sunmixic_init_operations);
  
}

int LCD_DPI::get_screen_width() {  // todo: make inline
  return m_screen_width;
}

int LCD_DPI::get_screen_height() {  // todo: make inline
  return m_screen_height;
}

int LCD_DPI::get_screen_bpp() {  // todo: make inline
  return (LCD_COLORDEPTH);
}

#if LCD_COLORDEPTH==8
lcd_error_t LCD_DPI::get_palette_ptr(color_palette_t* palette_ptr) {
  if (!m_lcd_enabled)
    return LCD_NOT_INIT;

  *palette_ptr = &m_palette[0];
  
  return LCD_SUCCESS;
}

void LCD_DPI::build_default_palette() {
  // calculate standard color palette of 256 colors
  int rr, gg, bb;
  int i = 0;

  for (int r = 0; r < 8; r++)
    for (int g = 0; g < 8; g++)
      for (int b = 0; b < 4; b++) {
        rr = r * 5;
        if (rr > 31) rr = 31;

        gg = g * 9;

        bb = b * 11;
        if (bb > 31) bb = 31;

        //_palette[i] = RGBColor565_565(rr, gg, bb);
        m_palette[i] = rr << 11 | gg << 5 | bb;

        i++;
      }
}
#endif

void LCD_DPI::retrace() {
  // reset DMAs
  for (int i = 0; i < NUM_LCD_DMA; i++)
    m_dma_config[i].ctrl &= ~DMA_CH0_CTRL_TRIG_EN_BITS; // channel_config_set_enable(&m_dma_config[i], false);

  for (int i = 0; i < NUM_LCD_DMA; i++) {
    //dma_channel_abort(m_dma_chan[i]);
    dma_hw->abort = 1u << m_dma_chan[i];
    while (dma_hw->ch[m_dma_chan[i]].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS) 
      tight_loop_contents();
  }

  for (int i = 0; i < NUM_LCD_DMA; i++)
    m_dma_config[i].ctrl |= DMA_CH0_CTRL_TRIG_EN_BITS; // channel_config_set_enable(&m_dma_config[i], true);  

  // reset PIO
  pio_sm_clear_fifos(m_pio, m_pio_sm_rot);
  pio_sm_clear_fifos(m_pio, m_pio_sm_scanout);
  #if LCD_COLORDEPTH==8
  pio_sm_clear_fifos(m_pio, m_pio_sm_clut);
  #endif

  pio_sm_exec(m_pio, m_pio_sm_scanout, pio_encode_jmp(m_program_scanout_entry));

}

void LCD_DPI::refresh_exec() {
  // reset RLUT
  for (int i = 0; i < (1u << m_dma_ring_buffer_size); i++)
    m_fb_rlut[i] = m_fb_rlut_backup[m_fb_cur][i];

  dma_channel_set_read_addr(m_dma_chan[2], m_fb_rlut, true);
  dma_channel_set_write_addr(m_dma_chan[3], m_fb_rlut, true);
  dma_channel_set_read_addr(m_dma_chan[0], m_fb_rlut, true);
}

void LCD_DPI::cycle_bufs() {
    if (sem_available(&m_pageflip_pending) < (m_num_fb - 1)) {
     //mutex_try_enter(&mut_update_fb, NULL);

      m_fb_cur++;
   
      if (m_fb_cur == m_num_fb)
        m_fb_cur = 0;
      sem_release(&m_pageflip_pending);
   
     //mutex_exit(&mut_update_fb);
    }
}

void __isr dma_handler() {

  assert(lcd_this);

  if (!dma_irqn_get_channel_status(LCD_DMA_IRQ, lcd_this->m_dma_chan[0]))
    return;

  dma_irqn_acknowledge_channel(LCD_DMA_IRQ, lcd_this->m_dma_chan[0]);

  if (lcd_this->m_refresh_mode == LCD_FRR_REFRESH)
    lcd_this->cycle_bufs();

  if (lcd_this->m_refresh_mode == LCD_VRR_REFRESH)
    sem_reset(&(lcd_this->dma_tx_done), 1);

  lcd_this->retrace();
  
  if (lcd_this->m_refresh_mode == LCD_FRR_REFRESH)
    lcd_this->refresh_exec();

}

lcd_error_t LCD_DPI::dma_init() {
  if (!m_pio_enabled)
    return LCD_PIO_ERR;

  if (m_dma_enabled)
    return LCD_SUCCESS;

  for (int i = 0; i < NUM_LCD_DMA; i++) {
    m_dma_chan[i] = dma_claim_unused_channel(true);
    
    if (m_dma_chan[i] < 0)
      return LCD_DMA_ERR;

    m_dma_config[i] = dma_channel_get_default_config(m_dma_chan[i]);
    
    channel_config_set_high_priority(&m_dma_config[i], true);
    channel_config_set_irq_quiet(&m_dma_config[i], true);
  }

#if LCD_COLORDEPTH==16
  /************************* DMA channel 1: color data --> SO-PIO  ************************/ 
  channel_config_set_transfer_data_size(&m_dma_config[1], DMA_SIZE_16);
  channel_config_set_high_priority(&m_dma_config[1], true);
  channel_config_set_read_increment(&m_dma_config[1], false);
  channel_config_set_chain_to(&m_dma_config[1], m_dma_chan[3]);
  dma_channel_configure(m_dma_chan[1],
                        &m_dma_config[1],
                        &m_pio->txf[m_pio_sm_scanout],  // write
                        NULL,  // read
                        1,
                        false); // gets triggered by DMA0

  /************************* DMA channel 2: RLUT --> ROT-Pio  ************************/ 
  channel_config_set_dreq(&m_dma_config[2], pio_get_dreq(m_pio, m_pio_sm_rot, true /* tx */));
  channel_config_set_ring(&m_dma_config[2], false /* read ring */, m_dma_ring_buffer_size + 2 /* 32bit words */);
  dma_channel_configure(m_dma_chan[2],
                        &m_dma_config[2],
                        &m_pio->txf[m_pio_sm_rot],
                        m_fb_rlut,
                        (1u << m_dma_ring_buffer_size) * (LCD_PHYS_RES_Y / m_lcd_scale),
                        true);  // gets chained by DMA1

  /**************************** DMA channel 3: ROT-Pio --> address list  ************************/ 
  channel_config_set_dreq(&m_dma_config[3], pio_get_dreq(m_pio, m_pio_sm_rot, false /* rx */));
  channel_config_set_read_increment(&m_dma_config[3], false);
  channel_config_set_write_increment(&m_dma_config[3], true);
  channel_config_set_ring(&m_dma_config[3], true /* write ring */, m_dma_ring_buffer_size + 2 /* 32bit words */);
  dma_channel_configure(m_dma_chan[3],
                        &m_dma_config[3],
                        m_fb_rlut,
                        &m_pio->rxf[m_pio_sm_rot],
                        1,
                        true);   // gets chained by DMA2

  /*************************** DMA channel 0: Pixel address --> DMA1  ************************/ 
  channel_config_set_dreq(&m_dma_config[0], pio_get_dreq(m_pio, m_pio_sm_scanout, true /* tx */));
  channel_config_set_ring(&m_dma_config[0], false /* read ring */, m_dma_ring_buffer_size + 2 /* 32 bit words */);
  channel_config_set_irq_quiet(&m_dma_config[0], false);
  dma_channel_configure(m_dma_chan[0],
                        &m_dma_config[0],
                        &dma_hw->ch[m_dma_chan[1]].al3_read_addr_trig,
                        m_fb_rlut,
                        (1u << m_dma_ring_buffer_size) * (LCD_PHYS_RES_Y / m_lcd_scale),
                        true /* go! */);

#elif LCD_COLORDEPTH==8
  /******************* DMA channel 1: Pixel-Data --> CLUT-PIO  ************************/ 
  //channel_config_set_dreq(&m_dma_config[1], pio_get_dreq(m_pio, m_pio_sm_clut, true /* tx */));
  channel_config_set_transfer_data_size(&m_dma_config[1], DMA_SIZE_8);
  channel_config_set_chain_to(&m_dma_config[1], m_dma_chan[4]);
  dma_channel_configure(m_dma_chan[1],
                        &m_dma_config[1],
                        &m_pio->txf[m_pio_sm_clut],
                        NULL,
                        1,
                        true); // gets triggered by DMA0

  /**************************** DMA channel 4: color addr --> DMA5  ************************/ 
  channel_config_set_dreq(&m_dma_config[4], pio_get_dreq(m_pio, m_pio_sm_clut, false /* rx */));
  channel_config_set_read_increment(&m_dma_config[4], false);
  channel_config_set_chain_to(&m_dma_config[4], m_dma_chan[2]);
  dma_channel_configure(m_dma_chan[4],
                        &m_dma_config[4],
                        &dma_hw->ch[m_dma_chan[5]].al3_read_addr_trig,
                        &m_pio->rxf[m_pio_sm_clut],
                        1,
                        true);  // control channel

  /************************* DMA channel 5: color data --> SO-PIO  ************************/ 
  channel_config_set_transfer_data_size(&m_dma_config[5], DMA_SIZE_16);
  dma_channel_configure(m_dma_chan[5],
                        &m_dma_config[5],
                        &m_pio->txf[m_pio_sm_scanout],
                        NULL,
                        1,
                        false);  // gets triggered by DMA4

  /************************* DMA channel 2: RLUT --> ROT-Pio  ************************/ 
  channel_config_set_dreq(&m_dma_config[2], pio_get_dreq(m_pio, m_pio_sm_rot, true /* tx */)); // no need for dreq 
  channel_config_set_ring(&m_dma_config[2], false /* read ring */, m_dma_ring_buffer_size + 2 /* 32bit words */);
  channel_config_set_high_priority(&m_dma_config[2], true);
  channel_config_set_chain_to(&m_dma_config[2], m_dma_chan[3]);
  dma_channel_configure(m_dma_chan[2],
                        &m_dma_config[2],
                        &m_pio->txf[m_pio_sm_rot],
                        m_fb_rlut,
                        1,
                        true);  // gets chained by DMA0

  /**************************** DMA channel 3: ROT-Pio --> address list  ************************/ 
  channel_config_set_dreq(&m_dma_config[3], pio_get_dreq(m_pio, m_pio_sm_rot, false /* rx */));
  channel_config_set_read_increment(&m_dma_config[3], false);
  channel_config_set_write_increment(&m_dma_config[3], true);
  channel_config_set_ring(&m_dma_config[3], true /* write ring */, m_dma_ring_buffer_size + 2 /* 32bit words */);
  channel_config_set_high_priority(&m_dma_config[3], true);
  dma_channel_configure(m_dma_chan[3],
                        &m_dma_config[3],
                        m_fb_rlut,  // write
                        &m_pio->rxf[m_pio_sm_rot],  // read
                        1,
                        true);   // gets chained by DMA2

  /*************************** DMA channel 0: Pixel address --> DMA1  ************************/ 
  channel_config_set_dreq(&m_dma_config[0], pio_get_dreq(m_pio, m_pio_sm_scanout, true /* tx */));
  channel_config_set_ring(&m_dma_config[0], false /* read ring */, m_dma_ring_buffer_size + 2 /* 32 bit words */);
  channel_config_set_irq_quiet(&m_dma_config[0], false);
  dma_channel_configure(m_dma_chan[0],
                        &m_dma_config[0],
                        &dma_hw->ch[m_dma_chan[1]].al3_read_addr_trig,
                        m_fb_rlut,
                        (1u << m_dma_ring_buffer_size) * (LCD_PHYS_RES_Y / m_lcd_scale),
                        true /* go! */);  // get chained by DMA3

#endif

  /*-------------------- setup IRQ ---------------------*/
  dma_irqn_set_channel_enabled(LCD_DMA_IRQ, m_dma_chan[0], true);  // USE THIS IF VALID FOR BETTER READING

  if (irq_get_exclusive_handler(DMA_IRQ_NUM(LCD_DMA_IRQ)) == NULL)
    irq_set_exclusive_handler(DMA_IRQ_NUM(LCD_DMA_IRQ), dma_handler);
    //irq_add_shared_handler(DMA_IRQ_NUM(LCD_DMA_IRQ), dma_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);  // 255 is highest prio
  irq_set_enabled(DMA_IRQ_NUM(LCD_DMA_IRQ), true);
  
  // Driving the LCD is highly sensitive to timing issues, and delayed interrupt servicing can lead to visible glitches.
  irq_set_priority(DMA_IRQ_NUM(LCD_DMA_IRQ), 0x40);

  m_dma_enabled = true;

  return LCD_SUCCESS;
}  // dma_init

void LCD_DPI::calc_div(uint32_t freq, uint16_t *div_int, uint16_t *div_frac) {
  uint32_t fcpu = clock_get_hz(clk_sys);

  freq *= 2;   // 1 LCD cycle == 2 PIO cycles
  *div_int = fcpu / freq;
  *div_frac = (uint16_t) (((float) fcpu / (float) freq - (float) *div_int) * 256.0);

  // max speed exceeded
  if (*div_int == 0) {
    *div_int = 1;
    *div_frac = 0;
  }

}  // calc_div

lcd_error_t LCD_DPI::pio_init() {
  if (m_pio_enabled)
    return LCD_SUCCESS;

  m_pio_sm_rot = pio_claim_unused_sm(m_pio, false);

  if (m_pio_sm_rot < 0)
    return LCD_PIO_ERR;

  m_pio_sm_scanout = pio_claim_unused_sm(m_pio, false);

  if (m_pio_sm_scanout < 0)
    return LCD_PIO_ERR;

#if LCD_COLORDEPTH==8
  m_pio_sm_clut = pio_claim_unused_sm(m_pio, false);

  if (m_pio_sm_clut < 0)
    return LCD_PIO_ERR;
#endif

  /******************************** rot sm *******************************/

  pio_sm_restart(m_pio, m_pio_sm_rot);

  if (!pio_can_add_program(m_pio, &lcd_addr_dec_program))
    return LCD_PIO_ERR;

  uint32_t program_offset = pio_add_program(m_pio, &lcd_addr_dec_program);
  pio_sm_config pio_cfg = lcd_addr_dec_program_get_default_config(program_offset);

  // configure the SM
  // This SM must be faster in processing incoming data than the scanout 
  // state machine. Otherwise it will lose sync!
  // One sample takes 4 SM cycles in 16 bit mode and 3 cycles in 8 bit mode
  sm_config_set_clkdiv_int_frac(&pio_cfg, 1, 0);  // max speed
  sm_config_set_out_shift(&pio_cfg, true, true, 1);    // OSR shifts out to the right (true), autopull, pull threshold
  sm_config_set_in_shift(&pio_cfg, true, true, 1);     // ISR shifts in to the right (true), autopush, push threshold
  pio_sm_init(m_pio, m_pio_sm_rot, program_offset, &pio_cfg);

  // clear FIFO & OSR
  pio_sm_clear_fifos(m_pio, m_pio_sm_rot);
  pio_sm_exec(m_pio, m_pio_sm_rot, pio_encode_out(pio_null, 32));

  /****************************** scanout sm ******************************/

  pio_sm_restart(m_pio, m_pio_sm_scanout);

  // allocate gpios
  pio_gpio_init(m_pio, PIN_LCD_DE);
  pio_gpio_init(m_pio, PIN_LCD_PCLK);
  for (int i = 0; i < 16; i++)
    pio_gpio_init(m_pio, PIN_LCD_DB0 + i);

  uint16_t div_int = 0;
  uint16_t div_frac = 0;

  // TODO: fix me!
  calc_div(LCD_FREQ, &div_int, &div_frac);
  if (LCD_COLORDEPTH == 16) {
    // 3x: @ 150 MHz - 50 fps
    div_int = 2;
    div_frac = 0;
  } else if (LCD_COLORDEPTH == 8 && m_lcd_scale == 3) {
    // 3x: @280 MHz --> 50 fps
    div_int = 3;  
    div_frac = 160;
  } else if (LCD_COLORDEPTH == 8 && m_lcd_scale == 2) {
    // 3x: @280 MHz --> 34 fps  // TODO: speed is too high --> artifacts at corners of the screen
    div_int = 3;
    div_frac = 0;
  }

  const pio_program_t* lcd_scanout_program;

  if (m_lcd_scale == 2) {
    lcd_scanout_program = &lcd_scanout_2x_program;
  } else if (m_lcd_scale == 3) {
    lcd_scanout_program = &lcd_scanout_3x_program;
  }

  if (!pio_can_add_program(m_pio, lcd_scanout_program))
    return LCD_PIO_ERR;

  program_offset = pio_add_program(m_pio, lcd_scanout_program);

  if (m_lcd_scale == 2) {
    pio_cfg = lcd_scanout_2x_program_get_default_config(program_offset);
    m_program_scanout_entry = program_offset + lcd_scanout_2x_offset_vsync;
  } else if (m_lcd_scale == 3) {
    pio_cfg = lcd_scanout_3x_program_get_default_config(program_offset);
    m_program_scanout_entry = program_offset + lcd_scanout_3x_offset_vsync;
  }


  // setup pins (PCLK, DE, DBs)
  sm_config_set_sideset_pin_base(&pio_cfg, PIN_LCD_PCLK);
  sm_config_set_out_pins(&pio_cfg, PIN_LCD_DB0, 16);
  sm_config_set_set_pins(&pio_cfg, PIN_LCD_DE, 1);

  // configure the SM
  sm_config_set_clkdiv_int_frac(&pio_cfg, div_int, div_frac);
  sm_config_set_fifo_join(&pio_cfg, PIO_FIFO_JOIN_TXGET);           // enable get from RX FIFO
  sm_config_set_out_shift(&pio_cfg, true, true, 16);                // OSR shifts out to the right (true), autopull, pull threshold (32 is same as 0, i.e. only autopull when empty)
  pio_sm_init(m_pio, m_pio_sm_scanout, program_offset, &pio_cfg);

  // setup pindirs
  pio_sm_set_consecutive_pindirs(m_pio, m_pio_sm_scanout, PIN_LCD_PCLK, 1, true);
  pio_sm_set_consecutive_pindirs(m_pio, m_pio_sm_scanout, PIN_LCD_DB0, 16, true);
  pio_sm_set_consecutive_pindirs(m_pio, m_pio_sm_scanout, PIN_LCD_DE, 1, true);

  // setup FIFOs & OSR
  pio_sm_clear_fifos(m_pio, m_pio_sm_scanout);
  pio_sm_exec(m_pio, m_pio_sm_scanout, pio_encode_set(pio_x, 2));
  pio_sm_exec(m_pio, m_pio_sm_scanout, pio_encode_mov(pio_isr, pio_x));
//  pio_sm_exec(m_pio, m_pio_sm_scanout, pio_encode_set(pio_x, 0));  // no initial VBLANK

  // send screen parameters to SM
  m_pio->rxf_putget[m_pio_sm_scanout][0] = LCD_PHYS_RES_X / m_lcd_scale - 1;  // x visible pixels  // -1 here is bcz. the PIO will cmp x!=0 BEFORE decrement
  // The 2nd "- 1" is bcz. in the pixel loop there will be one pull too much which then get's discarded so in the hblank loop there must be
  // one pull less to compensate for the lost word.
  m_pio->rxf_putget[m_pio_sm_scanout][1] = 2 * (((1u << m_dma_ring_buffer_size) - LCD_PHYS_RES_X) / m_lcd_scale) - 1 - 1; // hsync pixels
  m_pio->rxf_putget[m_pio_sm_scanout][2] = LCD_VBLANK * (LCD_HBLANK + LCD_PHYS_RES_X) - 1;  // vsync pixels

//  pio_sm_exec(m_pio, m_pio_sm_scanout, pio_encode_jmp(m_program_scanout_entry));

#if LCD_COLORDEPTH==8
  /******************************** clut sm *******************************/
  pio_sm_restart(m_pio, m_pio_sm_clut);

  if (!pio_can_add_program(m_pio, &lcd_calc_clut_addr_program))
    return LCD_PIO_ERR;

  program_offset = pio_add_program(m_pio, &lcd_calc_clut_addr_program);
  pio_cfg = lcd_calc_clut_addr_program_get_default_config(program_offset);

  // configure the SM
  // This SM must be faster in processing incoming data than the scanout 
  // state machine. Otherwise it will lose sync!
  // One sample needs 4 SM cycles to process
  sm_config_set_clkdiv_int_frac(&pio_cfg, 1, 0);  // max speed
  sm_config_set_out_shift(&pio_cfg, true, false, 1);  // OSR shifts out to the right (true), no autopull, empty threshold
  sm_config_set_in_shift(&pio_cfg, false, true, 9);   // ISR shifts in to the right (true), autopush, push threshold
  pio_sm_init(m_pio, m_pio_sm_clut, program_offset, &pio_cfg);

  // clear FIFO & OSR
  pio_sm_clear_fifos(m_pio, m_pio_sm_clut);
  pio_sm_exec(m_pio, m_pio_sm_clut, pio_encode_out(pio_null, 32));

  // Load base address to state machine register x
  uint32_t addrbase = (uint32_t) &m_palette[0];

  assert((addrbase & 0x1FF) == 0);

  pio_sm_put(m_pio, m_pio_sm_clut, addrbase >> 9);
  pio_sm_exec(m_pio, m_pio_sm_clut, pio_encode_pull(false, false));
  pio_sm_exec(m_pio, m_pio_sm_clut, pio_encode_out(pio_x, 32));

  pio_sm_set_enabled(m_pio, m_pio_sm_clut, true);
#endif

  // start all SMs
  pio_sm_set_enabled(m_pio, m_pio_sm_scanout, true /* enable */);
  pio_sm_set_enabled(m_pio, m_pio_sm_rot, true /* enable */);

  m_pio_enabled = true;

  return LCD_SUCCESS;
}  // m_pio_init

void LCD_DPI::display_start() {
 
  gpio_init(PIN_LCD_RST);
  gpio_set_dir(PIN_LCD_RST, GPIO_OUT);
  gpio_put(PIN_LCD_RST, 0);

  gpio_init(PIN_LCD_CS);
  gpio_set_dir(PIN_LCD_CS, GPIO_OUT);

  gpio_put(PIN_LCD_CS, 1);  // deassert CS

  sleep_ms(120);
  gpio_put(PIN_LCD_RST, 1);
  sleep_ms(120);

  display_init();

} // start

lcd_error_t LCD_DPI::deinit() {
  lcd_error_t res = LCD_SUCCESS;

  display_sleep();

  if (m_dma_enabled) {
    // disable IRQ
    irq_set_enabled(LCD_DMA_IRQ, false);
    //irq_set_exclusive_handler(LCD_DMA_IRQ, NULL);  // will assert

    // disable DMA
    for (int i = 0; i < NUM_LCD_DMA; i++) {
      dma_channel_cleanup(m_dma_chan[i]);
      dma_channel_unclaim(m_dma_chan[i]);  
    }

    m_dma_enabled = false;
  }

  if (m_pio_enabled) {
    pio_sm_set_enabled(m_pio, m_pio_sm_scanout, false /* disable */);
    pio_sm_unclaim(m_pio, m_pio_sm_scanout);
    pio_sm_set_enabled(m_pio, m_pio_sm_rot, false /* disable */);
    pio_sm_unclaim(m_pio, m_pio_sm_rot);
    #if LCD_COLORDEPTH==8
    pio_sm_set_enabled(m_pio, m_pio_sm_clut, false /* disable */);
    pio_sm_unclaim(m_pio, m_pio_sm_clut);
    #endif

    if (m_pio == pio0) {
      reset_block(RESETS_RESET_PIO0_BITS);
      unreset_block_wait(RESETS_RESET_PIO0_BITS);
      pio_clear_instruction_memory(pio0);
    } else if (m_pio == pio1) {
      reset_block(RESETS_RESET_PIO1_BITS);
      unreset_block_wait(RESETS_RESET_PIO1_BITS);
      pio_clear_instruction_memory(pio1);
    } else if (m_pio == pio2) {
      reset_block(RESETS_RESET_PIO2_BITS);
      unreset_block_wait(RESETS_RESET_PIO2_BITS);
      pio_clear_instruction_memory(pio2);
    }

    m_pio_enabled = false;
  }

  if (m_fb_rlut) {
    free((void*) m_fb_rlut);
    m_fb_rlut = NULL;
  }

  for (int i = 0; i < m_num_fb; i++) {
#if 0
    if (m_fb[i]) {
      free((void*) m_fb[i]);
      m_fb[i] = NULL;
    }
#endif
    if (m_fb_rlut_backup[i]) {
      free((void*) m_fb_rlut_backup[i]);
      m_fb_rlut_backup[i] = NULL;
    }
  }

  m_lcd_enabled = false;
  lcd_this = NULL;

  return res;

}  // deinit

void LCD_DPI::build_fb_rot_lut() {

  // TODO: Investigate. Working theory for the lost words is: During vblanking the scanout SM cannot accept any incoming data.
  // However, DMA0 keeps firing data at DMA1 as long as the PIO's DREQ will ask for it. With the SM's FIFO not getting
  // emptied. In consequence, some data might still be in flight from DMA0 to DMA1 to the FIFO while the FIFO is already full
  int offset_x = +3 + 8;  // 8 = (240 - SNES_HEIGHT) / 2
  int offset_y = 8;  // 8 = (SNES_WIDTH - 240) / 2
 
  //+ (LCD_PHYS_RES_X / m_lcd_scale - 224 /* SNES_EXTENDED_HEIGHT*/) / 2

  uint32_t ofs = (1u << m_dma_ring_buffer_size) + offset_x;
  for (int h = m_num_fb - 1; h >= 0; h--) {
    // clear RLUT
    for (int i = 0; i < (1u << m_dma_ring_buffer_size); i++)
      m_fb_rlut[i] = (uint32_t) &m_fb[h][225 * 256 /* SNES_WIDTH */ - 1]; // default to black
      //m_fb_rlut[i] = (uint32_t) &m_fb[h][LCD_PHYS_RES_Y / m_lcd_scale - 1];  

    // setup pixel addresses
    for (int i = 0; i < LCD_PHYS_RES_X / m_lcd_scale; i++) {
      uint16_t index = (i + ofs) % (1u << m_dma_ring_buffer_size);
      if ((index >= (offset_x + 0)) && (index < (offset_x + 224)))  // visible 224 pixels of the snes
        m_fb_rlut[index] = (uint32_t) &m_fb[h][(i + 1) * 256 /* SNES_WIDTH */ - offset_y - 1];
    }

    // repeat m_lcd_scale times
    for (int i = (1u << m_dma_ring_buffer_size) / m_lcd_scale; i < (1u << m_dma_ring_buffer_size); i++) {
      uint16_t index1 = (i + ofs) % (1u << m_dma_ring_buffer_size);
      uint16_t index2 = ((i % ((1u << m_dma_ring_buffer_size) / m_lcd_scale)) % ((1u << m_dma_ring_buffer_size) / m_lcd_scale) + ofs) % (1u << m_dma_ring_buffer_size);
      m_fb_rlut[index1] = m_fb_rlut[index2];
    }

    for (int i = 0; i < (1u << m_dma_ring_buffer_size); i++)
      m_fb_rlut_backup[h][i] = m_fb_rlut[i];

  }

}

lcd_error_t LCD_DPI::init(uint16_t num_fb, int auto_refresh, int scale) {
  if (m_lcd_enabled)
    return LCD_IS_INIT;

  lcd_error_t res = LCD_SUCCESS;

  lcd_this = this;

  if (scale < 2 || scale > 3)
    return LCD_BAD_PARAM;
  m_lcd_scale = scale;

  switch (auto_refresh) {
    case LCD_FRR_REFRESH:
      m_refresh_mode = LCD_FRR_REFRESH;
      break;
    case LCD_VRR_REFRESH:
      m_refresh_mode = LCD_VRR_REFRESH;
      break;
    default:
      return LCD_BAD_PARAM;
  }

  if ((num_fb > NUM_FB_MAX) || (num_fb < 1))
    return LCD_NFB_ERR;

  m_num_fb = num_fb;
  
  sem_init(&dma_tx_done, 0, 1);
  sem_init(&m_pageflip_pending, m_num_fb - 1, m_num_fb - 1);

  //mutex_init(&mut_update_fb);
  //mutex_exit(&mut_update_fb);

#if 0
  for (int i = 0; i < m_num_fb; i++) {
    //m_fb[i] = (color_t *) aligned_alloc(4, LCD_PHYS_RES_X / m_lcd_scale * LCD_PHYS_RES_Y / m_lcd_scale * sizeof(color_t));
    m_fb[i] = (color_t *) aligned_alloc(4, 256 /* SNES_WIDTH */ * 224 /* SNES_HEIGHT*/ * sizeof(color_t));

/*
    // PSRAM is too slow
    // DEBUG - TODO: REMOVE ME!
    m_fb[i] = (color_t *) sfe_mem_malloc(LCD_PHYS_RES_X / m_lcd_scale * LCD_PHYS_RES_Y / m_lcd_scale * sizeof(color_t) + 8);
    m_fb[i] = (color_t *) ((uint32_t) m_fb[i] + 4 - ((uint32_t) m_fb[i]) % 4);
*/

    if (!m_fb[i]) {
      res = LCD_MEM_ERR;
      goto bailout;
    }
  }
#endif

  switch (m_rotation) {
    case 0:
    case 2:
      m_screen_width = LCD_PHYS_RES_X / m_lcd_scale;
      m_screen_height = LCD_PHYS_RES_Y / m_lcd_scale;
      break;
    case 1:
    case 3:
      m_screen_width = LCD_PHYS_RES_Y / m_lcd_scale;
      m_screen_height = LCD_PHYS_RES_X / m_lcd_scale;
      break;
    default:
      printf("Illegal rotation settings. Defaulting to 1");
      m_rotation = 1;
      m_screen_width = LCD_PHYS_RES_X / m_lcd_scale;
      m_screen_height = LCD_PHYS_RES_Y / m_lcd_scale;      
  }

  m_dma_ring_buffer_size = 0;

  for (int i = 0; i < 20; i++) {
    if ((1u << i) > LCD_PHYS_RES_X)  // TODO: check for non-square displays LCD_PHYS_RES_Y <-> LCD_PHYS_RES_X
      break;
    m_dma_ring_buffer_size++;
  }

  // TODO only when rotated
#if 1
  m_fb_rlut = (uint32_t *) aligned_alloc(1u << (m_dma_ring_buffer_size + 2 /* 4 bytes */), (1u << m_dma_ring_buffer_size) * 4 /* 32 bit addresses */);
  if ((!m_fb_rlut) || (((uint32_t) m_fb_rlut % (1u << (m_dma_ring_buffer_size + 2 /* 4 bytes */))) != 0)) {
    res = LCD_MEM_ERR;
    goto bailout;
  }

  for (int i = 0; i < m_num_fb; i++) {
    m_fb_rlut_backup[i] = (uint32_t *) aligned_alloc(4, (1u << m_dma_ring_buffer_size) * 4 /* 32 bit addresses */);
    if (!m_fb_rlut_backup[i] || (((uint32_t) m_fb_rlut_backup[i] % 4) != 0)) {
      res = LCD_MEM_ERR;
      goto bailout;
    }
  }
#endif

  build_fb_rot_lut();
  
  #if LCD_COLORDEPTH==8
  build_default_palette();
  #endif

  lcd_backlight_init();

  display_start();

  res = pio_init();
  if (res != LCD_SUCCESS)
    goto bailout;

  res = dma_init();
  if (res != LCD_SUCCESS)
    goto bailout;
  
  m_lcd_enabled = true;

  return res;

  bailout:
  deinit();

  return res;
}  // init

bool LCD_DPI::is_ready() {
  if (m_refresh_mode == LCD_VRR_REFRESH)
    return (sem_available(&dma_tx_done) == 0) ? false : true;
  else
    return true;
}

lcd_error_t LCD_DPI::get_fb(gbuffer_t* lcd_buffer) {
  lcd_buffer->bitdepth = LCD_COLORDEPTH;
  lcd_buffer->width = m_screen_width;
  lcd_buffer->height = m_screen_height;
  lcd_buffer->data = m_fb[m_fb_cur];

  return LCD_SUCCESS;
}

lcd_error_t LCD_DPI::get_backbuffer(gbuffer_t* lcd_buffer) {

  if (m_num_fb != 1) {
      // wait for a free buffer
      while (sem_available(&m_pageflip_pending) == 0)
        tight_loop_contents();

      //mutex_try_enter(&mut_update_fb, NULL);
      int res = m_fb_cur;
      int fb_ahead = m_num_fb - sem_available(&m_pageflip_pending);  // skip next buffer as it has already been enqueued for scanout
      //mutex_exit(&mut_update_fb);

      for (int i = 0; i < fb_ahead; i++) {
        res++;
        if (res == m_num_fb)
          res = 0;
      }

    lcd_buffer->bitdepth = LCD_COLORDEPTH;
    lcd_buffer->width = 256; //m_screen_width;
    lcd_buffer->height = 224; //m_screen_height;
    lcd_buffer->data = m_fb[res];

    return LCD_SUCCESS;

  }

  // No back buffer available

  lcd_buffer->bitdepth = 0;
  lcd_buffer->width = 0;
  lcd_buffer->height = 0;
  lcd_buffer->data = NULL;
    
  return LCD_NFB_ERR;

}

lcd_error_t LCD_DPI::request_pageflip() {
  if (!m_lcd_enabled)
    return LCD_NOT_INIT;

  if (m_num_fb > 1)
    sem_acquire_blocking(&m_pageflip_pending);

  return LCD_SUCCESS;

}

#endif // LCD_IF_DPI