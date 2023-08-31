#include "led.h"

#include <stdio.h>

const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int8_t init_led() {
    int ret;

    if (!gpio_is_ready_dt(&led)) {
        return -1;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return -1;
    }
    return 0;
}

void set_led_state(uint8_t state) {
    if (state)
        led_on();
    else if (!state)
        led_off();
}

void led_on() { gpio_pin_set_dt(&led, 1); }
void led_off() { gpio_pin_set_dt(&led, 0); }
void led_toggle() { gpio_pin_toggle_dt(&led); }