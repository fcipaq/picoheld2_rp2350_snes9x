#include "backlight.h"

pwm_config lcd_bl_pwm_cfg = pwm_get_default_config();

int lcd_backlight_level;

int lcd_backlight_get() {
  return lcd_backlight_level;
}

int lcd_backlight_set(int level) {
  if ((level < 0) || (level > 100))
    return BACKLIGHT_ILL_LEVEL;
  lcd_backlight_level = level;
  pwm_set_gpio_level(PIN_LCD_BL_PWM, lcd_backlight_level);
  return BACKLIGHT_SUCCESS;
}

void lcd_backlight_init() {
  // setup backlight PWM
  lcd_backlight_level = 0;
  gpio_init(PIN_LCD_BL_PWM);
  gpio_set_function(PIN_LCD_BL_PWM, GPIO_FUNC_PWM);
  uint slice_num_backlight = pwm_gpio_to_slice_num(PIN_LCD_BL_PWM);
  pwm_clear_irq(slice_num_backlight);
  pwm_config_set_clkdiv(&lcd_bl_pwm_cfg, 1000000.0f);
  pwm_config_set_wrap(&lcd_bl_pwm_cfg, 100);
  pwm_init(slice_num_backlight, &lcd_bl_pwm_cfg, true);
  lcd_backlight_set(lcd_backlight_level);
}