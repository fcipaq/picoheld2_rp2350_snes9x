/*
4 kb ram free

*/
#include "tf_card.h"
#include "ff.h"

#include <picoengine.h>
#include <fonts/fontfiles/f13x16.h>

//#include <Arduino.h>

//#include <cassert>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "pico/mutex.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/vreg.h"

#include "pico/stdlib.h"  // overclock
#include "hardware/clocks.h"

extern "C" {
#include "sparkfun_pico/sfe_pico.h"
}

#include "hwcfg.h"
#include "controls.h"

extern "C" {
#include "snes9x/snes9x.h"
#include "snes9x/soundux.h"
#include "snes9x/memmap.h"
#include "snes9x/apu.h"
#include "snes9x/display.h"
#include "snes9x/cpuexec.h"
#include "snes9x/srtc.h"
#include "snes9x/save.h"
#include "snes9x/gfx.h"
}

#define LCD_ENABLE
#define AUDIO_ENABLE

#ifdef LCD_ENABLE
LCD_DPI lcd = LCD_DPI();
#endif

int brightness = 64;
/* ---------- LCD END ------------*/

// snes stuff
bool overclock_cycles = false;
int one_c = 4, slow_one_c = 5, two_c = 6;
extern SGFX GFX;  // snes9x API
char* savestate_name;// = "save.sav";  // short file name dos
volatile int target_fps;

// save state
uint32_t just_saved_timer;
int just_saved = 0;
int _sd_init = 0;
mutex_t audio_generate;

// sound
#define NUM_ADUDIO_BUF (3)
#define AUDIO_SAMPLE_RATE (16000)  // 32000 is (of course) slower
#define AUDIO_LOW_PASS_RANGE ((60 * 65536) / 100)
volatile int volume = 1;
int16_t audio_buf[NUM_ADUDIO_BUF][AUDIO_SAMPLE_RATE / 47] __attribute__((aligned(4))); // 4 kb

// --- MAIN
static void update_keymap(int id)
{
}

static bool screenshot_handler(const char *filename, int width, int height)
{
    //return rg_display_save_frame(filename, currentUpdate, width, height);
    return true;
}

