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

#include "hardware/adc.h"
#include "pico/time.h"

#include "controls.h"
#include <hwcfg.h>
#include <cstdio>
#define RP2350_E9_WA

uint16_t _analog_x_0 = 400;
uint16_t _analog_y_0 = 400;
uint16_t _analog_max_x = 600;
uint16_t _analog_min_x = 200;
uint16_t _analog_max_y = 600;
uint16_t _analog_min_y = 200;

int _ctrl_lst_ret_x;
int _ctrl_lst_ret_y;

// Values below this threshold are considered as zero
#define zeroThreshold 25   // 5^2
// Decrement the max. amplitude by this value, so that 100% can be reached
#define minmaxTolerance 10

#define BTN_NO_THRESHOLD (-1)
#define BTN_NO_PULL (-1)
#define BTN_PULLUP (1)
#define BTN_PULLDOWN (0)
#define BTN_TIMER_ZERO (0)
#define BTN_DIGITAL (1)
#define BTN_ANALOG (0)
#define BTN_PRESSED (1)
#define BTN_RELEASED (0)
#define BTN_PRESSED_ON_HI (true)
#define BTN_PRESSED_ON_LO (false)

/* TODO: MOVE ME TO HWCFG FOR BETTER HARDWARE ABSTRACTION */
keymap_t keymap[NUM_BTNS] = {
    { BUTTON_A,     PIN_BTN_A,   BTN_DIGITAL, BTN_RELEASED, BUTTON_PULL_MODE, BTN_NO_THRESHOLD, BTN_NO_THRESHOLD,     BTN_PRESSED_ON_LO, BTN_TIMER_ZERO}, 
    { BUTTON_B,     PIN_BTN_B,   BTN_DIGITAL, BTN_RELEASED, BUTTON_PULL_MODE, BTN_NO_THRESHOLD, BTN_NO_THRESHOLD,     BTN_PRESSED_ON_LO, BTN_TIMER_ZERO}, 
    { BUTTON_X,     PIN_BTN_X,   BTN_DIGITAL, BTN_RELEASED, BUTTON_PULL_MODE, BTN_NO_THRESHOLD, BTN_NO_THRESHOLD,     BTN_PRESSED_ON_LO, BTN_TIMER_ZERO},       
    { BUTTON_Y,     PIN_BTN_Y,   BTN_DIGITAL, BTN_RELEASED, BUTTON_PULL_MODE, BTN_NO_THRESHOLD, BTN_NO_THRESHOLD,     BTN_PRESSED_ON_LO, BTN_TIMER_ZERO}, 
    { BUTTON_ST,    PIN_BTN_ST,  BTN_DIGITAL, BTN_RELEASED,     BTN_PULLDOWN, BTN_NO_THRESHOLD, BTN_NO_THRESHOLD,     BTN_PRESSED_ON_HI, BTN_TIMER_ZERO}, 
    { BUTTON_SEL,   PIN_BTN_SEL, BTN_DIGITAL, BTN_RELEASED, BUTTON_PULL_MODE, BTN_NO_THRESHOLD, BTN_NO_THRESHOLD,     BTN_PRESSED_ON_LO, BTN_TIMER_ZERO}, 
//    { BUTTON_L,     PIN_BTN_L,   BTN_DIGITAL, BTN_RELEASED, BUTTON_PULL_MODE, BTN_NO_THRESHOLD, BTN_NO_THRESHOLD,     BTN_PRESSED_ON_LO, BTN_TIMER_ZERO}, 
    { BUTTON_L,     PIN_BTN_L,   BTN_ANALOG,  BTN_RELEASED,       BTN_PULLUP,                0,              500,            -1, BTN_TIMER_ZERO}, 
    { BUTTON_R,     PIN_BTN_R,   BTN_DIGITAL, BTN_RELEASED, BUTTON_PULL_MODE, BTN_NO_THRESHOLD, BTN_NO_THRESHOLD,     BTN_PRESSED_ON_LO, BTN_TIMER_ZERO}, 
//    { BUTTON_SW,    PIN_BTN_SW,  BTN_DIGITAL, BTN_RELEASED, BUTTON_PULL_MODE, BTN_NO_THRESHOLD, BTN_NO_THRESHOLD,     BTN_PRESSED_ON_LO, BTN_TIMER_ZERO}, 
    { BUTTON_SW,    PIN_BTN_SW,  BTN_ANALOG,   BTN_RELEASED,      BTN_PULLUP,                0,              500,            -1, BTN_TIMER_ZERO}, 

    { BUTTON_LEFT,  PIN_DPAD_LR,  BTN_ANALOG, BTN_RELEASED,      BTN_NO_PULL,             2250,              4095,           -1, BTN_TIMER_ZERO}, 
    { BUTTON_RIGHT, PIN_DPAD_LR,  BTN_ANALOG, BTN_RELEASED,      BTN_NO_PULL,                0,              1450,           -1, BTN_TIMER_ZERO}, 
    { BUTTON_UP,    PIN_DPAD_UD,  BTN_ANALOG, BTN_RELEASED,      BTN_NO_PULL,             2250,              4095,           -1, BTN_TIMER_ZERO}, 
    { BUTTON_DOWN,  PIN_DPAD_UD,  BTN_ANALOG, BTN_RELEASED,      BTN_NO_PULL,                0,              1450,           -1, BTN_TIMER_ZERO}, 
/*
    { BUTTON_LEFT,  PIN_ANALOG_Y, BTN_ANALOG, BTN_RELEASED,      BTN_NO_PULL,               0,               1000,           -1, BTN_TIMER_ZERO}, 
    { BUTTON_RIGHT, PIN_ANALOG_Y, BTN_ANALOG, BTN_RELEASED,      BTN_NO_PULL,             3000,              4095,           -1, BTN_TIMER_ZERO}, 
    { BUTTON_UP,    PIN_ANALOG_X, BTN_ANALOG, BTN_RELEASED,      BTN_NO_PULL,             3000,              4095,           -1, BTN_TIMER_ZERO}, 
    { BUTTON_DOWN,  PIN_ANALOG_X, BTN_ANALOG, BTN_RELEASED,      BTN_NO_PULL,                0,              1000,           -1, BTN_TIMER_ZERO}, 
*/
};

