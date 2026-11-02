/* SPDX-License-Identifier: GPL-3.0-only */
/* The tests' fixed layout: 0.4.0's pages (host/fixtures/layout-0.4.0.json -> host/ec_layout_test_gen.c). The tests
 * and the demo scripts walk the pages by panel input, so they run on this frozen layout whatever the shipped design
 * (design/layout/elasticat.json) becomes; host/test_layout.c checks the shipped one by finding params by id. */
#ifndef EC_LAYOUT_TEST_H
#define EC_LAYOUT_TEST_H
#include "ec_layout.h"
extern const ec_layout_t EC_LAYOUT_TEST;
#endif
