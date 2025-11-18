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
 
#ifndef POWER_H
#define POWER_H

/* ========================== includes ========================== */

/* ========================= definitions ========================= */
// Communication
#define PWR_ON_UNKOWN 0
#define PWR_ON_BAT 1
#define PWR_ON_GRD 2

#define PWR_BAT_LVL_UNKOWN 255

/* ====================== function declarations ====================== */
int pwr_init();
int pwr_get_source();
int pwr_get_bat_level(int granularity);
int pwr_get_bat_vol();
void pwr_shutdown();
void pwr_sd_sply(bool state);

#endif //POWER_H
