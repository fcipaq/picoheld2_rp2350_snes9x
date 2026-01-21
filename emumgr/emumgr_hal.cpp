/*
 * emumgr - Emulation manager for Snes9x on the Pico Held 2
 *
 * Copyright (C) 2025 Daniel Kammer (daniel.kammer@web.de)
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

 #include <cstdio>

 #include <picoengine.h>

 /* SD support */
#include "tf_card.h"
#include "ff.h"

#include "hardware/flash.h"
#include <hardware/xip_cache.h>
#include "pico/bootrom.h"
#include "hardware/watchdog.h"

#include "heapstrings.h"

#include "emumgr.h"
#include "emumgr_hal.h"

extern "C" {
#include "sparkfun_pico/sfe_pico.h"
}

#define ALPHABETIZE

bool m_sd_init = false;
FIL fil;
FATFS fs_handle;
extern LCD_DPI lcd;

/* ============================================ HAL ===================================================*/
bool em_hal_init() {
  watchdog_enable(5000, false);
  return true;
}

/* ============================================ LCD ===================================================*/
void em_get_backbuffer(gbuffer_t* fb_back) {
  lcd.get_backbuffer(fb_back);
}

void em_request_pageflip() {
  // this is a relatively reliable location in the code
  // to check whether the main thread is still running
  watchdog_hw->load = 5000 * 1000;  // 2s

  lcd.request_pageflip();
}

void em_lcd_refresh() {
  lcd.refresh();  // for FRR this does nothing
}

void em_lcd_backlight_set(int value) {
  lcd_backlight_set(value);
}

/* ============================================ PSRAM ===================================================*/

void* em_psram_malloc(uint32_t size) {
  return sfe_mem_malloc((size_t) size);
}

void em_psram_free(void* buffer) {
  sfe_mem_free(buffer);
}

/* ============================================ filesystem ===================================================*/

bool em_get_directory(char *path, char* buffer) {
    FRESULT res;
    DIR dir;
    FILINFO fno;

    int buf_pos = 0;
    int item_cnt = 0;

    res = f_opendir(&dir, path);                   /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);           /* Read a directory item */
            if ((fno.fname[0] == 0) || (item_cnt >= DIRECTORY_MAX_ITEMS)) break;          /* Error or end of dir/RAM */
            if (!(fno.fattrib & AM_DIR)) {
#ifdef ALPHABETIZE
              // alphabetize  
              for (int i = 0; i < item_cnt; i++) {
                bool leave = false;                
                char* item_buf = get_item_aligned(i, buffer);
                int len = strlen(item_buf) > strlen(fno.fname) ? strlen(fno.fname) : strlen(item_buf);
                for (int j = 0; j < len; j++) {
                  // case "==" unnecessary since two file names cannot be the same

                  // skip item
                  if (symbol_rank(fno.fname[j]) > symbol_rank(item_buf[j]))
                    break;

                  // that's the spot
                  if (symbol_rank(fno.fname[j]) < symbol_rank(item_buf[j])) {
                    insert_item_aligned(i, fno.fname, buffer);
                    leave = true;
                    break;
                  }
                }
                if (leave)
                  break;
              }
#else
              add_item_aligned(fno.fname, buffer);
#endif
              item_cnt++;
            }
        }
        f_closedir(&dir);
    } else {
        printf("Error reading directory: \"%s\". (%u)\n", path, res);
        return false;
    }

    //buffer[buf_pos] = terminatorstring;
    return true;
}

uint32_t em_f_size() {
    return f_size(&fil);
}

bool em_f_mkdir(char* path) {
    f_mkdir(path);
    return 0;
}

bool em_f_open(const char* filename, uint8_t access_mode) {
    FRESULT fr;
    uint8_t mode;

    if (access_mode == EM_FA_R)
      mode = FA_READ;

    if (access_mode == EM_FA_RWC)
      mode = FA_READ | FA_WRITE | FA_CREATE_ALWAYS;

    f_close(&fil);
    fr = f_open(&fil, (char*) filename, mode);

    if (fr == FR_OK)
      return true;
    else
      return false;

}

bool em_f_read(uint8_t* buf, int len, uint32_t* bytes_read) {
  FRESULT fr;
  UINT br;
  fr = f_read(&fil, buf, len, (UINT*) &br);
  *bytes_read = br;

  if (fr == FR_OK)
    return true;
  else
    return false;
}

bool em_f_write(uint8_t* buf, int len, uint32_t* bytes_written) {
  FRESULT fr;
  UINT bw;
  fr = f_write(&fil, buf, len, (UINT*) &bw);
  *bytes_written = bw;

  if (fr == FR_OK)
    return true;
  else
    return false;
}

bool em_f_close() {
  f_close(&fil);
  return true;
}

bool em_sd_deinit() {
  if (!m_sd_init)
    return true;

  f_mount(NULL, "", 0); // unmount
  //sleep_ms(500);
  pwr_sd_sply(0);

  m_sd_init = false;

  return true;
}

bool em_sd_init() {
  if (m_sd_init)
    return true;

  FRESULT fr;     /* FatFs return code */

  pwr_sd_sply(1);
  sleep_ms(250);  // Is that enough?

  // modify below if customized configuration is needed
  pico_fatfs_spi_config_t config = {
      spi0,  // if unmatched SPI pin assignments with spi0/spi1 or explicitly designated as NULL, SPI PIO will be configured 
      CLK_SLOW_DEFAULT,
      150000000 / 4, //CLK_FAST_DEFAULT,
      PIN_SD_MISO,  // SPIx_RX
      PIN_SD_CS,
      PIN_SD_SCK,   // SPIx_SCK
      PIN_SD_MOSI,  // SPIx_TX
      true   // use internal pullup
  };

  if (!pico_fatfs_set_config(&config)) {
    printf("Cannot setup SPI for SD card.\n");
    return false;
  }

  fr = f_mount(&fs_handle, "", 1);

  if (fr != FR_OK) {
    printf("Mounting failed: %d - retrying.\n", (int) fr);
    return false;
  }

  printf("Mounting complete\n");

  m_sd_init = true;

  return true;
}

/* ============================================ flash ===================================================*/

static void call_flash_range_erase(void *param) {
    uint32_t offset = (uint32_t)param;
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
}

bool em_flash(void* address) {
  int rc = flash_safe_execute(call_flash_range_erase, address, UINT32_MAX /* max delay*/ );

  //hard_assert(rc == PICO_OK);

  if (rc == PICO_OK)
    return true;
  else
    return false;
}

int em_get_flash_sector_size() {
    return FLASH_SECTOR_SIZE;
}

uint32_t em_get_flash_base_addr() {
    return XIP_BASE;
}

void em_flush_cache() {
  flash_flush_cache();    
  xip_cache_clean_all();
}
