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
 
#include <string>
#include <initializer_list>

/* SD support */
#include "tf_card.h"
#include "ff.h"

/* Pico Held hw library */
#include <picoengine.h>

#include "hardware/flash.h"
#include <hardware/xip_cache.h>
#include "pico/bootrom.h"

#include "emumgr.h"
#include "emumgr_lang_de.h"
//#include "emumgr_lang_en.h"

// assets
#include "../assets/rect.h"
#include "../assets/scroll_bar.h"
#include "../assets/battery.h"
//#include <fonts/fontfiles/pixelmix_14_16.h>
#include <fonts/fontfiles/picoheld_pixel_6x7.h>

extern "C" {
/* Snes9x */
#include "snes9x/snes9x.h"
#include "snes9x/soundux.h"
#include "snes9x/memmap.h"
#include "snes9x/apu.h"
#include "snes9x/display.h"
#include "snes9x/cpuexec.h"
#include "snes9x/srtc.h"
#include "snes9x/save.h"
#include "snes9x/gfx.h"

// PSRAM
#include "sparkfun_pico/sfe_pico.h"
}

#define FLASH_ROM_ADDRESS (0x500000)

uint32_t millis();
uint32_t micros();
int progress_bar(char* caption, int value, gbuffer_t fb);
int message(char* s);

/* ====================================================== variables ====================================================== */
// misc
int menu_first_run = 1;
extern LCD_DPI lcd;
FATFS fs_handle;
bool m_sd_init = false;
emulator_state_t emulator_state;
void* psram_rom;
int translucency = 50;

// starfield
#define NUM_STARS (100)
uint8_t* stars_x;
uint8_t* stars_y;
uint8_t* stars_color;
uint8_t* stars_speed;
uint32_t stars_timer;

// battery indicator
gbuffer_t draw_battery_gbuf;
int battery_last_level = -1;
int supply_type_last = -1;

/* ====================================================== end variables ====================================================== */

#define MENU_FONT picoheld_pixel_6x7
//#define MENU_FONT font_13x16
//#define MENU_FONT font_8x11

#define MENU_FONT_SHADOW_COLOR rgb_col_888_565(30, 30, 30)
#define MENU_FONT_SELECTED_COLOR rgb_col_888_565(230, 0, 200)
#define MENU_FONT_UNSELECTED_COLOR rgb_col_888_565(200, 200, 200)
#define MENU_FONT_CAPTION_COLOR rgb_col_888_565(200, 200, 200)

#define MENU_REPEAT_START (500)
#define MENU_REPEAT_DELAY (100)

#define MENU_OFFSET_X (30)
#define MENU_OFFSET_Y (30)
#define MENU_BORDERS_X (10)
#define MENU_BORDERS_Y (10)

#define MENU_MARGIN_VERTICAL_SCROLLBAR (15)

#define MENU_MAX_ITEMS (100)
#define MENU_MAX_DIR_ITEMS (100)
#define DIRECTORY_BUFFER_SIZE (MENU_MAX_DIR_ITEMS * (256 + 4))

#define SAVESTATE_DIRECTORY "/roms/snes/save/"
#define ROM_DIRECTORY "/roms/snes/"

#define RM_SUCCESS 0
#define RM_ERR_NO_DIR -1
#define RM_ERR_NO_ROMS -2
#define RM_ERR_READING_ROM -3
#define RM_ERR_FLASHING_ROM -4

#define SELECTION_DELAY 200
#define SELECTION_DELAY_ROMLIST 10

#define MENU_LST "\xff"

const char nullstring = 0;

const char* confirm_menu_items_overwrite[] = { STR_CONFIRM_OVERWRITE,
                                               STR_YES,
                                               STR_NO,
                                               MENU_LST };

const char* confirm_menu_items_bl[] = { STR_CONFIRM_BL,
                                                STR_YES,
                                                STR_NO,
                                                MENU_LST };

const char main_menu_items[] = { STR_MAIN_TITLE"\0"\
					 									     STR_LAUNCH"\0"\
                                 STR_LOAD"\0"\
                                 STR_SAVE"\0"\
                                 STR_SOUND"\0"\
                                 STR_BRIGHTNESS"\0"\
                                 STR_SEL_ROM"\0"\
                                 STR_OPTIONS"\0"\
                                 STR_SHUTDOWN"\0"\
                                 MENU_LST };

const char* opt_menu_items[] = { STR_OPTIONS_MENU,
                                                           STR_BUTTONS,
                                                           STR_TOGGLE_FPS,
                                                           STR_TOGGLE_Z80,
                                                           STR_LAUNCH_BL,
                                                           //                                                           "WAs",
                                                           STR_GO_BACK,
                                                           MENU_LST };

const char save_menu_items[] = { STR_SAVE_STATE"\0"\
                                 STR_SLOT_1"\0"\
                                 STR_SLOT_2"\0"\
                                 STR_SLOT_3"\0"\
                                 STR_SLOT_4"\0"\
                                 STR_SLOT_5"\0"\
                                 STR_SLOT_6"\0"\
                                 STR_GO_BACK"\0"\
                                 MENU_LST };

const char* load_menu_items[] = { STR_LOAD_STATE,
                                                             STR_SLOT_1,
                                                             STR_SLOT_2,
                                                             STR_SLOT_3,
                                                             STR_SLOT_4,
                                                             STR_SLOT_5,
                                                             STR_SLOT_6,
                                                             STR_GO_BACK,
                                                             MENU_LST };

const char* bri_menu = STR_BRIGHTNESS;

const char* snd_menu = STR_SOUND;

const char* btn_menu_items[] = { STR_BTN_MENU,
														   STR_BTN_ABC,
                                                           STR_BTN_ACB,
                                                           STR_BTN_BAC,
                                                           STR_BTN_BCA,
                                                           STR_BTN_CAB,
                                                           STR_BTN_CBA,
                                                           STR_GO_BACK,
                                                          MENU_LST };