int analog_read(int gpio_num) {
  adc_select_input(gpio_num - ADC_PIN_OFFSET);
  int read_val = adc_read();

  //printf("GPIO %d is at %d\n", gpio_num, read_val);
  return (read_val); // 10 bit res
}

void ctrl_recalib_analog() {
  _analog_x_0 = analog_read(PIN_ANALOG_X);
  _analog_y_0 = analog_read(PIN_ANALOG_Y);
  _analog_max_x = _analog_x_0 + 200;
  _analog_min_x = _analog_x_0 - 200;
  _analog_max_y = _analog_y_0 + 200;
  _analog_min_y = _analog_y_0 - 200;
}

/*
int8_t ctrl_analog_x_state() {
  int8_t ret;
  uint16_t cur_x = analog_read(PIN_ANALOG_X);

  if (cur_x > _analog_max_x)
    _analog_max_x = cur_x - minmaxTolerance;

  if (cur_x < _analog_min_x)
    _analog_min_x = cur_x + minmaxTolerance;

  if (cur_x > _analog_x_0)    
    ret = (cur_x - _analog_x_0) * 100 / (_analog_max_x - _analog_x_0);
  else
    ret = (cur_x - _analog_x_0) * 100 / (_analog_x_0 - _analog_min_x);

   // if |ret| < zeroThreshold^2 return zero
  if (ret * ret < zeroThreshold)
   ret = 0;
  
  // if the new value differs less than 10% from the old one it's being discarded
  if ((ret - _ctrl_lst_ret_x) * (ret - _ctrl_lst_ret_x) > zeroThreshold*zeroThreshold) {
    _ctrl_lst_ret_x = ret;
  } else {
    ret = _ctrl_lst_ret_x;
  }

  if (ret < -100)
    ret = -100;
  if (ret > 100)
    ret = 100;

  return (ret / 10) * -10;
}

int8_t ctrl_analog_y_state() {
  int8_t ret;
  uint16_t cur_y = analog_read(PIN_ANALOG_Y);

  if (cur_y > _analog_max_y)
    _analog_max_y = cur_y - minmaxTolerance;

  if (cur_y < _analog_min_y)
    _analog_min_y = cur_y + minmaxTolerance;

  if (cur_y > _analog_y_0)    
    ret = (cur_y - _analog_y_0) * 100 / (_analog_max_y - _analog_y_0);
  else
    ret = (cur_y - _analog_y_0) * 100 / (_analog_y_0 - _analog_min_y);

  // if |ret| < zeroThreshold^2 return zero
  if (ret * ret < zeroThreshold)
   ret = 0;

  // if the new value differs less than 10% from the old one it's being discarded
  if ((ret - _ctrl_lst_ret_y) * (ret - _ctrl_lst_ret_y) > zeroThreshold*zeroThreshold) {
    _ctrl_lst_ret_y = ret;
  } else {
    ret = _ctrl_lst_ret_y;
  }

  if (ret < -100)
    ret = -100;
  if (ret > 100)
    ret = 100;

  return (ret / 10) * 10;
}

uint16_t ctrl_dpad_state() {
  uint16_t cur_x = analog_read(PIN_ANALOG_X);
  uint16_t cur_y = analog_read(PIN_ANALOG_Y);

  uint16_t value = 0;

  if (cur_x > _analog_max_x)
    _analog_max_x = cur_x;

  if (cur_y > _analog_max_y)
    _analog_max_y = cur_y;

  if (cur_x < _analog_min_x)
    _analog_min_x = cur_x;

  if (cur_y < _analog_min_y)
    _analog_min_y = cur_y; 

  if (cur_x > 650)
    value |= DPAD_LEFT;

  if (cur_x < 300)
    value |= DPAD_RIGHT;

  if (cur_y < 300)
    value |= DPAD_UP;

  if (cur_y > 650)
    value |= DPAD_DOWN;

  return value;
}

uint16_t ctrl_button_state() {
  uint16_t value = 0;

  if (gpio_get(PIN_BUTTON_1) == BUTTON_PRESSED)
    value |= BUTTON_1;

  if (gpio_get(PIN_BUTTON_2) == BUTTON_PRESSED)
    value |= BUTTON_2;

  if (gpio_get(PIN_BUTTON_3) == BUTTON_PRESSED)
    value |= BUTTON_3;

    if (gpio_get(PIN_BUTTON_4) == BUTTON_PRESSED)
    value |= BUTTON_4;

  return value;
}
  */

