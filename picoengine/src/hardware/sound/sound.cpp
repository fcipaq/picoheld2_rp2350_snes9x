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

#include "snd_pio.pio.h"

/* ========================= definitions ========================= */
#define SND_ALLOW_DIRECT

/* ============================= variables ===========================*/
/* ----------------------------- control -----------------------------*/
uint8_t snd_init_complete = 0;
bool snd_pio_complete = false;

/* ----------------------------- Postprocess -----------------------------*/
uint8_t snd_level = 1;

/* ----------------------------- PIO -----------------------------*/
PIO snd_pio = pio1;
int8_t snd_pio_sm = 0;  // pioinit will claim a free one

// Updated later with the loading offset of the PIO program.
uint32_t snd_pio_program_offset = 0;

/* ----------------------------- DMA -----------------------------*/
int32_t snd_dma_chan;
dma_channel_config snd_dma_config;

uint8_t snd_dma_enabled = 0;

/* --------------------------- sound tx ---------------------------*/
struct snd_channel_t {
  void* snd_data[SND_NUM_BUFS];
  uint8_t snd_bitdepth[SND_NUM_BUFS];
  uint32_t snd_data_len[SND_NUM_BUFS];
  uint32_t snd_data_pos[SND_NUM_BUFS];
  volatile uint8_t num_cur_snd_data;
  volatile uint8_t num_nxt_snd_data;
  semaphore_t num_free_bufs;
};

struct snd_channel_t snd_channel[SND_NUM_CHAN];

uint32_t snd_buf_size = SND_BUF_SIZE;

// local DMA buffer
volatile uint8_t num_cur_snd_buf;
int16_t snd_buf[2][SND_BUF_SIZE];  

volatile uint32_t snd_buf_pos[2];

volatile uint8_t snd_playing = false;

/* ============================= implementation ===========================*/

// Set volume level.
// 0 is most quiet and 5 is loudest.
void snd_set_vol(int8_t vol) {
  if (vol > 5 || vol < 0)
    return;
  snd_level = 6 - vol;
}

/*
 * Returns the number of items waiting in the sound buffer queue of
 * a channel waiting to be played. Zero means idle.
 */
int snd_num_bufs_waiting(uint8_t num_snd_channel) {
  if (!snd_init_complete)
    return SND_NOT_INIT;

  if (num_snd_channel > SND_NUM_CHAN - 1)
    return SND_CHAN_INVALID;

  return SND_NUM_BUFS - sem_available(&(snd_channel[num_snd_channel].num_free_bufs));
}

/*
 * Returns the number of available buffers of
 * a channel. SND_NUM_BUFS means idle.
 */
int snd_num_bufs_free(uint8_t num_snd_channel) {
  if (!snd_init_complete)
    return SND_NOT_INIT;

  if (num_snd_channel > SND_NUM_CHAN - 1)
    return SND_CHAN_INVALID;

  return sem_available(&(snd_channel[num_snd_channel].num_free_bufs));
}

int snd_cancel_channel(uint8_t num_snd_channel) {
  if (!snd_init_complete)
    return SND_NOT_INIT;

  /*
  dma_channel_abort(snd_dma_chan);
  
  snd_playing = false;
    if (snd_buf != NULL)
	  free((void*) snd_buf);
*/

  if ((num_snd_channel > SND_NUM_CHAN - 1) && (num_snd_channel != SND_CHAN_ALL))
    return SND_CHAN_INVALID;

  int loop_start = num_snd_channel;
  int loop_end = num_snd_channel + 1;

  if (num_snd_channel == SND_CHAN_ALL) {
    loop_start = 0;
    loop_end = SND_NUM_CHAN;
  }

  for (int i = loop_start; i < loop_end; i++) {
    for (int h = 0; h < SND_NUM_BUFS - sem_available(&(snd_channel[i].num_free_bufs)); h++)
      sem_release(&(snd_channel[i].num_free_bufs));

    snd_channel[i].num_cur_snd_data = 0;
  }

  return SND_SUCCESS;
}