#define ROM_FILE_LIST_NAME_LENGTH 25

#define NUM_ROM_FILE_LIST 8

const char* rom_file_list_header = STR_ROM_SELECTOR_CAPTION;

#define MSG_SAVE_NUM_ROWS 2

const char* message_save[MSG_SAVE_NUM_ROWS] = { STR_SAVING_GAME_STATE,
                                                        STR_PLEASE_WAIT };

#define MSG_ERROR_NUM_ROWS 1

const char* message_error[MSG_ERROR_NUM_ROWS] = { STR_ERROR };

int stars_init() {
  if (stars_x == NULL) {
    stars_x = (uint8_t*) sfe_mem_malloc(NUM_STARS);
    stars_y = (uint8_t*) sfe_mem_malloc(NUM_STARS);
    stars_speed = (uint8_t*) sfe_mem_malloc(NUM_STARS);
    stars_color = (uint8_t*) sfe_mem_malloc(NUM_STARS);

    if (!stars_x || !stars_y || !stars_color || !stars_speed) {
      message("error: no psram");
      return -1;
    }

    for (int i = 0; i < NUM_STARS; i++) {
      stars_x[i] = rand() % 240;
      stars_y[i] = rand() % 224;
      stars_speed[i] = (rand() % 3) + 1;
    }
  }

  return 0;
}

void stars_draw(gbuffer_t fb) {
  if (millis() - stars_timer < 30)
    return;

  stars_timer = millis();

  for (int i = 0; i < NUM_STARS; i++) {
    stars_x[i] += stars_speed[i];
    if (stars_x[i] > 247)
      stars_x[i] = 0;
  }

  color_t stars_color;
  color_t col[3];
  col[0] = rgb_col_888_565(100, 100, 100);
  col[1] = rgb_col_888_565(150, 150, 150);
  col[2] = rgb_col_888_565(200, 200, 200);

  for (int i = 0; i < NUM_STARS; i++) {
      switch (stars_speed[i]) {
        case 0:
          stars_color = col[0];
          break;
        case 1:
          stars_color = col[1];
          break;
        case 2:
          stars_color = col[2];
          break;
      }
    draw_pixel(stars_x[i], stars_y[i], stars_color, fb);
  }
}

void draw_battery(int x, int y, int percent, gbuffer_t fb_back) {

    if (gbuf_get_dat_ptr(draw_battery_gbuf) == NULL) {
      if (gbuf_create(&draw_battery_gbuf, gbuf_get_width(battery_normal_image), gbuf_get_height(battery_normal_image), (void*) sfe_mem_malloc(gbuf_get_width(battery_normal_image) * gbuf_get_height(battery_normal_image) * 2)) != BUF_SUCCESS) {
        message("error: no psram"); 
        return;
      }       
    }

    int level = percent / 25;
    
    if (level > 3)
      level = 3;

    if ((battery_last_level != level) || pwr_get_source() != supply_type_last) {
      draw_rect_fill(0, 0, gbuf_get_width(draw_battery_gbuf), gbuf_get_height(draw_battery_gbuf), 0xf81f, draw_battery_gbuf);
      battery_last_level = level;
      supply_type_last = pwr_get_source();
      if (pwr_get_source() == PWR_ON_GRD) {
        blit_buf(0, 0, 0xf81f, battery_normal_image, draw_battery_gbuf);
        blit_buf(5, 3, 0xf81f, flash_image, draw_battery_gbuf);
      } else {
        if (level) {
          blit_buf(0, 0, 0xf81f, battery_normal_image, draw_battery_gbuf);
          for (int i = 0; i < level; i++) {
            draw_rect_fill(17 - i * 5, 3, 17 - i * 5 - 3, 8, rgb_col_888_565(200, 200, 200), draw_battery_gbuf);
          }
        } else {
          blit_buf(0, 0, 0xf81f, battery_empty_image, draw_battery_gbuf);
        }
      }

    } else {
#ifdef ENABLE_PSRAM_FB
      return;
#endif      
    }

    blit_buf_blend(x, y, 0xf81f, 40, draw_battery_gbuf, fb_back);

}

int message(char* s) {
  return 0;
};

int sd_deinit() {
  if (!m_sd_init)
    return true;

  f_mount(NULL, "", 0); // unmount
  sleep_ms(500);
  pwr_sd_sply(0);

  m_sd_init = false;

  return true;
}

int sd_init() {
  if (m_sd_init)
    return true;

  int ret = 0;

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

  bool spi_configured = pico_fatfs_set_config(&config);

  if (!spi_configured) {
    printf("Cannot setup SPI for SD card.\n");
    return false;
  }

  for (int i = 0; i < 2; i++) {
    fr = f_mount(&fs_handle, "", 1);

    if (fr == FR_OK)
      break;

    printf("Mounting failed: %d - retrying.\n", (int) fr);
    pwr_sd_sply(0);
    sleep_ms(500);
    pwr_sd_sply(1);
    sleep_ms(500);
    pico_fatfs_reboot_spi();
  }

  if (fr != FR_OK)
    return false;

  printf("Mounting complete\n");

  m_sd_init = true;

  return ret;
}