static bool save_state_handler(char *filename)
{
  char filename_local[ROM_NAME_LEN + 2];

  filename_local[0] = '/';
  memcpy(&filename_local[1], filename, ROM_NAME_LEN);
  filename_local[ROM_NAME_LEN + 1] = 0;

  bool ret = false;
#if MICROSD
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
  char filename_local[ROM_NAME_LEN + 2];

  filename_local[0] = '/';
  memcpy(&filename_local[1], filename, ROM_NAME_LEN);
  filename_local[ROM_NAME_LEN + 1] = 0;

  printf("Loadstate handler called...\n\n");

#if MICROSD
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

uint32_t timer_str_ld = 0;
int str_ld_debounce = 0;

void check_load_save(void) {
#if 0
  if ( (buts & BUTTON_1) &&
       (buts & BUTTON_2) && 
       (buts & BUTTON_3)) {
    if ((str_ld_debounce == 1) ) {
      if ((millis() - timer_str_ld) > 500) {
        if (dpad & DPAD_UP)
          save_state_handler(savestate_name);
        if (dpad & DPAD_DOWN)
          load_state_handler(savestate_name);
        if (dpad & DPAD_RIGHT) {
          brightness *= 2;
          if (brightness > 1024)
            brightness = 1024;
          set_brightness(brightness - 1);
        }
        if (dpad & DPAD_LEFT) {
          brightness /= 2;
          if (brightness < 16)
            brightness = 16;
          set_brightness(brightness - 1);
        }
        str_ld_debounce = 0;
      }
      // undo all buttons to not interfere with the game
      buts = 0;
      dpad = 0;
    } else {
      timer_str_ld = millis();
      str_ld_debounce = 1;
    }
  } else {
    str_ld_debounce = 0;
  }
#endif
}

static bool reset_handler(bool hard)
{
    S9xReset();
    return true;
}

uint Pitch = SNES_WIDTH * 2;
uint ZPitch = SNES_WIDTH;
uint8_t ZBuffer[SNES_WIDTH * SNES_HEIGHT * 1];
uint8_t SubZBuffer[SNES_WIDTH * SNES_HEIGHT * 1];
#ifndef LCD_ENABLE
uint8_t Screen[SNES_WIDTH * SNES_HEIGHT_EXTENDED * 2];
#endif

bool S9xInitDisplay(void)
{
#ifdef LCD_ENABLE
    gbuffer_t fb_back;
    lcd.get_backbuffer(&fb_back);
    GFX.Screen = (uint8_t*) fb_back.data;
#else
    GFX.Screen = (uint8_t*) Screen;
#endif

    GFX.Pitch = Pitch;
    GFX.ZPitch = ZPitch;

    GFX.SubScreen = GFX.Screen; //(uint8_t*) sfe_mem_malloc(GFX.Pitch * SNES_HEIGHT_EXTENDED);
    GFX.ZBuffer = ZBuffer; //(uint8_t*) sfe_mem_malloc(GFX.ZPitch * SNES_HEIGHT_EXTENDED);

    GFX.SubZBuffer = SubZBuffer; // 56 kB
    //GFX.SubZBuffer = (uint8_t *) sfe_mem_malloc(GFX.ZPitch * SNES_HEIGHT_EXTENDED);  // 56 kB
    
    return GFX.Screen && GFX.SubScreen && GFX.ZBuffer && GFX.SubZBuffer;
}

void S9xDeinitDisplay(void)
{
}

uint32_t S9xReadJoypad(int32_t port)
{
  if (port != 0)
    return 0;

  uint32_t joypad = 0;

  if (ctrl_is_pressed(BUTTON_LEFT)) joypad |= SNES_LEFT_MASK;
  if (ctrl_is_pressed(BUTTON_RIGHT)) joypad |= SNES_RIGHT_MASK;
  if (ctrl_is_pressed(BUTTON_UP)) joypad |= SNES_UP_MASK;
  if (ctrl_is_pressed(BUTTON_DOWN)) joypad |= SNES_DOWN_MASK;
  
  if (ctrl_is_pressed(BUTTON_ST)) joypad |= SNES_START_MASK;
  if (ctrl_is_pressed(BUTTON_SEL)) joypad |= SNES_SELECT_MASK;
  
  if (ctrl_is_pressed(BUTTON_A)) joypad |= SNES_A_MASK;
  if (ctrl_is_pressed(BUTTON_B)) joypad |= SNES_B_MASK;
  if (ctrl_is_pressed(BUTTON_X)) joypad |= SNES_X_MASK;
  if (ctrl_is_pressed(BUTTON_Y)) joypad |= SNES_Y_MASK;

  if (ctrl_is_pressed(BUTTON_R)) joypad |= SNES_TR_MASK;
  if (ctrl_is_pressed(BUTTON_L)) joypad |= SNES_TL_MASK;

  return joypad;
}

bool S9xReadMousePosition(int32_t which1, int32_t *x, int32_t *y, uint32_t *buttons)
{
    return false;
}

bool S9xReadSuperScopePosition(int32_t *x, int32_t *y, uint32_t *buttons)
{
    return false;
}

bool JustifierOffscreen(void)
{
    return true;
}

void JustifierButtons(uint32_t *justifiers)
{
    (void)justifiers;
}

void emu_panic(char* str)
{
  for (int i = 0; i < 10; i++) {
    printf(str);
    sleep_ms(1000);
  }
  pwr_shutdown();

}

void core1task() {
#ifdef AUDIO_ENABLE
  int16_t* audio_buf_9x = (int16_t*) sfe_mem_malloc(AUDIO_SAMPLE_RATE / 47 * 2 * 2); // stereo  // add some margin (50 = 100%, 47 = 106%)  // 2.7 kb
  
  int ret = snd_init();
  if (ret != SND_SUCCESS)
    emu_panic("error init sound\n");
  snd_set_freq(AUDIO_SAMPLE_RATE);  // must never overtake video
  snd_unmute();

  int cur_audio_buf = 0;

  int buffer_length = AUDIO_SAMPLE_RATE / target_fps * 1.02;  // add some margin to prevent crackling

  while (1) {
    while (!mutex_try_enter(&audio_generate, NULL))
      tight_loop_contents();

    S9xMixSamples(audio_buf_9x, buffer_length * 2);

    for (int i = 0; i < buffer_length; i++)
      audio_buf[cur_audio_buf][i] = audio_buf_9x[i * 2] / volume;

    cur_audio_buf++;
    cur_audio_buf %= NUM_ADUDIO_BUF;    

    snd_enqueue_buffer_blocking(audio_buf[cur_audio_buf], buffer_length);

  }
#endif
}

uint32_t millis() {
  return to_ms_since_boot(get_absolute_time());
}

uint32_t micros() {
  return to_us_since_boot(get_absolute_time());
}

int sd_init() {
    int ret = 0;

    FATFS fs;
    FIL fil;
    FRESULT fr;     /* FatFs return code */
    UINT br;
    UINT bw;

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

    if (!spi_configured)
      emu_panic("sd spi failed");
    for (int i = 0; i < 2; i++) {
      fr = f_mount(&fs, "", 1);
      //fr = f_mount(NULL, "", 1);  // unmount
      if (fr == FR_OK)
        break;
      printf("mount failed: %d retrying.\n", (int) fr);
      pwr_sd_sply(0);
      sleep_ms(1000);
      pwr_sd_sply(1);
      sleep_ms(1000);
      pico_fatfs_reboot_spi();
    }

    if (fr != FR_OK)
      return false;

    printf("mount complete\n");

  //fr = f_open(&fil, "bench.dat", FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
  fr = f_open(&fil, "rom.bin", FA_READ);

  if (fr != FR_OK) {
      printf("open file failed: %d\n", (int) fr);
      //emu_panic("open file failed");
      return -1;
  }
  
  printf("open complete\n");

  //fr = f_write(&fil, test_str, 17, &bw);
  #define READ_LEN (2048)
  uint32_t unread_bytes = f_size(&fil);
  uint8_t* buf = (uint8_t*) sfe_mem_malloc(unread_bytes);

  if (!buf) {
    printf("unable to get buffer (%d bytes).\n", unread_bytes);
    ret = -1;
    goto bailout;
  }

  printf("Got buffer (%d bytes).\n", unread_bytes);

  Memory.ROM = buf;
  Memory.ROM_Size = unread_bytes;

  while (unread_bytes) {
    UINT bytes_read;
    fr = f_read (&fil, buf, READ_LEN, &bytes_read);
    buf += bytes_read;
    unread_bytes -= bytes_read;
    if ((fr != FR_OK) && unread_bytes) {
      printf("read error occured\n");
      ret = -2;
      goto bailout;
    }
  }

bailout:

  f_close(&fil);

  return ret;
}

void setup(void) {
  int ret;

  #if 1
  vreg_set_voltage(VREG_VOLTAGE_1_20);
  sleep_ms(10);
  uint32_t sys_clock_khz = 300000;
  set_sys_clock_khz(sys_clock_khz, true);
  #endif
 
  stdio_init_all();

#if 0
  // Initialize chosen UART peripheral (UART0 here)
  uart_init(uart0, 115200);

  // Set the GPIO pins for UART0 TX and RX
  gpio_set_function(PIN_SD_CS_RX /* RX */, GPIO_FUNC_UART);
  gpio_set_function(PIN_SD_MISO_TX /* TX */, GPIO_FUNC_UART);
#endif

  if (!sfe_pico_alloc_init(PIN_PSRAM_CS, 70000000)) {  //70  // Pimoroni can do 90 - why?
    emu_panic("sfe error\n");
  } else
  printf("sfe init complete\n");
  
  printf("online\n");

  pe_config_t pe_cfg = {
    .lcd_num_fb = 2,
    .lcd_refresh_mode = LCD_FRR_REFRESH,
    .lcd_scale = 3,
  };

  if (pe_init(pe_cfg) != PE_SUCCESS)
    emu_panic("picoengine error\n");

  sleep_ms(5);  // 750 us required. TODO: investigate why

#ifdef LCD_ENABLE  
  ret = lcd.init(pe_cfg.lcd_num_fb, pe_cfg.lcd_refresh_mode, pe_cfg.lcd_scale);
  if (ret != LCD_SUCCESS)
    emu_panic("lcd error\n");

  printf("lcd init complete.\n");

  lcd_backlight_init();
  lcd_backlight_set(20);

  printf("trying to get some PSRAM\n");
  uint16_t* test  = (uint16_t*) sfe_mem_malloc(1024);
  if (!test) {
    emu_panic("PSRAM ERROR\n");
  }

  //pwr_shutdown();
#endif

}

int main(void)
{
  setup();

  Settings.CyclesPercentage = 100;
  Settings.H_Max = SNES_CYCLES_PER_SCANLINE;
  Settings.FrameTimePAL = 20000;
  Settings.FrameTimeNTSC = 16667;
  Settings.ControllerOption = SNES_JOYPAD;
  Settings.HBlankStart = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;
  Settings.SoundPlaybackRate = AUDIO_SAMPLE_RATE;
  Settings.DisableSoundEcho = false;
  Settings.InterpolatedSound = true;
#ifdef USE_BLARGG_APU
  Settings.SoundInputRate = AUDIO_SAMPLE_RATE;
#endif

    if (!S9xInitMemory())
        emu_panic("Memory init failed!\n");

    if (!S9xInitDisplay())
        emu_panic("Display init failed!\n");

    if (!S9xInitAPU())
        emu_panic("APU init failed!\n");

    if (!S9xInitSound(0, 0))
        emu_panic("Sound init failed!\n");

    if (!S9xInitGFX())
        emu_panic("Graphics init failed!\n");

//  Memory.ROM = (uint8_t*) ROM_DATA;
//  Memory.ROM_Size = ROM_SIZE;
  Memory.ROM_is_writable = true;

  printf("sd_init\n");
  sd_init();

  if (!LoadROM(NULL))
    emu_panic("ROM loading failed!\n");
 
  target_fps = 50;//Memory.ROMFramesPerSecond;
  int frame_time = 1000000 / target_fps;

  savestate_name = (char*) Memory.ROMName;
  load_state_handler(savestate_name);  // try and load a saved state

#ifdef USE_BLARGG_APU
    S9xSetSamplesAvailableCallback(S9xAudioCallback);
#else
    S9xSetPlaybackRate(Settings.SoundPlaybackRate);
#endif

    // speed control
    uint32_t startTime;

    int32_t framedrop_balance = 0;
    int frame_no = 0;
    int dropped_frames = 0;
    int manual_framedrop = 0;  // between two drawn frames there be skipped manual_framedrop frames
    int manual_framedrop_cnt = 0;
    int dfps = 0;
    unsigned long fps_timer = millis();

    // battery
    uint32_t bat_volt = 0;
    uint32_t bat_volt_cnt = 0;

    gbuffer_t fb_back;

    printf("Starting emulation.\n");

#ifdef AUDIO_ENABLE
  mutex_init(&audio_generate);
  mutex_try_enter(&audio_generate, NULL);
  multicore_reset_core1();
  multicore_launch_core1(core1task);
#endif

//#define PSRAM_TEST
#ifdef PSRAM_TEST
  uint32_t* psram_test_buffer = (uint32_t*) sfe_mem_malloc(1024 * 1024 * 4);
  assert(psram_test_buffer);
  int skippy = 0;
  int tainted = 0;
  int untainted = 0;
#endif

    while (1) {
      //check_load_save();
      
      ctrl_button_state_update();
      if (ctrl_is_pressed(BUTTON_SW))
        pwr_shutdown();

      if (ctrl_is_pressed(BUTTON_L)) {
        manual_framedrop = 1;
        manual_framedrop_cnt = 0;
      }

      if (ctrl_is_pressed(BUTTON_R)) {
        manual_framedrop = 0;
      }

      startTime = micros();

      if (IPPU.RenderThisFrame == false)
        dropped_frames++;

      if (IPPU.RenderThisFrame) {
        lcd.get_backbuffer(&fb_back);
        GFX.Screen = (uint8_t*) fb_back.data;
        GFX.SubScreen = GFX.Screen;
      }

      S9xMainLoop();

#ifdef AUDIO_ENABLE
      mutex_exit(&audio_generate);
#endif

      /* ---------- LCD ------------*/
    if (IPPU.RenderThisFrame) {
      char debug_print[20];
      itoa(target_fps - dfps, debug_print, 10);
      font_write_string(11, 11, 0x0, debug_print, (font_t*) font_13x16, fb_back);
      font_write_string(10, 10, 0xffff, debug_print, (font_t*) font_13x16, fb_back);

      itoa(bat_volt, debug_print, 10);
      font_write_string(11, 11 + 15, 0x0, debug_print, (font_t*) font_13x16, fb_back);
      font_write_string(10, 10 + 15, 0xffff, debug_print, (font_t*) font_13x16, fb_back);

      itoa(pwr_get_bat_level(4 /* granularity */), debug_print, 10);
      font_write_string(11, 11 + 30, 0x0, debug_print, (font_t*) font_13x16, fb_back);
      font_write_string(10, 10 + 30, 0xffff, debug_print, (font_t*) font_13x16, fb_back);

      if (pwr_get_source() == PWR_ON_GRD) {
      font_write_string(11, 11 + 45, 0x0, "GRD", (font_t*) font_13x16, fb_back);
      font_write_string(10, 10 + 45, 0xffff, "GRD", (font_t*) font_13x16, fb_back);
      } else {
      font_write_string(11, 11 + 45, 0x0, "BAT", (font_t*) font_13x16, fb_back);
      font_write_string(10, 10 + 45, 0xffff, "BAT", (font_t*) font_13x16, fb_back);
      }

      if (just_saved) {
        uint16_t* scr = (uint16_t*) GFX.Screen;
        uint16_t col = just_saved == 1 ? 0b0000011111100000 : 0b1111100000000000;
        for (int x = 230; x < 240; x++)
          for (int y = 12; y < 22; y++) {
            scr[x + y * SNES_WIDTH] = col;
          }
        if (millis() - just_saved_timer > 2000)
          just_saved = 0;
      }
    }

#ifdef PSRAM_TEST
    skippy++;
    if (skippy > 200) {

      for (uint32_t i = 0; i < 1024 * 1024; i++)
        psram_test_buffer[i] = i;

      for (uint32_t i = 0; i < 1024 * 1024; i++) {
        if (psram_test_buffer[i] != i) {
          tainted++;
          break;
        }
      }
      skippy = 0;
      untainted++;
      printf("memory corruption (tainted/untainted): %d/%d\n", tainted, untainted);
    }

    char debug_print[20];
    itoa(untainted, debug_print, 10);
    font_write_string(11, 11 + 75, 0x0, debug_print, (font_t*) font_13x16, fb_back);
    font_write_string(10, 10 + 75, 0xffff, debug_print, (font_t*) font_13x16, fb_back);
    itoa(tainted, debug_print, 10);
    font_write_string(11, 11 + 90, 0x0, debug_print, (font_t*) font_13x16, fb_back);
    font_write_string(10, 10 + 90, 0xffff, debug_print, (font_t*) font_13x16, fb_back);
#endif

#ifdef LCD_ENABLE
    if (IPPU.RenderThisFrame == true) {
      lcd.request_pageflip();
    }
#endif
 
    /* ---------- LCD END------------*/
    IPPU.RenderThisFrame = true;
      
    if (manual_framedrop) {
      if (manual_framedrop_cnt == manual_framedrop) {
        manual_framedrop_cnt = 0;
      } else {
        manual_framedrop_cnt++;
        IPPU.RenderThisFrame = false;
      }
    }

    framedrop_balance += (micros() - startTime) - frame_time;

    // too slow?
    if (framedrop_balance > frame_time / 2) // fcipaq: TODO: what amount is best?
      IPPU.RenderThisFrame = false;
  
    // too fast?
    if (framedrop_balance < (-frame_time / 2))   // Allow some jitter
    {
      int32_t fpsLimiter = micros();
      while (micros() - fpsLimiter < (-framedrop_balance))
        tight_loop_contents();
      framedrop_balance = 0;
    }

    bat_volt = pwr_get_bat_vol();

    frame_no++;
    if (millis() - fps_timer > 1000) {
      printf("fps: %d (dropped: %d), (target fps is: %d)\n", frame_no * 1000 / (millis() - fps_timer), dropped_frames, target_fps);
      printf("GPIO 43: %d\n", analog_read(43));
      printf("GPIO 44: %d\n", analog_read(44));
      dfps = dropped_frames;
      dropped_frames = 0;
      frame_no = 0;
      fps_timer = millis();
      printf("Bat vol: %d\n", pwr_get_bat_vol());
      printf("Power src: %d\n", pwr_get_source());
      printf("A: %d\n", ctrl_is_pressed(BUTTON_A));
      printf("B: %d\n", ctrl_is_pressed(BUTTON_B));
      printf("X: %d\n", ctrl_is_pressed(BUTTON_X));
      printf("Y: %d\n", ctrl_is_pressed(BUTTON_Y));
      printf("L: %d\n", ctrl_is_pressed(BUTTON_L));
      printf("R: %d\n", ctrl_is_pressed(BUTTON_R));
      printf("ST: %d\n", ctrl_is_pressed(BUTTON_ST));
      printf("SEL: %d\n", ctrl_is_pressed(BUTTON_SEL));
      printf("ASW: %d\n", ctrl_is_pressed(BUTTON_SW));
      printf("LEFT: %d %d\n", ctrl_is_pressed(BUTTON_LEFT), analog_read(PIN_DPAD_LR));
      printf("RIGHT: %d %d\n", ctrl_is_pressed(BUTTON_RIGHT), analog_read(PIN_DPAD_LR));
      printf("UP: %d %d\n", ctrl_is_pressed(BUTTON_UP), analog_read(PIN_DPAD_UD));
      printf("DOWN: %d %d\n", ctrl_is_pressed(BUTTON_DOWN), analog_read(PIN_DPAD_UD));
      printf("ANA X: %d\n", analog_read(PIN_ANALOG_X));
      printf("ANA Y: %d\n", analog_read(PIN_ANALOG_Y));
    }

  }
}
