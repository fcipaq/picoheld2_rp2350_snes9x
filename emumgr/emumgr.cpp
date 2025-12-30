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
#include <fonts/fontfiles/pixelmix_14_16.h>

#include "hardware/flash.h"
#include "pico/bootrom.h"

#include "emumgr.h"
#include "emumgr_lang_de.h"
//#include "emumgr_lang_en.h"

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

/* ====================================================== variables ====================================================== */
FATFS fs_handle;
bool m_sd_init = false;
/* ====================================================== end variables ====================================================== */

#define MENU_FONT pixelmix_14_16
//#define MENU_FONT font_13x16

#define SAVESTATE_DIRECTORY "/roms/snes/save/"
#define ROM_DIRECTORY "/roms/snes/"

#define RM_SUCCESS 0
#define RM_ERR_NO_DIR -1
#define RM_ERR_NO_ROMS -2
#define RM_ERR_READING_ROM -3
#define RM_ERR_FLASHING_ROM -4

#define SELECTION_DELAY 200
#define SELECTION_DELAY_ROMLIST 10

#define MENU_LST ("")
const char* confirm_menu_items_overwrite[] = { STR_CONFIRM_OVERWRITE,
                                                       STR_YES,
                                                       STR_NO,
                                                       MENU_LST };

const char* confirm_menu_items_bl[] = { STR_CONFIRM_BL,
                                                STR_YES,
                                                STR_NO,
                                                MENU_LST };

const char* main_menu_items[] = { STR_MAIN_TITLE,
															                               STR_LAUNCH,
                                                             STR_LOAD,
                                                             STR_SAVE,
                                                             STR_SEL_ROM,
                                                             STR_SOUND,
                                                             STR_BRIGHTNESS,
                                                             STR_OPTIONS,
                                                             MENU_LST  };

const char* opt_menu_items[] = { STR_OPTIONS_MENU,
                                                           STR_BUTTONS,
                                                           STR_TOGGLE_FPS,
                                                           STR_TOGGLE_Z80,
                                                           STR_LAUNCH_BL,
                                                           //                                                           "WAs",
                                                           STR_GO_BACK,
                                                           MENU_LST };