int load_rom_from_sd(char* filename, gbuffer_t fb) {
  int ret = 0;
  FIL fil;
  FRESULT fr;

  int step = 0;
  int percent = 0; 

  fr = f_open(&fil, filename, FA_READ);

  if (fr != FR_OK) {
      printf("open file failed: %d\n", (int) fr);
      //message("open file failed");
      return -1;
  }
  
  printf("open complete\n");

  if (psram_rom)
    sfe_mem_free(psram_rom);

  uint32_t filesize = f_size(&fil);
  psram_rom = sfe_mem_malloc(filesize);
  uint8_t* buf = (uint8_t*) psram_rom;
  uint32_t unread_bytes = filesize;

  if (!buf) {
    printf("unable to get buffer (%d bytes).\n", filesize);
    ret = -2;
    goto bailout;
  }

  printf("Got buffer (%d bytes).\n", filesize);

  Memory.ROM = buf;
  Memory.ROM_Size = filesize;

  while (unread_bytes) {
    UINT bytes_read;
    fr = f_read (&fil, buf, 2048 /* read len */, &bytes_read);
    buf += bytes_read;
    unread_bytes -= bytes_read;
    if ((fr != FR_OK) && unread_bytes) {
      printf("read error occured, still %d unread bytes. FR is: %d\n", (int) unread_bytes, fr);
      ret = -3;
      goto bailout;
    }
    
    percent = 100 - unread_bytes * 100 / filesize;
    if (percent >= step) {
      step += 5;  // smoothness
      printf("Loading from SD: %d %\n", percent);
      progress_bar(STR_LOADING, percent, fb);
    }
  }

  printf("successfully read %d bytes from file %s\n", (int) filesize, filename);
  
bailout:

  f_close(&fil);
  
  return ret;
}

static bool screenshot_handler(const char *filename, int width, int height)
{
    //return rg_display_save_frame(filename, currentUpdate, width, height);
    return true;
}

int save_state_handler(char *filename)
{
  int ret = 0;

  printf("Savestate handler called: %s\n", filename);

  FIL fil;
  FRESULT fr;
  
  fr = f_open(&fil, filename, FA_READ | FA_WRITE | FA_CREATE_ALWAYS);

  if (fr != FR_OK) {
    printf("open file failed: %d\n", (int) fr);
    message("open save file failed");
    return -1;  // error opening
  }

  UINT bw;

  fr = f_write(&fil, (uint8_t*) &CPU, sizeof(CPU), &bw);
  if ((fr != FR_OK) || (bw != sizeof(CPU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) &ICPU, sizeof(ICPU), &bw);
  if ((fr != FR_OK) || (bw != sizeof(ICPU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) &PPU, sizeof(PPU), &bw);
  if ((fr != FR_OK) || (bw != sizeof(PPU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) &DMA, sizeof(DMA), &bw);
  if ((fr != FR_OK) || (bw != sizeof(DMA))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) Memory.VRAM, VRAM_SIZE, &bw);
  if ((fr != FR_OK) || (bw != VRAM_SIZE)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) Memory.RAM, RAM_SIZE, &bw);
  if ((fr != FR_OK) || (bw != RAM_SIZE)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) Memory.SRAM, SRAM_SIZE, &bw);
  if ((fr != FR_OK) || (bw != SRAM_SIZE)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) Memory.FillRAM, FILLRAM_SIZE, &bw);
  if ((fr != FR_OK) || (bw != FILLRAM_SIZE)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) &APU, sizeof(APU), &bw);
  if ((fr != FR_OK) || (bw != sizeof(APU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) &IAPU, sizeof(IAPU), &bw);
  if ((fr != FR_OK) || (bw != sizeof(IAPU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) IAPU.RAM, 0x10000, &bw);
  if ((fr != FR_OK) || (bw != 0x10000)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_write(&fil, (uint8_t*) &SoundData, sizeof(SoundData), &bw);
  if ((fr != FR_OK) || (bw != sizeof(SoundData))) {
    ret = -2;  // error writing
    goto bailout;
  }

  printf("Game state saved.\n\n");

  ret = 0;

  bailout:
  
  f_close(&fil);

  return ret;
}

int load_state_handler(char *filename)
{
  int ret = 0;

  FIL fil;
  FRESULT fr;

  printf("Loadstate handler called...\n\n");

  fr = f_open(&fil, filename, FA_READ);

  if (fr != FR_OK) {
    printf("open file failed: %d\n", (int) fr);
    message("open save file failed");
    return -1;  // error opening
  }

  UINT br;

  S9xReset();

  // At this point we can't go back and a failure will corrupt the state anyway
  printf("Now reading to memory.\n");

  fr = f_read (&fil, (uint8_t*) &CPU, sizeof(CPU), &br);
  if ((fr != FR_OK) || (br != sizeof(CPU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) &ICPU, sizeof(ICPU), &br);
  if ((fr != FR_OK) || (br != sizeof(ICPU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) &PPU, sizeof(PPU), &br);
  if ((fr != FR_OK) || (br != sizeof(PPU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) &DMA, sizeof(DMA), &br);
  if ((fr != FR_OK) || (br != sizeof(DMA))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) Memory.VRAM, VRAM_SIZE, &br);
  if ((fr != FR_OK) || (br != VRAM_SIZE)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) Memory.RAM, RAM_SIZE, &br);
  if ((fr != FR_OK) || (br != RAM_SIZE)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) Memory.SRAM, SRAM_SIZE, &br);
  if ((fr != FR_OK) || (br != SRAM_SIZE)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) Memory.FillRAM, FILLRAM_SIZE, &br);
  if ((fr != FR_OK) || (br != FILLRAM_SIZE)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) &APU, sizeof(APU), &br);
  if ((fr != FR_OK) || (br != sizeof(APU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) &IAPU, sizeof(IAPU), &br);
  if ((fr != FR_OK) || (br != sizeof(IAPU))) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) IAPU.RAM, 0x10000, &br);
  if ((fr != FR_OK) || (br != 0x10000)) {
    ret = -2;  // error writing
    goto bailout;
  }
  fr = f_read (&fil, (uint8_t*) &SoundData, sizeof(SoundData), &br);
  if (br != sizeof(SoundData)) {
    ret = -2;  // error writing
    goto bailout;
  }

  printf("Game state loaded.\n");

bailout:

  f_close(&fil);

  //return S9xLoadState();
  return ret;
}

//================================================================================================================================================================
#if 0

int calc_rom_checksum(uint8_t* rom_addr) {
  //todo
  return true;
}

