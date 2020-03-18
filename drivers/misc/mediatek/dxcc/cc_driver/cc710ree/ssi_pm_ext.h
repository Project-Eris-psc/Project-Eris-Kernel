/*****************************************************************************
* Copyright (C) 2015 ARM Limited or its affiliates.	                     *
* This program is free software; you can redistribute it and/or modify it    *
* under the terms of the GNU General Public License as published by the Free *
* Software Foundation; either version 2 of the License, or (at your option)  * 
* any later version.							     *
* This program is distributed in the hope that it will be useful, but 	     *
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY *
* or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License   *
* for more details.							     *	
* You should have received a copy of the GNU General Public License along    *
* with this program; if not, write to the Free Software Foundation, 	     *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.        *
******************************************************************************/

/* \file ssi_pm_ext.h
    */

#ifndef __PM_EXT_H__
#define __PM_EXT_H__


#include "ssi_config.h"
#include "ssi_driver.h"
#include <linux/clk.h>

void ssi_pm_ext_hw_suspend(struct device *dev, struct clk *dxcc_pub_clk);

void ssi_pm_ext_hw_resume(struct device *dev, struct clk *dxcc_pub_clk);


#endif /*__POWER_MGR_H__*/

