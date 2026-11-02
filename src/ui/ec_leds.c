/* SPDX-License-Identifier: GPL-3.0-only */
/* LED helpers (ec_leds.h) */
#include "ec_leds.h"
#include "ec_plat.h"

void ec_leds_clear(void)
{
    int i;
    for (i = 0; i < EC_NKEY; i++)
        ec_plat_led_key(i, EC_LED_OFF);
    for (i = 0; i < EC_NB; i++)
        ec_plat_led_button(i, EC_LED_OFF);
    ec_plat_led_play_green(EC_LED_OFF);
}

void ec_leds_steps(uint16_t trigs, int playhead)
{
    int i;
    for (i = 0; i < EC_NLOWER; i++)
        ec_plat_led_lower(i, i == playhead ? EC_LED_LIT : (trigs >> i & 1u) ? EC_LED_MID : EC_LED_OFF);
}

void ec_leds_upper(int selected, uint16_t active)
{
    int i;
    for (i = 0; i < EC_NUPPER; i++)
        ec_plat_led_upper(i, i == selected ? EC_LED_LIT : (active >> i & 1u) ? EC_LED_DIM : EC_LED_OFF);
}

void ec_leds_button(int label, uint8_t level) { ec_plat_led_button(label, level); }
