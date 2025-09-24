/*
 * Pico Engine - a library for the Pico Held 2 handheld
 *
 * Copyright (C) 2023 - 2025 Daniel Kammer (daniel.kammer@web.de)
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
  
#ifndef PICOENGINE_H
#define PICOENGINE_H

/* ========================== includes ========================== */
// HAL
#include "hardware/lcd/backlight/backlight.h"
#include "hardware/lcd/lcd.h"
#include "hardware/lcd/lcd_mcu/lcd_mcu.h"
#include "hardware/lcd/lcd_dpi/lcd_dpi.h"
#include "hardware/sound/sound.h"
#include "hardware/controls/controls.h"
#include "hardware/power/power.h"
#include "hardware/bootloader/bootloader.h"
#include "setup.h"

// Graphics
#include "graphics/gbuffers.h"
#include "graphics/colors.h"
#include "graphics/primitives.h"
#include "graphics/blitter.h"
#include "graphics/tilemap.h"
#include "fonts/fonts.h"

/* ======================== definitions ========================= */

#define PICOENGINE_VERSION 250321

// error messages
#define PE_SUCCESS           0
#define PE_UNKNOWN_ERROR    -1

/* ====================== init ====================== */
int pe_init(pe_config_t pe_cfg) {
  ctrl_init();
  
  bool bl_req = bl_check_selection();

  if (bl_req)
    bl_launch_bl();


//  if (lcd_init(pe_cfg.lcd_num_fb, pe_cfg.lcd_refresh_mode, pe_cfg.lcd_scale) != LCD_SUCCESS)
//    return PE_UNKNOWN_ERROR;

/*
  if ((lcd_init() != LCD_SUCCESS) && (!bl_req))
    return PE_UNKNOWN_ERROR;

  if (bl_req)
    bl_menu();

#ifndef PE_SND_INIT_MANUAL
  if (snd_init() != SND_SUCCESS)
    return PE_UNKNOWN_ERROR;
#endif
*/

  if (pwr_init() != 0)
    return PE_UNKNOWN_ERROR;
  
  return PE_SUCCESS;
}

#endif // BLITTER