int check_rom_valid(uint8_t* rom_addr) {
/*
  const char system_id_val1[16] = "SEGA MEGA DRIVE";
  const char system_id_val2[13] = "SEGA GENESIS";
  char system_id[16];

  memcpy(system_id, (void*)((uint32_t)rom_addr), 15);
  system_id[15] = 0;

  if (strcmp(system_id_val1, system_id))
    if (calc_rom_checksum(rom_addr))
      return true;

  system_id[12] = 0;

  if (strcmp(system_id_val2, system_id))
    if (calc_rom_checksum(rom_addr))
      return true;

  return false;
*/
  return true;
}

  // reset machine
  // let's just hope noone messed with the WD settings...
  watchdog_reboot(0, 0, 1);
  watchdog_enable(1, false);

  #endif


/* ----------------------- main function -----------------------*/
char* get_item(int item, char* buffer) {
  int pos = 0;
  int cur_entry = 0;

  while (item != cur_entry) {

    while (1) {
      pos++;
      if (pos >= DIRECTORY_BUFFER_SIZE)
        return (char*) &nullstring;
      if (buffer[pos] == 0) {
        pos++;
        break;
      }
    }

    cur_entry++;

  }

  return &buffer[pos];
}


int get_item_cnt(char* buffer) {
  int pos = 0;
  int cur_entry = 0;

  for (int i = 0; i < DIRECTORY_BUFFER_SIZE; i++) {

    while (1) {
      pos++;
      if (buffer[pos] == 0) {
        break;
      }
      if (buffer[pos] == 255)
        return cur_entry;
    }

    cur_entry++;

  }

  return -1;  // no terminator found within boundaries
}

int get_directory(char *path, char* buffer) {
    FRESULT res;
    DIR dir;
    FILINFO fno;

    int buf_pos = 0;
    int item_cnt = 0;

    res = f_opendir(&dir, path);                   /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);           /* Read a directory item */
            if ((fno.fname[0] == 0) || (item_cnt >= MENU_MAX_DIR_ITEMS)) break;          /* Error or end of dir/RAM */
            if (!(fno.fattrib & AM_DIR)) {
                strcpy(&buffer[buf_pos], fno.fname);
                buf_pos += strlen(fno.fname) + 1;
                item_cnt++;
            }
        }
        f_closedir(&dir);
    } else {
        printf("Error reading directory: \"%s\". (%u)\n", path, res);
        return -1;
    }

    buffer[buf_pos] = 255;  // MENU_LST

    return 0;
}

int progress_bar(char* caption, int value, gbuffer_t fb) {
  if ((value < 0) || (value > 100))
    return -1;
  
  gbuffer_t fb_back;

  color_t menu_font_shadow_color = MENU_FONT_SHADOW_COLOR;
  color_t menu_font_caption_color = MENU_FONT_CAPTION_COLOR;

  font_t* menu_font = (font_t*) MENU_FONT;
  int menu_offset_x = MENU_OFFSET_X;
  int menu_width = gbuf_get_width(fb) - 2 * menu_offset_x;
  
  lcd.get_backbuffer(&fb_back);
  blit_buf(0, 0, BLIT_NO_ALPHA, fb, fb_back);
  if (menu_first_run)
    stars_draw(fb_back);

  blit_buf_blend((gbuf_get_width(fb_back) - gbuf_get_width(rect_image_small)) / 2, (gbuf_get_height(fb_back) - gbuf_get_height(rect_image_small)) / 2, 0xf81f, translucency, rect_image_small, fb_back);

  font_write_string_centered(1 + gbuf_get_width(fb_back) / 2 , 1 + gbuf_get_height(fb_back) / 2 - font_get_height(menu_font) - 10, menu_font_shadow_color, caption, menu_font, fb_back);
  font_write_string_centered(gbuf_get_width(fb_back) / 2 , gbuf_get_height(fb_back) / 2 - font_get_height(menu_font) - 10, menu_font_caption_color, caption, menu_font, fb_back);

  int bar_start = menu_offset_x + 10 + gbuf_get_width(bar_left_full_image);
  int bar_end = menu_offset_x + 10 + menu_width - 20 - gbuf_get_width(bar_right_full_image);
    
  for (int i = 0; i < bar_end - bar_start; i++) {
    if (i * 100 / (bar_end - bar_start) > value)
      blit_buf(bar_start + i, gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_mid_empty_image, fb_back);  
    else
      blit_buf(bar_start + i, gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_mid_full_image, fb_back);  
  }

  blit_buf(menu_offset_x + 10 + (menu_width - 20 - gbuf_get_width(bar_right_full_image)), gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_right_empty_image, fb_back);
  blit_buf(menu_offset_x + 10, gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_left_full_image, fb_back);
  blit_buf(menu_offset_x + 10 + gbuf_get_width(bar_right_full_image) + (menu_width - 20 - 2 * gbuf_get_width(bar_right_full_image)) * value / 100, gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_right_full_image, fb_back);

  lcd.request_pageflip();

  return 0;

}

