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

#include "power.h"
#include <hwcfg.h>

uint32_t bat_vol;
bool sd_force_off = true;

#define BAT_AVG_WIN (50)

void pwr_sd_sply(bool state) {
  if (!state) {  
    gpio_set_dir(PIN_SD_DET, GPIO_OUT);
    gpio_put(PIN_SD_DET, 1);  // override the HW SD_DET line
    sd_force_off = true;
  } else {
    gpio_set_dir(PIN_SD_DET, GPIO_IN);
    gpio_set_pulls(PIN_SD_DET, false, false);  // floating means HW SD_DET can control the power line
    sd_force_off = false;
  }
}

bool pwr_sd_det() {
  if (sd_force_off)
    return false;

  return gpio_get(PIN_SD_DET) == 1 ? true : false;
}

// Read raw battery voltage (without averaging)
uint16_t pwr_read_bat_vol() {
  adc_select_input(PIN_BAT_ADC - ADC_PIN_OFFSET);
  return (adc_read() * PWR_REF_VOL /* mV */ * 2 / 4096);
}

// Read raw battery and average values
int pwr_get_bat_vol() {
  bat_vol = (bat_vol * BAT_AVG_WIN + pwr_read_bat_vol()) / (BAT_AVG_WIN + 1);
  return bat_vol;
}

// Return type of currently used power source:
// either on battery or on grid
int pwr_get_source() {
  if (!gpio_get(PIN_CHG_STAT) || pwr_get_bat_vol() > (PWR_CHG_MIN_VOL))
    return PWR_ON_GRD;
  else
    return PWR_ON_BAT;
}

#if 0
/*
 * battery OCV [mV] vs. SoC [%] LUT
 * OCV can be measured typically up to a current of 25 mA (see data sheet below)
 * src: https://pdfserv.maximintegrated.com/en/an/AN4189.pdf
 */
const unsigned int bat_chg_by_vol[] PROGMEM =
{ 
  3306, //   0%
  3687, //  10%
  3741, //  20%
  3775, //  30%
  3793, //  40%
  3821, //  50%
  3884, //  60%
  3945, //  70%
  4008, //  80%
  4086, //  90%
  4177  // 100%
};
#endif

// Return battery power level in percent (linear)
int pwr_get_bat_percent() {
  return 0;
}

int pwr_get_bat_level(int granularity) {
  //if (pwr_get_source() == PWR_ON_GRD)
    // some other logic

  int level;

  if (level > 100)
    level = 100;

  if (level < 0)
    level = 0;
  
  level = (pwr_get_bat_vol() - PWR_BAT_MIN_VOL) * 100 / (PWR_BAT_MAX_VOL - PWR_BAT_MIN_VOL);

  // return level in steps of "granularity", because the voltage divider
  // will otherwise deliver very inaccurate data
  level /= 100 / granularity;

  if (level > (granularity - 1))
    level = granularity - 1;

  if (level < 0)
    level = 0;

  return level;

}

void pwr_shutdown() {
  gpio_put(PIN_PWR_EN, 0);
}

void pwr_turnon() {
  // latch power switch
  gpio_put(PIN_PWR_EN, 1);
}

int pwr_init()
{
  gpio_init(PIN_PWR_EN);
  gpio_set_dir(PIN_PWR_EN, GPIO_OUT);
  pwr_turnon();
  
  //adc_init();  // already done in ctrl init
  adc_gpio_init(PIN_BAT_ADC);
//  adc_gpio_init(PIN_5V_ADC);

  // initialize battery voltage
  bat_vol = 0;
  for (int i = 0; i < 10; i++)
    bat_vol += pwr_read_bat_vol();
  bat_vol /= 10;

  gpio_init(PIN_CHG_STAT);
  gpio_set_dir(PIN_CHG_STAT, GPIO_IN);
  gpio_set_pulls(PIN_CHG_STAT, true, false);  // pull up

  pwr_sd_sply(0);
 
  return 0;
}
