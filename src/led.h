#ifndef LED_H
#define LED_H
#include <stdint.h>
#include <zephyr/drivers/gpio.h>

#define LED0_NODE DT_ALIAS(led0)

extern const struct gpio_dt_spec led;

int8_t init_led();
void set_led_state(uint8_t state);
void led_on();
void led_off();
void led_toggle();

#endif /* LED_H */