int slider_menu(char* caption, int start_val, int step, int brightness, gbuffer_t fb) {
  gbuffer_t fb_back;

  color_t menu_font_shadow_color = MENU_FONT_SHADOW_COLOR;
  color_t menu_font_caption_color = MENU_FONT_CAPTION_COLOR;

  font_t* menu_font = (font_t*) MENU_FONT;
  int menu_offset_x = MENU_OFFSET_X;
  int menu_width = gbuf_get_width(fb) - 2 * menu_offset_x;
  int bar_height = 10;
  int menu_height = font_get_height(menu_font) + bar_height;

  int b_debounce = 0;
  int b_debounce_timer = 0;
  int b_repeater = 0;

  int value = start_val;
  int first_run = 1;
  int buttons_locked = 1;

  if ((step < 1) || (step > 50))
    step = 10;

  while (1) {
    lcd.get_backbuffer(&fb_back);
    blit_buf(0, 0, BLIT_NO_ALPHA, fb, fb_back);
    if (menu_first_run)
      stars_draw(fb_back);

    ctrl_button_state_update();

    if (first_run && !ctrl_is_pressed(BUTTON_ANY)) {
          buttons_locked = 0;
          first_run = 0;
    }

    if (!buttons_locked) {
      if (ctrl_is_pressed(BUTTON_LEFT) && !b_debounce)
        value -= step;
      
      if (ctrl_is_pressed(BUTTON_RIGHT) && !b_debounce)
        value += step;
  
      if (value < 0)
        value = 0;

      if (value > 100)
        value = 100;

      if (brightness && value > 0)
        lcd_backlight_set(value);

      if ( ctrl_is_pressed(BUTTON_A) ||
           ctrl_is_pressed(BUTTON_B) ||
           ctrl_is_pressed(BUTTON_X) ||
           ctrl_is_pressed(BUTTON_Y) ||
           ctrl_is_pressed(BUTTON_L) ||
           ctrl_is_pressed(BUTTON_R) ||
           ctrl_is_pressed(BUTTON_ST) ||
           ctrl_is_pressed(BUTTON_SEL) ||
           ctrl_is_pressed(BUTTON_SW))
        return value;

      if (!ctrl_is_pressed(BUTTON_ANY)) {
        b_debounce = 0;
        b_repeater = 0;
      } else {
        if (b_debounce == 0) {
          b_debounce = 1;
          b_debounce_timer = millis();
        } else if ((millis() - b_debounce_timer > MENU_REPEAT_START) && !b_repeater) {
          b_repeater = 1;
          b_debounce = 0;
          b_debounce_timer = millis();         
        } else if ((millis() - b_debounce_timer > MENU_REPEAT_DELAY) && b_repeater) {
          b_debounce = 0;
          b_debounce_timer = millis();         
        }
      }
    }

    blit_buf_blend((gbuf_get_width(fb_back) - gbuf_get_width(rect_image_small)) / 2, (gbuf_get_height(fb_back) - gbuf_get_height(rect_image_small)) / 2, 0xf81f, translucency, rect_image_small, fb_back);

    font_write_string_centered(1 + gbuf_get_width(fb_back) / 2 , 1 + gbuf_get_height(fb_back) / 2 - font_get_height(menu_font) - 10, menu_font_shadow_color, caption, menu_font, fb_back);
    font_write_string_centered(gbuf_get_width(fb_back) / 2 , gbuf_get_height(fb_back) / 2 - font_get_height(menu_font) - 10, menu_font_caption_color, caption, menu_font, fb_back);

    //draw_rect_fill(menu_offset_x + 10, gbuf_get_height(fb_back) / 2 - 0, menu_offset_x + 10 + (menu_width - 20) * value / 100, gbuf_get_height(fb_back) / 2 + 10, 0xffff, fb_back);
    int bar_start = menu_offset_x + 10 + gbuf_get_width(bar_left_full_image);
    int bar_end = menu_offset_x + 10 + menu_width - 20 - gbuf_get_width(bar_right_full_image);
    
    for (int i = 0; i < bar_end - bar_start; i++) {
      if (i * 100 / (bar_end - bar_start) > value)
        blit_buf(bar_start + i, gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_mid_empty_image, fb_back);  
      else
        blit_buf(bar_start + i, gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_mid_full_image, fb_back);  
    }

    blit_buf(menu_offset_x + 10 + (menu_width - 20 - gbuf_get_width(bar_right_full_image)), gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_right_empty_image, fb_back);
    blit_buf(menu_offset_x + 10, gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_left_full_image, fb_back);
    blit_buf(menu_offset_x + 10 + gbuf_get_width(bar_right_full_image) + (menu_width - 20 - 2 * gbuf_get_width(bar_right_full_image)) * value / 100, gbuf_get_height(fb_back) / 2 - 0, 0xf81f, bar_right_full_image, fb_back);

    lcd.request_pageflip();
  }

}

