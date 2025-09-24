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
 
#ifndef SETUP_H
#define SETUP_H

/* ------------------------ screen orientatin ------------------------*/
// 0 = 0° (portrait), 1 = 90° (landscape), 2 = 180° (portrait), 3 = 270° (landscape)
#define LCD_ROTATION (1)

/* ---------------------- color depth ----------------------*/
// currently supported are 8 bits and 16 bits of colors
#define LCD_COLORDEPTH (16)

/* ----------------- pixel doubling / panel fitting -----------------*/
// please choose 1 or 0 fitters

/* ========================= sanity check ========================= */
#if LCD_COLORDEPTH!=8 && LCD_COLORDEPTH!=16
#error "Please choose a valid color depth"
#endif

#endif //SETUP_H