void snd_prepare_buf() {

  uint32_t max_len;
  bool buffers_waiting;
  snd_buf_pos[num_cur_snd_buf] = 0;

  num_cur_snd_buf++;
  if (num_cur_snd_buf == 2)
    num_cur_snd_buf = 0;


  do {
    max_len = snd_buf_size;
    buffers_waiting = false;

    // get number of bytes in smallest buffer
    for (int i = 0; i < SND_NUM_CHAN; i++) {
      if (sem_available(&(snd_channel[i].num_free_bufs)) != SND_NUM_BUFS) {

        int32_t len = snd_channel[i].snd_data_len[snd_channel[i].num_cur_snd_data] - snd_channel[i].snd_data_pos[snd_channel[i].num_cur_snd_data];
        // TODO: (snd_buf_size - snd_buf_pos[num_cur_snd_buf]) might get negative?
        uint32_t tx_len = len > (snd_buf_size - snd_buf_pos[num_cur_snd_buf]) ? (snd_buf_size - snd_buf_pos[num_cur_snd_buf]) : len;

        if (tx_len < max_len)
          max_len = tx_len;

        buffers_waiting = true;
      }
    }

    // mix channels
    int16_t* buf_tmp_16[SND_NUM_CHAN];
    int8_t* buf_tmp_8[SND_NUM_CHAN];

    for (int i = 0; i < SND_NUM_CHAN; i++) {
      if (snd_channel[i].snd_bitdepth[snd_channel[i].num_cur_snd_data] == 16)
        buf_tmp_16[i] = (int16_t*) snd_channel[i].snd_data[snd_channel[i].num_cur_snd_data];
      else if (snd_channel[i].snd_bitdepth[snd_channel[i].num_cur_snd_data] == 8)
        buf_tmp_8[i] = (int8_t*) snd_channel[i].snd_data[snd_channel[i].num_cur_snd_data];
    }

    for (uint32_t h = 0; h < max_len; h++)
      snd_buf[num_cur_snd_buf][h + snd_buf_pos[num_cur_snd_buf]] = 0;

    for (int i = 0; i < SND_NUM_CHAN; i++) {
      if (sem_available(&(snd_channel[i].num_free_bufs)) != SND_NUM_BUFS) {
        if (snd_channel[i].snd_bitdepth[snd_channel[i].num_cur_snd_data] == 16)
          for (uint32_t h = 0; h < max_len; h++)
            snd_buf[num_cur_snd_buf][h + snd_buf_pos[num_cur_snd_buf]] += buf_tmp_16[i][h + snd_channel[i].snd_data_pos[snd_channel[i].num_cur_snd_data]] / snd_level;
        else if (snd_channel[i].snd_bitdepth[snd_channel[i].num_cur_snd_data] == 8)
          for (uint32_t h = 0; h < max_len; h++)
            snd_buf[num_cur_snd_buf][h + snd_buf_pos[num_cur_snd_buf]] += (int16_t) ((((int16_t) buf_tmp_8[i][h + snd_channel[i].snd_data_pos[snd_channel[i].num_cur_snd_data]])) << 8 ) / snd_level;
            //snd_buf[num_cur_snd_buf][h + snd_buf_pos[num_cur_snd_buf]] += (int16_t) (((int16_t) buf_tmp_8[i][h + snd_channel[i].snd_data_pos[snd_channel[i].num_cur_snd_data]]) << 8 ) / snd_level;

      }
    }

    // check if channels still have more buffers to be played
    for (int i = 0; i < SND_NUM_CHAN; i++)
      if (sem_available(&(snd_channel[i].num_free_bufs)) != SND_NUM_BUFS) {
        int32_t len = snd_channel[i].snd_data_len[snd_channel[i].num_cur_snd_data] - snd_channel[i].snd_data_pos[snd_channel[i].num_cur_snd_data];
        if (len <= max_len) {
          // whole buffer has been played, next one
          sem_release(&(snd_channel[i].num_free_bufs));

          // cycle through available buffers
          snd_channel[i].num_cur_snd_data++;
          if (snd_channel[i].num_cur_snd_data == SND_NUM_BUFS)
            snd_channel[i].num_cur_snd_data = 0;

        } else {
          // Unable to play the whole buffer at once - splitting
          snd_channel[i].snd_data_pos[snd_channel[i].num_cur_snd_data] += max_len;
        }
      }

    if (buffers_waiting)
      snd_buf_pos[num_cur_snd_buf] += max_len;

  } while ((max_len != 0) && (buffers_waiting));

  if (buffers_waiting)
    snd_playing = true;
  else
    snd_playing = false;
}  // snd_prepare_buf

void snd_tx() {
  if (dma_channel_is_busy(snd_dma_chan))
    return;

  dma_channel_configure(snd_dma_chan, &snd_dma_config, &snd_pio->txf[snd_pio_sm], snd_buf[num_cur_snd_buf], snd_buf_pos[num_cur_snd_buf], true);  // snd_buf_pos is also the length

  snd_prepare_buf();
}

void __isr snd_dma_handler() {
  if (!dma_channel_get_irq1_status(snd_dma_chan))
    return;

  // clear irq flag
  dma_channel_acknowledge_irq1(snd_dma_chan);

  if (snd_playing) {
    // deque buffer if it's txSound why we're here
    // only call txSound if there are more buffer available
    snd_tx();
    return;
  } else {
    //      for (int h = 0; h < 2; h++)
    //	    if (snd_buf[h] != NULL) {
    //         free((void*) snd_buf[h]);
    //		  snd_buf[h] = NULL;
    //	    }
  }
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

#ifndef SND_ALLOW_DIRECT
  dma_channel_set_irq1_enabled(snd_dma_chan, true);

  irq_set_exclusive_handler(DMA_IRQ_1, snd_dma_handler);
#endif

  snd_dma_enabled = true;

  return true;
}

