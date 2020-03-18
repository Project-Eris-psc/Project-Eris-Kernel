/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MAG_FACTORY_H__
#define __MAG_FACTORY_H__

#include "mag.h"
#include "cust_mag.h"

extern struct mag_context *mag_context_obj;

#define SETCALI 1
#define CLRCALI 2
#define GETCALI 3

int mag_factory_device_init(void);

#endif