bool ctrl_is_pressed(int btn) {
  if (btn == BUTTON_ANY) {
    for (int i = 0; i < NUM_BTNS; i++)
      if (keymap[i].key_state == 1)
        return true;
    
    return false;
  } else {
    return keymap[btn].key_state == 1;
  }
}

uint32_t ctrl_button_state_update() {
    uint32_t res = 0;
    
    for (int i = 0; i < NUM_BTNS; i++) {
      
      int keyresult = 0;

      if (keymap[i].digital == BTN_DIGITAL) {
        #ifdef RP2350_E9_WA
        if (keymap[i].pull_mode == BTN_PULLDOWN) {
          gpio_set_dir(keymap[i].gpio_num, GPIO_IN);
          gpio_set_pulls(keymap[i].gpio_num, false, true);
        }
        #endif

        if (gpio_get(keymap[i].gpio_num) == keymap[i].pressed_state)
          keyresult = 1;

        #ifdef RP2350_E9_WA
        if (keymap[i].pull_mode == BTN_PULLDOWN) {
          gpio_set_dir(keymap[i].gpio_num, GPIO_OUT);
          gpio_put(keymap[i].gpio_num, 0);
        }
        #endif

      } else {

        int adc_value = analog_read(keymap[i].gpio_num);
        if ((adc_value > keymap[i].threshold_1) && (adc_value < keymap[i].threshold_2))
          keyresult = 1;        
      }

      if (keyresult) {
        if (keymap[i].key_state == 0) {
          keymap[i].key_state = 1;
          keymap[i].key_timer = to_ms_since_boot(get_absolute_time());
        }
        res |= 1u << i;
      } else {
        keymap[i].key_state = 0;
      }
    }
    
    return res;
}
  

int ctrl_init() {
  adc_init();

  adc_gpio_init(PIN_ANALOG_X);
  adc_gpio_init(PIN_ANALOG_Y);

  ctrl_recalib_analog();

  for (int i = 0; i < NUM_BTNS; i++) {
    if (keymap[i].digital == BTN_DIGITAL) {
      gpio_init(keymap[i].gpio_num);
      gpio_set_dir(keymap[i].gpio_num, GPIO_IN);
    } else {
      adc_gpio_init(keymap[i].gpio_num); // might be init'ed twice, but that's ok
    }

    if (keymap[i].pull_mode == BTN_PULLUP) {
      gpio_set_pulls(keymap[i].gpio_num, true, false);
    } else if (keymap[i].pull_mode == BTN_PULLDOWN) {
      gpio_set_pulls(keymap[i].gpio_num, false, true);
    } else {
      // no pull
    }
}

  return 0;
}