const char* save_menu_items[] = { STR_SAVE_STATE,
                                                             STR_SLOT_1,
                                                             STR_SLOT_2,
                                                             STR_SLOT_3,
                                                             STR_SLOT_4,
                                                             STR_SLOT_5,
                                                             STR_SLOT_6,
                                                             STR_GO_BACK,
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

const char* bri_menu_items[] = { STR_BRIGHTNESS,
                                                           STR_20,
                                                           STR_40,
                                                           STR_60,
                                                           STR_80,
                                                           STR_100,
                                                           STR_GO_BACK,
                                                           MENU_LST };

const char* snd_menu_items[] = { STR_SOUND,
                                                           STR_OFF,
                                                           STR_25,
                                                           STR_50,
                                                           STR_75,
                                                           STR_100,
                                                           STR_TOGGLE_SND_ACC,
                                                           STR_GO_BACK,
                                                           MENU_LST };

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

int message(char* s) {
  
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

bool load_rom() {
  int ret = 0;
  FIL fil;
  FRESULT fr;

    //fr = f_open(&fil, "bench.dat", FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
  fr = f_open(&fil, "rom.bin", FA_READ);

  if (fr != FR_OK) {
      printf("open file failed: %d\n", (int) fr);
      //message("open file failed");
      return -1;
  }
  
  printf("open complete\n");

  //fr = f_write(&fil, test_str, 17, &bw);
  uint32_t unread_bytes = f_size(&fil);
  uint8_t* buf = (uint8_t*) sfe_mem_malloc(unread_bytes);

  if (!buf) {
    printf("unable to get buffer (%d bytes).\n", unread_bytes);
    ret = -1;
    goto bailout;
  }

  printf("Got buffer (%d bytes).\n", unread_bytes);

  //Memory.ROM = (uint8_t*) FLASH_ROM_ADDRESS;
  //Memory.ROM_Size = FLASH_ROM_ADDRESS;

  Memory.ROM = buf;
  Memory.ROM_Size = unread_bytes;
  Memory.ROM_is_writable = true;

  while (unread_bytes) {
    UINT bytes_read;
    fr = f_read (&fil, buf, 2048 /* read len */, &bytes_read);
    buf += bytes_read;
    unread_bytes -= bytes_read;
    if ((fr != FR_OK) && unread_bytes) {
      printf("read error occured, still %d unread bytes. FR is: %d\n", (int) unread_bytes, fr);
      ret = -2;
      goto bailout;
    }
  }

bailout:

  f_close(&fil);
  
  return ret;
}

static bool screenshot_handler(const char *filename, int width, int height)
{
    //return rg_display_save_frame(filename, currentUpdate, width, height);
    return true;
}

static bool save_state_handler(char *filename)
{
  bool ret = false;

#if 0
  char filename_local[ROM_NAME_LEN + 2];

  filename_local[0] = '/';
  memcpy(&filename_local[1], filename, ROM_NAME_LEN);
  filename_local[ROM_NAME_LEN + 1] = 0;

  File savestate_file;

  printf("Savestate handler called...\n\n");

  if (!_sd_init) {
    if (!SD.begin(PIN_SD_CS, SPI, SD_SPI_SPEED)) {
      printf("Unable to init SD card\n");
      goto bailout;
    }

    _sd_init = 1;
  }

  if (SD.exists(filename)) {
    if (!SD.remove(filename)) {
      printf("Unable to delete game state file\n");
      goto bailout;
    }
  }

  savestate_file = SD.open(filename_local, FILE_WRITE);

  if (!savestate_file) {
    printf("Unable to open save state file for writing.\n\n");
    goto bailout;
  }

  savestate_file.write((uint8_t*) &CPU, sizeof(CPU));
  savestate_file.write((uint8_t*) &ICPU, sizeof(ICPU));
  savestate_file.write((uint8_t*) &PPU, sizeof(PPU));
  savestate_file.write((uint8_t*) &DMA, sizeof(DMA));
  savestate_file.write((uint8_t*) Memory.VRAM, VRAM_SIZE);
  savestate_file.write((uint8_t*) Memory.RAM, RAM_SIZE);
  savestate_file.write((uint8_t*) Memory.SRAM, SRAM_SIZE);
  savestate_file.write((uint8_t*) Memory.FillRAM, FILLRAM_SIZE);
  savestate_file.write((uint8_t*) &APU, sizeof(APU));
  savestate_file.write((uint8_t*) &IAPU, sizeof(IAPU));
  savestate_file.write((uint8_t*) IAPU.RAM, 0x10000);
  savestate_file.write((uint8_t*) &SoundData, sizeof(SoundData));

  savestate_file.close();

  printf("Game state saved.\n\n");

  ret = true;
  
  bailout:
  
  if (ret)
    just_saved = 1;
  else
    just_saved = 2;

  just_saved_timer = millis();
#endif

  return ret;
}

static bool load_state_handler(char *filename)
{
#if 0
  char filename_local[ROM_NAME_LEN + 2];

  filename_local[0] = '/';
  memcpy(&filename_local[1], filename, ROM_NAME_LEN);
  filename_local[ROM_NAME_LEN + 1] = 0;

  printf("Loadstate handler called...\n\n");

  if (!_sd_init) {
    if (!SD.begin(PIN_SD_CS, SPI, 10000000)) {
      printf("Unable to init SD card\n\n");
      just_saved_timer = millis();
      just_saved = 2;
      return false;
    }

    _sd_init = 1;
  }

  File readstate_file = SD.open(filename_local, FILE_READ);

  if (!readstate_file) {
    printf("Failed to load game state:\n");
    printf("*\n");
    printf(filename_local);
    printf("*\n");
    just_saved_timer = millis();
    just_saved = 2;
    return false;
  }

  S9xReset();

  // At this point we can't go back and a failure will corrupt the state anyway
  printf("Now reading to memory.\n");

  readstate_file.read((uint8_t*) &CPU, sizeof(CPU));
  readstate_file.read((uint8_t*) &ICPU, sizeof(ICPU));
  readstate_file.read((uint8_t*) &PPU, sizeof(PPU));
  readstate_file.read((uint8_t*) &DMA, sizeof(DMA));
  readstate_file.read((uint8_t*) Memory.VRAM, VRAM_SIZE);
  readstate_file.read((uint8_t*) Memory.RAM, RAM_SIZE);
  readstate_file.read((uint8_t*) Memory.SRAM, SRAM_SIZE);
  readstate_file.read((uint8_t*) Memory.FillRAM, FILLRAM_SIZE);
  readstate_file.read((uint8_t*) &APU, sizeof(APU));
  readstate_file.read((uint8_t*) &IAPU, sizeof(IAPU));
  readstate_file.read((uint8_t*) IAPU.RAM, 0x10000);
  readstate_file.read((uint8_t*) &SoundData, sizeof(SoundData));
  
  readstate_file.close();

  printf("Game state loaded.\n");

  return S9xLoadState();
#else
  return false;
#endif
}

//================================================================================================================================================================
#if 0

void create_filename(char* s, int num, int ofs) {
  char a;
  for (int h = ofs; h < 32 * 2 + 1; h += 2) {
    a = s[h];
    s[h] = s[h + 1];
    s[h + 1] = a;
  }

  for (char* s2 = s; *s2; ++s2) {
    if (*s2 != ' ')
      *s++ = *s2;
  }
  *s++ = num + 0x30;
  *s = 0;
}

void clearscreen() {
}

#define MENU_BUFFER_HEIGHT 20
#define MENU_BUFFER_WIDTH 320

#define color_bkg RGBColor888_565(0, 10, 25)
#define color_shadow RGBColor888_565(64, 0, 0)
#define color_caption RGBColor888_565(164, 73, 164)
#define color_selected 0xf000
#define color_unselected 0xffff


void printStrings() {
    for (const auto& s : strings) {
        std::cout << s << " ";
    }
    std::cout << std::endl;
}

void message(std::initializer_list<std::string> messages, int delay_ms) {
    for (const auto& s : messages) {
      #define MAX_MSG_LEN (50)
      char buffer[MAX_MSG_LEN + 1];
      int a = s.end() <= MAX_MSG_LEN ? s.end() : MAX_MSG_LEN;
      std::copy(s.begin(), a, buffer);
      buffer[a] = '\0';  // Manual null-terminate
      font_write_string_centered(SCREEN_WIDTH / 2 + 1, 0 + 1, color_shadow, (char*)message[h], (glcdFont_t*) MENU_FONT, scr_buf);
      font_write_string_centered(SCREEN_WIDTH / 2, 0, color_unselected, (char*)message[h], (glcdFont_t*) MENU_FONT, scr_buf);
    }

  delay(50 + delay_ms);

}

int selection_menu(const char** menu, int menu_items, int sel_delay, int cyclic, int preselect, int backkey) {
  int ret = 0;
  int debounce = 1;
  int debounce_fist = 1;
  uint16_t color = 0;
  int8_t selected_item = preselect;
  
  while (1) {

    for (int h = 0; h < menu_items; h++) {
      if (h == selected_item)
        color = color_selected;
      else
        color = color_unselected;

      if (h == 0)
        color = color_caption;

      glcdFillRect(0, 0, MENU_BUFFER_WIDTH, MENU_BUFFER_HEIGHT, color_bkg, scr_buf);

      font_write_string_centered(SCREEN_WIDTH / 2 + 1, 0 + 1, color_shadow, (char*)menu[h], (glcdFont_t*) MENU_FONT, scr_buf);
      font_write_string_centered(SCREEN_WIDTH / 2, 0, color, (char*)menu[h], (glcdFont_t*) MENU_FONT, scr_buf);
    }

    uint16_t dpad = checkDPad();
    uint16_t bts = checkButtons();

    if (debounce == -1 || debounce_fist) {
      while (bts) {
        bts = checkButtons();
      }
      delay(sel_delay);
	  debounce_fist = 0;
    }

    if (debounce > 0)
      debounce++;

    if (debounce > 30)
      debounce = 0;

    if (debounce == 0) {
      if (dpad & DPAD_DOWN) {
        debounce = 1;
        selected_item++;
      }
      if (dpad & DPAD_UP) {
        debounce = 1;
        selected_item--;
      }
      if ((dpad & DPAD_LEFT) && backkey) {
        debounce = 1;
        ret = -3;  // means left
        goto bailout;
      }

      if (selected_item < 1) {
        if (!cyclic) {
          ret = -1;
          goto bailout;
        }

        selected_item = menu_items - 1;
      }

      if (selected_item > menu_items - 1) {
        if (!cyclic) {
          ret = -2;
          goto bailout;
        }

        selected_item = 1;
      }

      if (bts) {
        while (bts) {
          bts = checkButtons();
        }
        delay(10);
        ret = selected_item;

        goto bailout;
      }
    }
  }

bailout:

  free((void*)scr_buf);
  return ret;
}

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

#define BAR_START_Y 110
#define BAR_END_Y BAR_START_Y + 26
#define BAR_START_X 50
#define BAR_END_X SCREEN_WIDTH - BAR_START_X
#define BAR_BORDER_COLOR color_unselected
#define BAR_COLOR RGBColor888_565(0, 255, 0)
#define BAR_CAPTION_Y 50

void draw_bar(int percent, char* title) {
  if (percent < 0)
    percent = 0;

  if (percent > 100)
    percent = 10;

  int fill = (SCREEN_WIDTH - 2 * BAR_START_X - 8) * percent / 100;

  for (int i = 0; i < SCREEN_HEIGHT; i++) {
    for (int h = 0; h < SCREEN_WIDTH; h++)
      scr_buf[BUF_HEADER_SIZE + h] = color_bkg;

    if (i >= BAR_CAPTION_Y && i < BAR_CAPTION_Y + getFontHeight((glcdFont_t*) MENU_FONT)) {
      font_write_string_centered(SCREEN_WIDTH / 2 + 1, BAR_CAPTION_Y - i + 1, color_shadow, title, (glcdFont_t*) MENU_FONT, scr_buf);
      font_write_string_centered(SCREEN_WIDTH / 2, BAR_CAPTION_Y - i, color_unselected, title, (glcdFont_t*) MENU_FONT, scr_buf);
    }

    if (i >= BAR_START_Y && i < BAR_START_Y + 2)
      for (int h = BAR_START_X; h < BAR_END_X; h++)
        scr_buf[BUF_HEADER_SIZE + h] = BAR_BORDER_COLOR;

    if (i >= BAR_START_Y + 4 && i <= BAR_END_Y - 4)
      for (int h = BAR_START_X + 2; h < BAR_START_X + fill; h++)
        scr_buf[BUF_HEADER_SIZE + h] = BAR_COLOR;

    if (i >= BAR_END_Y && i < BAR_END_Y + 2)
      for (int h = BAR_START_X; h < BAR_END_X; h++)
        scr_buf[BUF_HEADER_SIZE + h] = BAR_BORDER_COLOR;

    if (i >= BAR_START_Y && i < BAR_END_Y + 2) {
      for (int h = BAR_START_X - 2; h < BAR_START_X; h++)
        scr_buf[BUF_HEADER_SIZE + h] = BAR_BORDER_COLOR;

      for (int h = BAR_END_X; h < BAR_END_X + 2; h++)
        scr_buf[BUF_HEADER_SIZE + h] = BAR_BORDER_COLOR;
    }

  }

}

int flash_rom(FsFile &rom_file, uint8_t* rom_addr) {
  char rom_file_str_tmp[256];
  rom_file.getName(rom_file_str_tmp, 255);
  String rom_file_str = rom_file_str_tmp;

  long rom_file_size = rom_file.size();
  long bytes_remaining = rom_file_size;
  long bytes_written = 0;

  uint32_t rom_addr_int = ((uint32_t)&rom_addr[0]) - XIP_BASE;

  //  if (PICO_FLASH_SIZE_BYTES - (__flash_binary_end - XIP_BASE) < rom_file_size)
  if (PICO_FLASH_SIZE_BYTES - rom_addr_int < rom_file_size) {
    message2(STR_ERROR, STR_ROM_TOO_BIG, 2, 2000);
    return false;
  }

  int ret = true;
  uint8_t percent = 0;
  uint8_t percent_old = 0;

  long erase_size = rom_file_size;

  if (erase_size % FLASH_SECTOR_SIZE)
    erase_size = (erase_size / FLASH_SECTOR_SIZE + 1) * FLASH_SECTOR_SIZE;

  for (int i = 0; i < erase_size / FLASH_SECTOR_SIZE; i++) {
    flash_range_erase(rom_addr_int + i * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    //	percent = 100 * (i * FLASH_SECTOR_SIZE) / erase_size;
    percent = 66 * (i * FLASH_SECTOR_SIZE) / erase_size;
    if (percent - percent_old > 5) {
      percent_old = percent;
      draw_bar(percent, STR_LOADING);
    }
  }

  //percent_old = 0;

  while (bytes_remaining > 0) {
    //memset(M68K_RAM, 0, FLASH_SECTOR_SIZE);

    int read_len = rom_file.readBytes((char*)M68K_RAM, bytes_remaining < FLASH_SECTOR_SIZE ? bytes_remaining : FLASH_SECTOR_SIZE);

    // swap hi/lo byte --> emu performance improvement due to different endianess
    for (int i = 0; i < FLASH_SECTOR_SIZE; i += 2) {
      uint8_t tmp = M68K_RAM[i];
      M68K_RAM[i] = M68K_RAM[i + 1];
      M68K_RAM[i + 1] = tmp;
    }

    // erasing flash here does not work...
    //flash_range_erase(rom_addr_int + bytes_written, FLASH_SECTOR_SIZE);
    flash_range_program(rom_addr_int + bytes_written, M68K_RAM, FLASH_SECTOR_SIZE);

    //percent = 100 - 100 * bytes_remaining / rom_file_size;
    percent = 100 - 33 * bytes_remaining / rom_file_size;
    if (percent - percent_old > 5) {
      percent_old = percent;
      draw_bar(percent, STR_LOADING);
    }

    bytes_remaining -= FLASH_SECTOR_SIZE;
    bytes_written += FLASH_SECTOR_SIZE;
  }

  rom_file.close();

#if WRITE_BACK_FILE_TO_SD
  rom_file = SD.open("/cmp.bin", FILE_WRITE);
  bytes_remaining = rom_file_size;
  bytes_written = 0;
  while (bytes_remaining > 0) {
    rom_file.write(&ROM_DATA[bytes_written], FLASH_SECTOR_SIZE < bytes_remaining ? FLASH_SECTOR_SIZE : bytes_remaining);
    bytes_remaining -= FLASH_SECTOR_SIZE;
    bytes_written += FLASH_SECTOR_SIZE;
  }
  rom_file.close();
#endif

  // reset machine
  // let's just hope noone messed with the WD settings...
  watchdog_reboot(0, 0, 1);
  watchdog_enable(1, false);

  return ret;
}


int load_rom() {
  int ret = RM_SUCCESS;
  long ofs = 0;
  int h = 1;
  int eod = 0;  // end of dir
  int preselect = 1;
  FsFile rom_file;
  int sel = 0;

  char* rom_file_list[NUM_ROM_FILE_LIST];

  rom_file_list[0] = (char*)rom_file_list_header;

  for (int i = 1; i < NUM_ROM_FILE_LIST; i++) {
    rom_file_list[i] = (char*) malloc(ROM_FILE_LIST_NAME_LENGTH + 1);
    assert(rom_file_list[i]);
  }

  FsFile rom_dir = SD.open(ROM_DIRECTORY);

  if (!rom_dir.isDirectory()) {
    rom_dir.close();
    message2(STR_ERROR, STR_READ_ROM_DIR, 2, 2000);
    ret = RM_ERR_NO_DIR;
    goto bailout;
  }

  message2(STR_PRESS_LEFT, STR_RETURN, 2, 1000);

  while (sel <= 0) {

    if (sel != -2) {
      if (sel == -1) {
        rom_dir.rewindDirectory();

        ofs--;
        if (ofs < 0)
          ofs = 0;

        for (h = 0; h < ofs; h++) {
          while (1) {
            rom_file = rom_dir.openNextFile();
            if (!rom_file)
              goto allofsfilesread;
            if (!rom_file.isDirectory())
              break;
          }
        }
      }

allofsfilesread:

      for (h = 1; h < NUM_ROM_FILE_LIST; h++) {

        do {
          rom_file = rom_dir.openNextFile();
        } while (rom_file && rom_file.isDirectory());

        if (!rom_file) {
          eod = 1;
          break;
        } else {
          eod = 0;
        }

        char rom_file_str_tmp[256];
        rom_file.getName(rom_file_str_tmp, 255);

        String rom_file_str = rom_file_str_tmp;

        rom_file_str = rom_file_str.substring(0, ROM_FILE_LIST_NAME_LENGTH);
        rom_file_str.toCharArray(rom_file_list[h], ROM_FILE_LIST_NAME_LENGTH);
      }
    } else {
      do {
        rom_file = rom_dir.openNextFile();
      } while (rom_file && rom_file.isDirectory());

      if (!rom_file) {
        eod = 1;
        goto no_more_roms;
      } else {
        eod = 0;
        ofs++;
      }

      for (h = 1; h < NUM_ROM_FILE_LIST - 1; h++)
        for (int i = 0; i < ROM_FILE_LIST_NAME_LENGTH + 1; i++)
          rom_file_list[h][i] = rom_file_list[h + 1][i];

      h++;  // for selection_menu

      char rom_file_str_tmp[256];
      rom_file.getName(rom_file_str_tmp, 255);

      String rom_file_str = rom_file_str_tmp;

      rom_file_str = rom_file_str.substring(0, ROM_FILE_LIST_NAME_LENGTH);
      rom_file_str.toCharArray(rom_file_list[NUM_ROM_FILE_LIST - 1], ROM_FILE_LIST_NAME_LENGTH);
    }

    if (h == 1 && eod == 1 && ofs == 0) {
      message2(STR_NO_ROMS, ROM_DIRECTORY, 2, 2000);
      ret = RM_ERR_NO_ROMS;
      goto bailout;
    }

no_more_roms:

    if (sel == -1)
      preselect = 1;

    if (sel == -2)
      preselect = h - 1;

    sel = selection_menu((const char**)rom_file_list, h, SELECTION_DELAY_ROMLIST, 0, preselect, 1);

    if (sel == -3)
      goto bailout;
  }

  message2(STR_FLASHING_ROM, STR_PLEASE_WAIT, 2, 0);

  rom_dir.rewindDirectory();

  for (int i = 0; i < ofs + sel; i++) {
    do {
      rom_file = rom_dir.openNextFile();
    } while (rom_file && rom_file.isDirectory());
    if (!rom_file) {
      message2(STR_ERROR, STR_READING_ROM, 2, 2000);
      ret = RM_ERR_READING_ROM;
      goto bailout;
    }
  }

  if (!flash_rom(rom_file, (uint8_t*)ROM_DATA)) {
    ret = RM_ERR_FLASHING_ROM;
    message2(STR_ERROR, STR_FLASHING_ROM, 2, 2000);
  }

bailout:

  for (int i = 1; i < NUM_ROM_FILE_LIST; i++)
    free((void*)rom_file_list[i]);

  return ret;
}
bool init_sd_card() {
  SPI.setRX(PIN_SD_MISO);
  SPI.setTX(PIN_SD_MOSI);
  SPI.setSCK(PIN_SD_SCK);

  if (!SD.begin(SdSpiConfig(PIN_SD_CS, SHARED_SPI, SD_SCK_MHZ(1), &SPI))) {
    
    char debug_print[20];
    int a =  (SD.card()->errorCode());
        itoa(a, debug_print, 10);

		
	message2(debug_print, "SD init error :(", 2, 2000);
    return false;
  } else
    return true;
}

bool savestate(int num) {
  char filename[100] = SAVESTATE_DIRECTORY;
  int a = strlen(filename);

  // concatenate strings without library
  memcpy((void*)(filename + a), (void*)((unsigned long)&ROM_DATA[0] + 0x150), 32);
  filename[a + 31] = '/';
  memcpy((void*)(filename + a + 32 + 0), (void*)((unsigned long)&ROM_DATA[0] + 0x150), 32);
  filename[a + 32 * 2] = 0;

  create_filename(filename, num, a);

  if (SD.exists(filename)) {

    uint8_t sel = selection_menu(confirm_menu_items_overwrite, NUM_CONFIRM_MENU_ITEMS_OVERWRITE, SELECTION_DELAY, 1, 2, 0);

    if (sel != 1)
      return false;

    SD.remove(filename);
  }

  message(message_save, MSG_SAVE_NUM_ROWS, 0);

  SD.mkdir(SAVESTATE_DIRECTORY);

  savestate_file = SD.open(filename, FILE_WRITE);

  if (!savestate_file) {
    message(message_error, MSG_ERROR_NUM_ROWS, 2000);
    return false;
  }

  gwenesis_save_state();

  savestate_file.close();

  return true;
}

bool loadstate(int num) {
  char filename[100] = SAVESTATE_DIRECTORY;
  int a = strlen(filename);

  // concatenate strings without library
  memcpy((void*)(filename + a), (void*)((unsigned long)&ROM_DATA[0] + 0x150), 32);
  filename[a + 31] = '/';
  memcpy((void*)(filename + a + 32 + 0), (void*)((unsigned long)&ROM_DATA[0] + 0x150), 32);
  filename[a + 32 * 2] = 0;

  create_filename(filename, num, a);

  savestate_file = SD.open(filename, FILE_READ);

  if (!savestate_file) {
    message(message_error, MSG_ERROR_NUM_ROWS, 2000);
    return false;
  }

  gwenesis_load_state();

  savestate_file.close();

  return true;
}

#endif


/* ----------------------- main function -----------------------*/

void emumgr_menu(gbuffer_t fb) {
  sd_init();

  load_rom();

  if (!LoadROM(NULL))
    message("ROM loading failed!\n");
 
}
