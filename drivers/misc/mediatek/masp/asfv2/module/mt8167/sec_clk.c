/*
 * Copyright (C) 2011-2014 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include "../sec_clk.h"

#define SEC_DEV_NAME "sec"

int sec_clk_enable(struct platform_device *dev)
{
	struct clk *sec_clk;
	struct clk *sec_clk_13m;

	sec_clk = devm_clk_get(&dev->dev, "main");
	sec_clk_13m = devm_clk_get(&dev->dev, "second");

	if (IS_ERR(sec_clk))
		return PTR_ERR(sec_clk);

	if (IS_ERR(sec_clk_13m))
		return PTR_ERR(sec_clk_13m);

	clk_prepare_enable(sec_clk);
	clk_prepare_enable(sec_clk_13m);

	pr_debug("[%s] get hacc clock\n", SEC_DEV_NAME);

	return 0;
}