int selection_menu(char* item_list, bool centered, gbuffer_t fb) {
  gbuffer_t fb_back, fb_font;

  color_t menu_font_shadow_color = MENU_FONT_SHADOW_COLOR;
  color_t menu_font_selected_color = MENU_FONT_SELECTED_COLOR;
  color_t menu_font_unselected_color = MENU_FONT_UNSELECTED_COLOR;
  color_t menu_font_caption_color = MENU_FONT_CAPTION_COLOR;

  float menu_caption_offset = 1.2;
  int menu_offset_x = (gbuf_get_width(fb) - gbuf_get_width(rect_image_big)) / 2 + MENU_BORDERS_X;
  int menu_offset_y = (gbuf_get_height(fb) - gbuf_get_height(rect_image_big)) / 2 + MENU_BORDERS_Y;
  int menu_width = gbuf_get_width(fb) - 2 * menu_offset_x;
  int menu_height = gbuf_get_height(fb) - 2 * menu_offset_y;
  font_t* menu_font = (font_t*) MENU_FONT;
  int menu_items_clearance_y = 5;
  int max_vis_items = (menu_height - (font_get_height(menu_font) + menu_items_clearance_y) * menu_caption_offset) / (font_get_height(menu_font) + menu_items_clearance_y);
  int num_itms = get_item_cnt(item_list) - 1;  // minus the caption

  while (max_vis_items > num_itms) {
    menu_items_clearance_y++;
    max_vis_items = (menu_height - (font_get_height(menu_font) + menu_items_clearance_y) * menu_caption_offset) / (font_get_height(menu_font) + menu_items_clearance_y);
  }
   
  int b_debounce = 0;
  int b_debounce_timer = 0;
  int b_repeater = 0;

  int cur_itm = 0;
  int ofs_itm = 0;
  if (num_itms < max_vis_items)
    max_vis_items = num_itms;
  int first_run = 1;
  int buttons_locked = 1;

  uint32_t h_font_scrl_timer = millis() + 1000;
  int h_font_scrl_pos = 0;
  int h_font_scrl_dir = 1;
  uint32_t h_font_scrl_delay = 1000;

  if (gbuf_create(&fb_font, menu_width - MENU_MARGIN_VERTICAL_SCROLLBAR, menu_height, (void*) sfe_mem_malloc((menu_width - MENU_MARGIN_VERTICAL_SCROLLBAR) * menu_height * 2)) != BUF_SUCCESS) {
    message("error: no psram"); 
    cur_itm = -1;
    goto leave;
  }
  
  while (1) {
    lcd.get_backbuffer(&fb_back);
    blit_buf(0, 0, BLIT_NO_ALPHA, fb, fb_back);
    if (menu_first_run)
      stars_draw(fb_back);

    blit_buf_blend((gbuf_get_width(fb_back) - gbuf_get_width(rect_image_big)) / 2, (gbuf_get_height(fb_back) - gbuf_get_height(rect_image_big)) / 2, 0xf81f, translucency, rect_image_big, fb_back);
    
    // caption
    font_write_string_centered(1 + menu_offset_x + menu_width / 2, 1 + menu_offset_y, menu_font_shadow_color, get_item(0, item_list), menu_font, fb_back);
    font_write_string_centered(menu_offset_x + menu_width / 2, menu_offset_y, menu_font_caption_color, get_item(0, item_list), menu_font, fb_back);

    draw_rect_fill(0, 0, menu_width - 1, menu_height - 1, 0xf81f, fb_font);

    for (int i = 0; i < max_vis_items; i++) {
      
      // scrolling offset
      if (cur_itm >= ofs_itm + max_vis_items)
        ofs_itm++;

      if (cur_itm < ofs_itm)
        ofs_itm--;

      char* text = get_item(i + 1 + ofs_itm, item_list);
      
      if ((i + ofs_itm) == cur_itm) {
        // horizontal scrolling of long entries (e.g. file names)
        int len = font_get_string_width(text, menu_font);
        if (millis() - h_font_scrl_timer > h_font_scrl_delay) {
          h_font_scrl_delay = 30;
          h_font_scrl_timer = millis();
          if (h_font_scrl_dir == 1) {
            if (len + h_font_scrl_pos > (menu_width - MENU_MARGIN_VERTICAL_SCROLLBAR))
              h_font_scrl_pos--;
            else
              h_font_scrl_dir = -1;
          } else {
            if (h_font_scrl_pos < 0)
              h_font_scrl_pos++;
            else
              h_font_scrl_dir = 1;
          }
        }
        if (centered) {
          font_write_string_centered(menu_width / 2 + h_font_scrl_pos + 1, 1 + (i + menu_caption_offset) * (font_get_height(menu_font) + menu_items_clearance_y), menu_font_shadow_color, text, menu_font, fb_font);
          font_write_string_centered(menu_width / 2 + h_font_scrl_pos + 0, (i + menu_caption_offset) * (font_get_height(menu_font) + menu_items_clearance_y), menu_font_selected_color, text, menu_font, fb_font);
        } else {
          font_write_string(h_font_scrl_pos + 1, 1 + (i + menu_caption_offset) * (font_get_height(menu_font) + menu_items_clearance_y), menu_font_shadow_color, text, menu_font, fb_font);
          font_write_string(h_font_scrl_pos + 0, (i + menu_caption_offset) * (font_get_height(menu_font) + menu_items_clearance_y), menu_font_selected_color, text, menu_font, fb_font);
        }
      } else {
        if (centered) {
          font_write_string_centered(menu_width / 2 + 1, 1 + (i + menu_caption_offset) * (font_get_height(menu_font) + menu_items_clearance_y), menu_font_shadow_color, text, menu_font, fb_font);
          font_write_string_centered(menu_width / 2 + 0, (i + menu_caption_offset) * (font_get_height(menu_font) + menu_items_clearance_y), menu_font_unselected_color, text, menu_font, fb_font);
        } else {
          font_write_string(1, 1 + (i + menu_caption_offset) * (font_get_height(menu_font) + menu_items_clearance_y), menu_font_shadow_color, text, menu_font, fb_font);
          font_write_string(0, (i + menu_caption_offset) * (font_get_height(menu_font) + menu_items_clearance_y), menu_font_unselected_color, text, menu_font, fb_font);
        }
      }

    }

    blit_buf(menu_offset_x, menu_offset_y, 0xf81f, fb_font, fb_back);

    // scroll arrows to indicate additional items
    int scrl_up_arrow_y = menu_offset_y + menu_caption_offset * (font_get_height(menu_font) + menu_items_clearance_y);
    int scrl_down_arrow_y = menu_offset_y + menu_height - 10;

    int total_range_y = scrl_down_arrow_y - scrl_up_arrow_y - gbuf_get_height(bar_up_full_image);
    float scrl_pcnt =  (float) ofs_itm / (float) (num_itms - max_vis_items);

    if (num_itms > max_vis_items) {
      // When there is a great number of items (like in a directory)
      // use a position indicator

      for (int i = 0; i < total_range_y; i++) {
        if ((i * 100) / total_range_y < scrl_pcnt * 100)
          blit_buf(menu_offset_x + menu_width - 10, scrl_up_arrow_y + gbuf_get_height(bar_up_full_image) + i, 0xf81f, bar_mid_h_full_image, fb_back);
        else
          blit_buf(menu_offset_x + menu_width - 10, scrl_up_arrow_y + gbuf_get_height(bar_up_full_image) + i, 0xf81f, bar_mid_h_empty_image, fb_back);
      }

      // arrow up
      //font_put_char(menu_offset_x + menu_width - 10, scrl_up_arrow_y, 0x0, 'A', menu_font, fb_back);
      blit_buf(menu_offset_x + menu_width - 10, scrl_up_arrow_y, 0xf81f, bar_up_full_image, fb_back);
      
      // arrow down
      //font_put_char(menu_offset_x + menu_width - 10, scrl_down_arrow_y, 0x0, 'V', menu_font, fb_back);
      blit_buf(menu_offset_x + menu_width - 10, scrl_down_arrow_y, 0xf81f, bar_down_empty_image, fb_back);

      // position indicator
      //blit_buf(menu_offset_x + menu_width - 10, scrl_up_arrow_y + (font_get_height(menu_font) + menu_items_clearance_y) + total_range_y * scrl_pcnt, 0xf81f, scroll_dot_image, fb_back);
      //font_put_char(menu_offset_x + menu_width - 10, scrl_up_arrow_y + (font_get_height(menu_font) + menu_items_clearance_y) + total_range_y * scrl_pcnt, 0x0, 'O', menu_font, fb_back);
      blit_buf(menu_offset_x + menu_width - 10, scrl_up_arrow_y + gbuf_get_height(bar_up_full_image) + total_range_y * scrl_pcnt, 0xf81f, bar_down_full_image, fb_back);

    } /* else if (num_itms > max_vis_items) {
      // When there is only a small number of items
      // just use arrows to indicate there are more items you can scroll to
      
      // arrow up
      if (ofs_itm > 0)
        font_put_char(menu_offset_x + menu_width - 10, scrl_up_arrow_y, 0x0, 'A', menu_font, fb_back);

      // arrow down
      if (ofs_itm + max_vis_items < num_itms)
        font_put_char(menu_offset_x + menu_width - 10, scrl_down_arrow_y, 0x0, 'V', menu_font, fb_back);

    } */

    ctrl_button_state_update();

    if (first_run && buttons_locked && !ctrl_is_pressed(BUTTON_ANY)) {
      first_run = 0;
      buttons_locked = 0;
    }

    if (!buttons_locked) {
      // return selected item
      if (ctrl_is_pressed(BUTTON_A) ||
          ctrl_is_pressed(BUTTON_B) ||
          ctrl_is_pressed(BUTTON_X) ||
          ctrl_is_pressed(BUTTON_Y) ||
          ctrl_is_pressed(BUTTON_L) ||
          ctrl_is_pressed(BUTTON_R) ||
          ctrl_is_pressed(BUTTON_ST) ||
          ctrl_is_pressed(BUTTON_SEL) ||
          ctrl_is_pressed(BUTTON_SW))
        goto leave;

      if (ctrl_is_pressed(BUTTON_DOWN) && !b_debounce && (cur_itm < (num_itms - 1))) {
        cur_itm++;
        h_font_scrl_timer = millis();
        h_font_scrl_pos = 0;
        h_font_scrl_dir = 1;
        h_font_scrl_delay = 1000;
      }

      if (ctrl_is_pressed(BUTTON_UP) && !b_debounce && (cur_itm > 0)) {
        cur_itm--;
        h_font_scrl_timer = millis();
        h_font_scrl_pos = 0;
        h_font_scrl_dir = 1;
        h_font_scrl_delay = 1000;
      }

      if (!ctrl_is_pressed(BUTTON_ANY)) {
        b_debounce = 0;
        b_repeater = 0;
      } else {
        if (b_debounce == 0) {
          b_debounce = 1;
          b_debounce_timer = millis();
        } else if ((millis() - b_debounce_timer > MENU_REPEAT_START) && !b_repeater) {
          b_repeater = 1;
          b_debounce = 0;
          b_debounce_timer = millis();         
        } else if ((millis() - b_debounce_timer > MENU_REPEAT_DELAY) && b_repeater) {
          b_debounce = 0;
          b_debounce_timer = millis();         
        }
      }
    }

    lcd.request_pageflip();
  }

leave:
  if (gbuf_get_dat_ptr(fb_font))
    sfe_mem_free((void*) gbuf_get_dat_ptr(fb_font));

  return cur_itm;

}

