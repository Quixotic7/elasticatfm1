/* SPDX-License-Identifier: GPL-3.0-only */
/* LED helpers over ec_plat.h: the lower row as 16 steps, the upper row as tracks / buses, the buttons */
#ifndef EC_LEDS_H
#define EC_LEDS_H
#include <stdint.h>

void ec_leds_clear(void);                          /* every key and button LED off */
/* the lower row: trigs (bit i: step i has a trig, orange on the device's mockup), the playhead step (-1 none) */
void ec_leds_steps(uint16_t trigs, int playhead);
/* the upper row: the selected index LIT, active ones (bit i) DIM, others off */
void ec_leds_upper(int selected, uint16_t active);
void ec_leds_button(int label, uint8_t level);

#endif
