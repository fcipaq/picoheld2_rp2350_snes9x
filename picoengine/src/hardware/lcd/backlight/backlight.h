#pragma once

#include "hardware/pwm.h"
#include "hardware/gpio.h"

#include <hwcfg.h>

typedef enum {
  BACKLIGHT_SUCCESS = 0,  		/**< @brief Command completed successfully */
  BACKLIGHT_ILL_LEVEL = -1,   /**< @brief The requested level is out of the valid range. */
} backlight_error_t; 

int lcd_backlight_get();
int lcd_backlight_set(int level);
void lcd_backlight_init();