/*
4 kb ram free

*/

#include <picoengine.h>
#include <fonts/fontfiles/f13x16.h>
#include <fonts/fontfiles/picoheld_pixel_5x7_rosa.h>

//#include <Arduino.h>

//#include <cassert>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "pico/mutex.h"
#include "pico/multicore.h"
#include "pico/flash.h"
//#include "pico/time.h"
#include "hardware/vreg.h"

#include "pico/stdlib.h"  // overclock
#include "hardware/clocks.h"

extern "C" {
#include "sparkfun_pico/sfe_pico.h"
}

#include "hwcfg.h"
#include "controls.h"

#include "emumgr.h"

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

#include <malloc.h>
//#define HEADER_ROM
#ifdef HEADER_ROM
#include "../rom_lion_king.h"
#endif

#define LCD_ENABLE
#define AUDIO_ENABLE

#ifdef LCD_ENABLE
LCD_DPI lcd = LCD_DPI();
#endif

gbuffer_t fb_back;
extern emulator_state_t emulator_state;
/* ---------- LCD END ------------*/

// snes stuff
bool overclock_cycles = false;
int one_c = 4, slow_one_c = 5, two_c = 6;
extern SGFX GFX;  // snes9x API

volatile int target_fps;
mutex_t audio_generate;

// sound
#define NUM_ADUDIO_BUF (3)
#define AUDIO_SAMPLE_RATE (16000)  // 32000 is (of course) slower
#define AUDIO_LOW_PASS_RANGE ((60 * 65536) / 100)
#ifdef AUDIO_ENABLE
int16_t audio_buf[NUM_ADUDIO_BUF][AUDIO_SAMPLE_RATE / 47] __attribute__((aligned(4))); // 4 kb
#endif

// --- MAIN
static void update_keymap(int id)
{
}

uint32_t timer_str_ld = 0;
int str_ld_debounce = 0;

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
  flash_safe_execute_core_init();

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

    if (emulator_state.volume) {
      if (emulator_state.muted) {
        emulator_state.muted = 0;
        snd_unmute();
      }

      S9xMixSamples(audio_buf_9x, buffer_length * 2);

      for (int i = 0; i < buffer_length; i++)
        audio_buf[cur_audio_buf][i] = audio_buf_9x[i * 2] / (6 - emulator_state.volume / 2);

      cur_audio_buf++;
      cur_audio_buf %= NUM_ADUDIO_BUF;    

      snd_enqueue_buffer_blocking(audio_buf[cur_audio_buf], buffer_length);
    } else {
      if (!emulator_state.muted) {
        emulator_state.muted = 1;
        snd_mute();
      }
    }

  }
#endif
}

uint32_t millis() {
  return to_ms_since_boot(get_absolute_time());
}

uint32_t micros() {
  return to_us_since_boot(get_absolute_time());
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

  if (!sfe_pico_alloc_init(PIN_PSRAM_CS, 90000000)) {  //70  // Pimoroni can do 90 - why?
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

  // power up SD card
  pwr_sd_sply(1); 

#ifdef LCD_ENABLE  
  ret = lcd.init(pe_cfg.lcd_num_fb, pe_cfg.lcd_refresh_mode, pe_cfg.lcd_scale);
  if (ret != LCD_SUCCESS)
    emu_panic("lcd error\n");

  printf("lcd init complete.\n");

  emulator_state.volume = 2;
  emulator_state.brightness = 20;

  lcd_backlight_init();
  lcd_backlight_set(0);

  printf("trying to get some PSRAM\n");
  uint16_t* test  = (uint16_t*) sfe_mem_malloc(1024);
  if (!test) {
    emu_panic("PSRAM ERROR\n");
  }
#endif

}

void print_memory_usage() {
    struct mallinfo info = mallinfo();

    printf("Total allocated: %d bytes\n", info.uordblks);
    printf("Total free: %d bytes\n", info.fordblks);
    printf("Total heap size: %d bytes\n", info.arena);
    printf("Largest free block: %d bytes\n", info.ordblks);
}

void bl_fade_in() {
#define BL_FADE_STEPS (50)
  float delta = (float) emulator_state.brightness / (float) BL_FADE_STEPS;
  float value = 0;

  for (int i = 0; i < BL_FADE_STEPS; i++) {
    lcd_backlight_set((int) value);
    value += delta;
    sleep_ms(5);
  }

  lcd_backlight_set((int) emulator_state.brightness);

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
  Memory.ROM = NULL;
    struct mallinfo info = mallinfo();

    printf("Total allocated: %d bytes\n", info.uordblks);
    printf("Total free: %d bytes\n", info.fordblks);
    printf("Total heap size: %d bytes\n", info.arena);
    printf("Largest free block: %d bytes\n", info.ordblks);

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

#ifdef HEADER_ROM
  Memory.ROM = (uint8_t*) ROM_DATA;
  Memory.ROM_Size = ROM_SIZE;
#endif

  bl_fade_in();

  emumgr_menu(fb_back);

  target_fps = 50;//Memory.ROMFramesPerSecond;
  int frame_time = 1000000 / target_fps;

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
#ifdef ENABLE_PSRAM_FB
    gbuffer_t fb_back_psram;
#endif

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
      ctrl_button_state_update();

      if (emulator_state.manual_framedrop) {
        if (!manual_framedrop) {
          manual_framedrop = 1;
          manual_framedrop_cnt = 0;
        }
      } else {
        manual_framedrop = 0;
      }

      startTime = micros();

      if (IPPU.RenderThisFrame == false)
        dropped_frames++;

      if (IPPU.RenderThisFrame) {
        lcd.get_backbuffer(&fb_back);
#ifdef ENABLE_PSRAM_FB
        lcd.get_psram_backbuffer(&fb_back_psram);
#endif
        GFX.Screen = (uint8_t*) fb_back.data;
        GFX.SubScreen = GFX.Screen;
      }

      S9xMainLoop();

      if (ctrl_is_pressed(BUTTON_SW)) {
        emumgr_menu(fb_back);
        IPPU.RenderThisFrame == false;
        framedrop_balance = 0;
        fps_timer = millis();
        startTime = micros();
      }      

#ifdef AUDIO_ENABLE
      mutex_exit(&audio_generate);
#endif

      /* ---------- LCD ------------*/
    if (IPPU.RenderThisFrame) {

      if (emulator_state.show_fps) {
        char debug_print[20];
        itoa(target_fps - dfps, debug_print, 10);
        font_write_string(14, 6, 0x0, debug_print, (font_t*) font_13x16, fb_back);
        int r = (511 * dfps) / target_fps;
        int g = 511 - r;
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        font_write_string(13, 5, rgb_col_888_565(r, g, 0), debug_print, (font_t*) font_13x16, fb_back);
      }

#ifdef ENABLE_PSRAM_FB
      draw_battery(219, 1, pwr_get_bat_level(4), fb_back_psram);
#else
      draw_battery(219, 5, pwr_get_bat_level(4), fb_back);
#endif

      #if 0 // bat info
      char debug_print[10];
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
      #endif
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

      printf("Bat vol: %d\n", pwr_get_bat_vol());
    #if 0
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
      printf("fps: %d (dropped: %d), (target fps is: %d)\n", frame_no * 1000 / (millis() - fps_timer), dropped_frames, target_fps);
      printf("GPIO 43: %d\n", analog_read(43));
      printf("GPIO 44: %d\n", analog_read(44));
    #endif
      dfps = dropped_frames;
      dropped_frames = 0;
      frame_no = 0;
      fps_timer = millis();
    }

  }
}
