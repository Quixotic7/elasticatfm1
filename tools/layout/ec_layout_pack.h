/* SPDX-License-Identifier: GPL-3.0-only */
/* The layout's packed text form: read into a layout (the designer's preview), written from one (tests). */
#ifndef EC_LAYOUT_PACK_H
#define EC_LAYOUT_PACK_H
#include "ec_layout.h"

/* the packed text -> a layout in static storage (one at a time: the next call replaces it); NULL on a bad pack */
const ec_layout_t *ec_layout_unpack(const char *text);
const char *ec_layout_pack_error(void);
/* a layout -> its packed text; returns the length, -1 when cap is too small */
int ec_layout_pack(const ec_layout_t *l, char *out, int cap);

#endif