static void call_flash_range_erase(void *param) {
    uint32_t offset = (uint32_t)param;
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
}

int copy_rom_to_flash(gbuffer_t fb) {

  if (!psram_rom)
    return -1;

  long erase_size = Memory.ROM_Size;
  uint32_t rom_flash_address = FLASH_ROM_ADDRESS;
  uint32_t bytes_remaining = Memory.ROM_Size;
  uint32_t bytes_written = 0;
  int percent;
  int percent_old = -1;

  if (erase_size % FLASH_SECTOR_SIZE)
    erase_size = (erase_size / FLASH_SECTOR_SIZE + 1) * FLASH_SECTOR_SIZE;

  printf("trying to erase %d bytes @ %d\n", (int) erase_size, (int) rom_flash_address);

  for (int i = 0; i < erase_size / FLASH_SECTOR_SIZE; i++) {
    //flash_range_erase(rom_flash_address + i * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    int rc = flash_safe_execute(call_flash_range_erase, (void*) (rom_flash_address + i * FLASH_SECTOR_SIZE), UINT32_MAX);
    //hard_assert(rc == PICO_OK);
    if (rc == PICO_OK)
      printf("erased 2k @ %d\n", (int)rom_flash_address + i * FLASH_SECTOR_SIZE);
    else
      printf("failed to erase 2k @ %d reason: %d\n", (int)rom_flash_address + i * FLASH_SECTOR_SIZE, rc);
    //	percent = 100 * (i * FLASH_SECTOR_SIZE) / erase_size;
    percent = 66 * (i * FLASH_SECTOR_SIZE) / erase_size;
    if (percent - percent_old > 5) {
      percent_old = percent;
      progress_bar(STR_LOADING, percent, fb);
    }
  }

  //percent_old = 0;

  while (bytes_remaining > 0) {
    //flash_range_program(rom_flash_address + bytes_written, &Memory.ROM[bytes_written], FLASH_SECTOR_SIZE);

    //percent = 100 - 100 * bytes_remaining / rom_file_size;
    percent = 100 - 33 * bytes_remaining / Memory.ROM_Size;
    if (percent - percent_old > 5) {
      percent_old = percent;
      progress_bar(STR_LOADING, percent, fb);
    }

    bytes_remaining -= FLASH_SECTOR_SIZE;
    bytes_written += FLASH_SECTOR_SIZE;
  }
  
  Memory.ROM = (uint8_t*) rom_flash_address + XIP_BASE;
  Memory.ROM_is_writable = false;

  sfe_mem_free(psram_rom);
  psram_rom = (uint8_t*) NULL;
  flash_flush_cache();

  return 0;

}


