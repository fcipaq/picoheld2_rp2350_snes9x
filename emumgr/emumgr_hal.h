/*
 * emumgr - emulation manager for GWENESIS on the Pico Held
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
 
#pragma once

#define EM_FA_R (1)
#define EM_FA_RWC (2)

#define DIRECTORY_MAX_ITEMS (100)
#define DIRECTORY_BUFFER_SIZE (DIRECTORY_MAX_ITEMS * MAX_ITEM_LEN + 1)

#define FLASH_ROM_ADDRESS (0x500000 + XIP_BASE)
#define CAP_FLASH_ROM
/* ============================================ HAL ===================================================*/
bool em_hal_init();

/* ============================================ LCD ===================================================*/
void em_get_backbuffer(gbuffer_t* fb_back);
void em_request_pageflip();
void em_lcd_refresh();
void em_lcd_backlight_set(int value);
int em_lcd_get_width();
int em_lcd_get_height();

/* ============================================ PSRAM ===================================================*/
void* em_psram_malloc(uint32_t size);
void em_psram_free(void* buffer);

/* ============================================ filesystem ===================================================*/
bool em_get_directory(char *path, char* buffer);

uint32_t em_f_size();
bool em_f_mkdir(char* path);
bool em_f_open(const char* filename, uint8_t access_mode);
bool em_f_read(uint8_t* buf, int len, uint32_t* bytes_read);
bool em_f_write(uint8_t* buf, int len, uint32_t* bytes_written);
bool em_f_close();

bool em_sd_deinit();
bool em_sd_init();

/* ============================================ flash ===================================================*/
static void call_flash_range_erase(void *param);
bool em_flash(void* address);
int em_get_flash_sector_size();
uint32_t em_get_flash_base_addr();

void em_flush_cache();