#ifdef SND_ALLOW_DIRECT
void snd_dma_send_buffer(int16_t* buffer, int length) {
  while (dma_channel_is_busy(snd_dma_chan))
    tight_loop_contents();

  dma_channel_configure(snd_dma_chan, &snd_dma_config, &snd_pio->txf[snd_pio_sm], buffer, length, true);
}
#endif

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
int snd_enque_buf(sbuffer_t snd_buffer, uint8_t num_snd_channel, bool blocking) {
  
  if (!snd_init_complete)
    return SND_NOT_INIT;

  if (num_snd_channel > SND_NUM_CHAN - 1)
    return SND_CHAN_INVALID;

  // this is a possible race condition when using multi threading
  // buffer enque.

  // request a buffer
  if (!sem_try_acquire(&(snd_channel[num_snd_channel].num_free_bufs))) {
    if (blocking == SND_BLOCKING)
      // wait for a free buffer in blocking mode
      sem_acquire_blocking(&(snd_channel[num_snd_channel].num_free_bufs));
    else
      // leave in non-blocking mode (not waiting for a buffer)
      return SND_NO_FREE_BUF;
  }

  snd_channel[num_snd_channel].num_nxt_snd_data++;

  if (snd_channel[num_snd_channel].num_nxt_snd_data == SND_NUM_BUFS)
    snd_channel[num_snd_channel].num_nxt_snd_data = 0;

  snd_channel[num_snd_channel].snd_data[snd_channel[num_snd_channel].num_nxt_snd_data] = snd_buffer.data;
  snd_channel[num_snd_channel].snd_data_len[snd_channel[num_snd_channel].num_nxt_snd_data] = snd_buffer.len;
  snd_channel[num_snd_channel].snd_bitdepth[snd_channel[num_snd_channel].num_nxt_snd_data] = snd_buffer.bitdepth;
  snd_channel[num_snd_channel].snd_data_pos[snd_channel[num_snd_channel].num_nxt_snd_data] = 0;

  snd_playing = true;

  // if there is only one buffer in use (the one that
  // has just been dequed) then that means
  // there is currently no ongoing transmission and
  // therefore the transmission must be reinitiated
  if (sem_available(&(snd_channel[num_snd_channel].num_free_bufs)) == (SND_NUM_BUFS - 1)) {
    snd_channel[num_snd_channel].num_cur_snd_data = snd_channel[num_snd_channel].num_nxt_snd_data;

    bool reinit_tx = true;

    // check if other channels have ongoing transmissions
    // TODO: COULD A RACE CONDITION OCCUR HERE?
    // WHERE THE CHECK IS NEGATIVE BUT THERE IS CURRENTLY AN ONGOING ISR?
    for (int h = 0; h < SND_NUM_CHAN; h++)
      if (sem_available(&(snd_channel[h].num_free_bufs)) != SND_NUM_BUFS && h != num_snd_channel)
        reinit_tx = false;

    if ((reinit_tx) && !irq_is_enabled(DMA_IRQ_1)) {
      /*
	  for (int h = 0; h < 2; h++) {
	    if (snd_buf[h] == NULL)
          snd_buf[h] = (uint8_t*) malloc(snd_buf_size);
        if (snd_buf[h] == NULL) {
	      if (h == 1)
		   // buffer 0 has already been allocated --> undo
	    	free((void*) snd_buf[0]);  
		  return SND_NO_RAM;
		}
	  }
*/
      irq_set_enabled(DMA_IRQ_1, true);
      snd_prepare_buf();
      snd_tx();
    }
  }

  return SND_SUCCESS;
}

bool snd_init() {
  if (snd_init_complete)
    return SND_SUCCESS;

  gpio_init(PIN_SND_EN);
  gpio_set_dir(PIN_SND_EN, GPIO_OUT);
  snd_mute();
  
  if (!snd_pio_init())
    return SND_PIO_ERR;

  if (!snd_dma_init())
    return SND_DMA_ERR;

  for (int h = 0; h < SND_NUM_CHAN; h++) {
    // setup semaphores and init channels
    sem_init(&(snd_channel[h].num_free_bufs), SND_NUM_BUFS, SND_NUM_BUFS);
    snd_channel[h].num_cur_snd_data = 0;
    snd_channel[h].num_nxt_snd_data = 0;
  }


  snd_set_freq(44100);

  snd_init_complete = true;

  return SND_SUCCESS;
}
