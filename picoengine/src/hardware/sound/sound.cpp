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

/* ========================== includes ========================== */
#include "sound.h"
#include <hwcfg.h>
#include <setup.h>
#include <typedefs.h>

#include <stdint.h>
#include <stdio.h>
#include <cstdlib>

#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pico/sem.h"
#include "pico/mutex.h"

#include "snd_pio.pio.h"

/* ========================= definitions ========================= */

/* ============================= variables ===========================*/
/* ----------------------------- control -----------------------------*/
uint8_t snd_init_complete = 0;
bool snd_pio_complete = false;

mutex_t snd_ready;
volatile int16_t* snd_next_buffer = NULL;
volatile int snd_next_buffer_len = 0;
int preload_buffer;

/* ----------------------------- PIO -----------------------------*/
PIO snd_pio = pio1;
int8_t snd_pio_sm = 0;  // pioinit will claim a free one

// Updated later with the loading offset of the PIO program.
uint32_t snd_pio_program_offset = 0;

/* ----------------------------- DMA -----------------------------*/
int32_t snd_dma_chan;
dma_channel_config snd_dma_config;

uint8_t snd_dma_enabled = 0;

volatile uint8_t snd_playing = false;

/* ============================= implementation ===========================*/

void __isr snd_dma_handler() {
  if (!dma_channel_get_irq1_status(snd_dma_chan))
    return;

  // clear irq flag
  dma_channel_acknowledge_irq1(snd_dma_chan);

  dma_channel_configure(snd_dma_chan, &snd_dma_config, &snd_pio->txf[snd_pio_sm], snd_next_buffer, snd_next_buffer_len, true);

  mutex_exit(&snd_ready);

}

bool snd_dma_init() {
  if (snd_dma_enabled)
    return false;

  snd_dma_chan = dma_claim_unused_channel(false);

  if (snd_dma_chan < 0)
    return false;

  snd_dma_config = dma_channel_get_default_config(snd_dma_chan);

  channel_config_set_transfer_data_size(&snd_dma_config, DMA_SIZE_16);
  channel_config_set_dreq(&snd_dma_config, pio_get_dreq(snd_pio, snd_pio_sm, true));

  dma_channel_set_irq1_enabled(snd_dma_chan, true);

  irq_set_exclusive_handler(DMA_IRQ_1, snd_dma_handler);
  
  irq_set_enabled(DMA_IRQ_1, true);

  snd_dma_enabled = true;

  return true;
}

void snd_dma_shutdown(void) {
  if (!snd_dma_enabled)
    return;

  dma_channel_unclaim(snd_dma_chan);

  snd_dma_enabled = false;
}

/*
 * Set output frequency in Hz
 */
int snd_set_freq(uint32_t freq) {

  if (!snd_pio_complete)
    return SND_NOT_INIT;

  uint32_t fcpu = clock_get_hz(clk_sys);

  printf("CPU freq.: %d\n", fcpu);

  freq *= 2 * 16 * 2 * 2; /* 2 channels, 16 bits, 2 PIO cycles per clock cycle, ? */

  uint16_t clock_div = fcpu / freq;
  uint16_t clock_fract = (uint16_t) (((float)fcpu / (float)freq - (float)clock_div) * 256.0);

//  if (clock_fract > 127)
//    clock_div++;

  // max speed exceeded
  if (clock_div == 0)
    return SND_PIO_ERR;

  // Set clock divider and fractional divider
    pio_sm_set_clkdiv_int_frac8(snd_pio, snd_pio_sm, clock_div, 1);
    printf("CPU freq.: %d\n", fcpu);
    printf("clock div.: %d\n", clock_div);


  return SND_SUCCESS;
}

void snd_mute() {
  gpio_put(PIN_SND_EN, 0);
}

void snd_unmute() {
  gpio_put(PIN_SND_EN, 1);
}

bool snd_pio_init() {

  if (snd_pio_complete)
    return SND_PIO_ERR;

  snd_pio_sm = pio_claim_unused_sm(snd_pio, false);

  if (snd_pio_sm < 0)
    return false;

  #if (PICO_PIO_USE_GPIO_BASE!=1)
  #error "Something's messed up with PICO_PIO_USE_GPIO_BASE"
  #endif
  
  pio_set_gpio_base(snd_pio, 16);

  snd_pio_program_offset = pio_add_program(snd_pio, &snd_output_program);
  pio_sm_config c = snd_output_program_get_default_config(snd_pio_program_offset); 
  
  pio_gpio_init(snd_pio, PIN_I2S_DAT);
  pio_gpio_init(snd_pio, PIN_I2S_LR);
  pio_gpio_init(snd_pio, PIN_I2S_BCK);

  sm_config_set_out_pins(&c, PIN_I2S_DAT, 1);
  sm_config_set_sideset_pin_base(&c, PIN_I2S_LR);
  sm_config_set_sideset(&c, 2, false, false);

  pio_sm_set_consecutive_pindirs(snd_pio, snd_pio_sm, PIN_I2S_LR, 3, true);

  sm_config_set_out_shift(&c, false, false, 0);    // OSR shifts out to the left (false): MSB shifts out first, no autopull, so no pull threshold

  if (pio_sm_init(snd_pio, snd_pio_sm, snd_pio_program_offset, &c) != PICO_OK)
    return false;

  pio_sm_set_enabled(snd_pio, snd_pio_sm, true);

  snd_pio_complete = true;

  return true;
}  //pioInit

/*
 * Enque a buffer to be played
 * A buffer will be dequed and assigned to played. If there is currently no
 * free buffer then the function will either wait for a free buffer (BLOCKING)
 * or return 0 (NONBLOCKING)
 * The number of free buffers will be returned.
 * In BLOCKING mode returning 0 means, a free buffer had to be waited for.
 * In NONBLOCKING mode, returning 0 means, no free buffer was available and
 * the sound data could not be transmitted.
 */
void snd_enqueue_buffer_blocking(int16_t* snd_buffer, int len) {
  if (preload_buffer > 1) {
    while (!mutex_try_enter(&snd_ready, NULL))
      tight_loop_contents();
  }
    
  if (preload_buffer == 1)
    dma_channel_configure(snd_dma_chan, &snd_dma_config, &snd_pio->txf[snd_pio_sm], snd_next_buffer, snd_next_buffer_len, true);

  snd_next_buffer = snd_buffer;
  snd_next_buffer_len = len;

  if (preload_buffer < 2)
    preload_buffer++;

}

bool snd_init() {
  if (snd_init_complete)
    return SND_SUCCESS;

  gpio_init(PIN_SND_EN);
  gpio_set_dir(PIN_SND_EN, GPIO_OUT);
  snd_mute();

  mutex_init(&snd_ready);
  mutex_try_enter(&snd_ready, NULL);

  if (!snd_pio_init())
    return SND_PIO_ERR;

  if (!snd_dma_init())
    return SND_DMA_ERR;

  snd_set_freq(44100);

  snd_init_complete = true;

  return SND_SUCCESS;
}