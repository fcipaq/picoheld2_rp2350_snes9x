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

#ifndef BUTTONS_H
#define BUTTONS_H

/* ========================== includes ========================== */
#include <cstdint>

/* ========================= definitions ========================= */
/* --------------------- software assignment -------------------- */
enum {
    BUTTON_A = 0,
    BUTTON_B,
    BUTTON_X,
    BUTTON_Y,
    BUTTON_ST,
    BUTTON_SEL,
    BUTTON_L,
    BUTTON_R,
    BUTTON_SW,

    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_UP,
    BUTTON_DOWN,
    

    // last entry
    NUM_BTNS,
    BUTTON_ANY
};

typedef struct {
  int              pin_id;
  int              gpio_num;
  int              digital;
  int              key_state;
  int              pull_mode;
  int              threshold_1;
  int              threshold_2;
  int              pressed_state;
  uint32_t         key_timer;
  int32_t          debounce;
} keymap_t;

/* ====================== function declarations ====================== */
int ctrl_init();

void ctrl_recalib_analog();

uint16_t ctrl_dpad_state();
uint32_t ctrl_button_state_update();
int8_t ctrl_analog_x_state();
int8_t ctrl_analog_y_state();

bool ctrl_is_pressed(int btn);
int analog_read(int gpio_num);

#endif // BUTTONS_H