int menu_loadfile(gbuffer_t fb) {
  char* buffer;
  int res = 0;
  int ret = 0;
  buffer = (char*) sfe_mem_malloc(DIRECTORY_BUFFER_SIZE + 1000);
  char* filename = &buffer[DIRECTORY_BUFFER_SIZE];

  if (!buffer)
    return -2; // no free psram

  strcpy(buffer, "Select file");

  res = get_directory(ROM_DIRECTORY, &buffer[strlen(buffer) + 1]);
  
  if (res != 0) {
    ret = -3;  // cannot read dir
    goto bailout;
  }

  res = selection_menu((char*) buffer, 0, fb);

  if (res == -1) {
    ret = -1;  // aborted by user
    goto bailout;
  }

  strcpy(filename, ROM_DIRECTORY);
  strcat(filename, get_item(res + 1, buffer));

  printf("filename to load is: %s\n", filename);

  res = load_rom_from_sd(filename, fb);
  if (res != 0) {
    printf("load_rom_from_sd return %d\n", res);
    ret = -4;  // error reading file
    goto bailout;
  }

  xip_cache_clean_all();

  Memory.ROM_is_writable = true;

#if 0
  copy_rom_to_flash(fb);
#endif

  if (!LoadROM(NULL)) {
    ret = -5;  // error loading rom
    goto bailout;
  }
 
  for (int i = 0; i < SRAM_SIZE; i++)
    Memory.SRAM[i] = 0;

 bailout:

  if (buffer)
    sfe_mem_free((void*) buffer);

  return ret;
}

void emumgr_menu(gbuffer_t fb) {
  if (menu_first_run)
    stars_init();

  gbuffer_t fb_saved;
  int res;
  int main_choice, sub_choice;
  char* filename = (char*) sfe_mem_malloc(520);

  if (gbuf_create(&fb_saved, SNES_WIDTH, SNES_HEIGHT, (void*) sfe_mem_malloc(SNES_WIDTH * SNES_HEIGHT * 2)) != BUF_SUCCESS)
    message("error: no psram");

  //save original screen contents
  blit_buf(0, 0, BLIT_NO_ALPHA, fb, fb_saved);

  snd_mute();
  sd_init();

  // TODO: fixme
  emulator_state.show_bat = true;
  emulator_state.show_fps = true;

  while (1) {
    main_choice = selection_menu((char*) main_menu_items, 1, fb_saved);
    switch (main_choice) {
      case 0:
        if (Memory.ROM) {
          S9xReset();
          
          if (LoadROM(NULL))
            goto leave;
          else
            message("error loading rom");
      
        } else {
          message("choose rom first");
        }
        
        break;
      case 1:
        sub_choice = selection_menu((char*) save_menu_items, 1, fb_saved);
        if ((sub_choice >= 0) && (sub_choice <= 5)) {
          strcpy(filename,  SAVESTATE_DIRECTORY);
          strcat(filename, Memory.ROMName);
          char slot_num[3];
          itoa(sub_choice + 1, slot_num, 10);
          strcat(filename, slot_num);
          if (!load_state_handler(filename))
            goto leave;

        }          
        break;
      case 2:
        sub_choice = selection_menu((char*) save_menu_items, 1, fb_saved);
        if ((sub_choice >= 0) && (sub_choice <= 5)) {
          strcpy(filename,  SAVESTATE_DIRECTORY);
          strcat(filename, Memory.ROMName);
          char slot_num[3];
          itoa(sub_choice + 1, slot_num, 10);
          strcat(filename, slot_num);
          
          int a = save_state_handler(filename);
          if (!a)
            printf("successfully saved.\n");
          else
            printf("error saving: %d\n", a);


          goto leave;
        }
          
        break;
      case 3:
        sub_choice = slider_menu((char*) snd_menu, emulator_state.volume * 10, 10, 0, fb_saved);
        if ((sub_choice >= 0) && (sub_choice <= 100))
          emulator_state.volume = sub_choice / 10;
        break;
      case 4:
        sub_choice = slider_menu((char*) bri_menu, emulator_state.brightness, 10, 1, fb_saved);
        if ((sub_choice >= 0) && (sub_choice <= 100))
          if (sub_choice == 0) sub_choice = 1;
          emulator_state.brightness = sub_choice;
        break;
      case 5:
        res = menu_loadfile(fb_saved);
        switch (res) {
          case 0:
            printf("starting emulation\n");
            goto leave;
          case -1:
            printf("aborted by user\n");
            break;
          case -2:
            printf("no psram\n");
            break;
          case -3:
            printf("cannot read dir\n");
            break;
          case -4:
            printf("error reading file\n");
            break;
          case -5:
            printf("error loading rom\n");
            break;
          default:
            break;
        }
        break;
      case 7:
        pwr_shutdown();
        break;  
      default:
        break;
    }
  }

leave:

  sd_deinit();
  if (!emulator_state.muted)
    snd_unmute();

  if (gbuf_get_dat_ptr(fb_saved))
    sfe_mem_free((void*) gbuf_get_dat_ptr(fb_saved));

  if (menu_first_run) {
    menu_first_run = 0;
    translucency = 90;
  }

}
