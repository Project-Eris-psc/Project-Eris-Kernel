/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>

#define RT5738_DRV_VERSION	"1.1.0_MTK"

#define RT5738_REG_VSEL0	(0x00)
#define RT5738_REG_VSEL1	(0x01)
#define RT5738_REG_CTRL1	(0x02)
#define RT5738_REG_ID1		(0x03)
#define RT5738_REG_ID2		(0x04)
#define RT5738_REG_MONITOR	(0x05)
#define RT5738_REG_CTRL2	(0x06)
#define RT5738_REG_CTRL3	(0x07)
#define RT5738_REG_CTRL4	(0x08)

#define RT5738_VID_MASK		(0xe0)
#define RT5738_VID_SHIFT	(5)
#define RT5738_DID_MASK		(0x0f)
#define RT5738_DID_SHIFT	(0)

#define RT5738_INFO(format, args...) pr_info(format, ##args)
#define RT5738_ERR(format, args...)	pr_info(format, ##args)

#define RT5738_MIN_VOLTAGE	(300000)
#define RT5738_MAX_VOLTAGE	(1300000)
#define RT5738_VSEL_STEP	(5000)

#define rt5738_vsell_vol_reg	(RT5738_REG_VSEL0)
#define rt5738_vsell_vol_mask	(0xff)
#define rt5738_vsell_vol_shift	(0)
#define rt5738_vsell_enable_reg	(RT5738_REG_CTRL2)
#define rt5738_vsell_enable_mask	(0x01)
#define rt5738_vsell_enable_shift	(0x00)
#define rt5738_vsell_mode_reg	(RT5738_REG_CTRL1)
#define rt5738_vsell_mode_mask	(0x01)
#define rt5738_vsell_mode_shift	(0x00)

#define rt5738_vselh_vol_reg	(RT5738_REG_VSEL1)
#define rt5738_vselh_vol_mask	(0xff)
#define rt5738_vselh_vol_shift	(0)
#define rt5738_vselh_enable_reg	(RT5738_REG_CTRL2)
#define rt5738_vselh_enable_mask (0x02)
#define rt5738_vselh_enable_shift (0x01)
#define rt5738_vselh_mode_reg	(RT5738_REG_CTRL1)
#define rt5738_vselh_mode_mask	(0x02)
#define rt5738_vselh_mode_shift	(0x01)

#define rt5738_chip_data_decl(_name) \
{ \
	.vol_reg = _name##_vol_reg, \
	.vol_mask = _name##_vol_mask, \
	.vol_shift = _name##_vol_shift, \
	.enable_reg = _name##_enable_reg, \
	.enable_mask = _name##_enable_mask, \
	.enable_shift = _name##_enable_shift, \
	.mode_reg = _name##_mode_reg, \
	.mode_mask = _name##_mode_mask, \
	.mode_shift = _name##_mode_shift, \
	.ramp_up_reg = RT5738_REG_CTRL1, \
	.ramp_down_reg = RT5738_REG_CTRL2, \
	.ramp_up_mask = (0x70), \
	.ramp_down_mask = (0xd0), \
	.ramp_up_shift = 4, \
	.ramp_down_shift = 5, \
}

enum {
	VSEL_LOW_DEFAULT,
	VSEL_HIGH_DEFAULT,
	VSEL_ALWAYS_LOW,
	VSEL_ALWAYS_HIGH,
	VSEL_MAX,
};

struct rt5738_chip_data {
	unsigned char vol_reg;
	unsigned char vol_mask;
	unsigned char vol_shift;
	unsigned char enable_reg;
	unsigned char enable_mask;
	unsigned char enable_shift;
	unsigned char mode_reg;
	unsigned char mode_mask;
	unsigned char mode_shift;
	unsigned char ramp_up_reg;
	unsigned char ramp_up_mask;
	unsigned char ramp_up_shift;
	unsigned char ramp_down_reg;
	unsigned char ramp_down_mask;
	unsigned char ramp_down_shift;
};

static struct rt5738_chip_data rt5738_chip_data_map[VSEL_MAX] = {
	rt5738_chip_data_decl(rt5738_vsell),
	rt5738_chip_data_decl(rt5738_vselh),
};

struct rt5738_regulator_info {
	struct device *dev;
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct rt5738_chip_data *chip_data;
	struct regulator_desc *desc;
	struct regulator_dev *regulator;
	int vsel_gpio;
	int ramp_up_val;
	int ramp_down_val;
};

static const struct regmap_config rt5738_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int rt5738_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct rt5738_chip_data *chip = info->chip_data;
	const int count = rdev->desc->n_voltages;

	if (selector > count)
		return -EINVAL;

	return regmap_write(info->regmap, chip->vol_reg, selector);
}

static int rt5738_get_voltage_sel(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct rt5738_chip_data *chip = info->chip_data;
	unsigned int regval;
	int ret;

	ret = regmap_read(info->regmap, chip->vol_reg, &regval);
	if (ret < 0)
		return ret;

	return regval;
}

static int rt5738_enable(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct rt5738_chip_data *chip = info->chip_data;

	return regmap_update_bits(info->regmap, chip->enable_reg,
				  chip->enable_mask, 0x1 << chip->enable_shift);
}

static int rt5738_disable(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct rt5738_chip_data *chip = info->chip_data;

	return regmap_update_bits(info->regmap, chip->enable_reg,
				  chip->enable_mask, 0x0 << chip->enable_shift);
}

static int rt5738_is_enabled(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct rt5738_chip_data *chip = info->chip_data;
	unsigned int regval;
	int ret;

	ret = regmap_read(info->regmap, chip->enable_reg, &regval);
	if (ret < 0)
		return ret;

	return regval & chip->enable_mask ? 1 : 0;
}

static int rt5738_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct rt5738_chip_data *chip = info->chip_data;
	int ret;
	unsigned int regval = 0;

	switch (mode) {
	case REGULATOR_MODE_FAST:	/* force pwm mode */
		ret = regmap_update_bits(info->regmap,
					 chip->mode_reg, chip->mode_mask,
					 0x1 << chip->mode_shift);
		break;
	case REGULATOR_MODE_NORMAL:
	default:
		ret = regmap_update_bits(info->regmap,
					 chip->mode_reg, chip->mode_mask,
					 0x0 << chip->mode_shift);
		break;
	}

	regmap_read(info->regmap, chip->mode_reg, &regval);
	RT5738_INFO("%s: control1: %x\n", __func__, regval);

	return ret;
}

static unsigned int rt5738_get_mode(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct rt5738_chip_data *chip = info->chip_data;
	int ret;
	unsigned int regval = 0;

	ret = regmap_read(info->regmap, chip->mode_reg, &regval);
	if (ret < 0) {
		RT5738_ERR("%s read mode fail\n", __func__);
		return ret;
	}

	if (regval & chip->mode_mask)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_NORMAL;
}

static int rt5738_set_ramp_rate(struct regulator_dev *rdev)
{
	struct rt5738_regulator_info *info = rdev_get_drvdata(rdev);
	struct rt5738_chip_data *chip = info->chip_data;
	int ret;

	ret = regmap_update_bits(info->regmap, chip->ramp_up_reg,
				 chip->ramp_up_mask,
				 info->ramp_up_val << chip->ramp_up_shift);

	if (ret < 0) {
		pr_info("%s_up fail, ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	ret = regmap_update_bits(info->regmap, chip->ramp_down_reg,
				 chip->ramp_down_mask,
				 info->ramp_down_val << chip->ramp_down_shift);
	if (ret < 0) {
		pr_info("%s_down fail, ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;
}

static const struct regulator_ops rt5738_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = rt5738_set_voltage_sel,
	.get_voltage_sel = rt5738_get_voltage_sel,
	.enable = rt5738_enable,
	.disable = rt5738_disable,
	.is_enabled = rt5738_is_enabled,
	.set_mode = rt5738_set_mode,
	.get_mode = rt5738_get_mode,
};

static struct regulator_desc rt5738_regulator_desc = {
	.id = 0,
	.name = "rt5738",
	.min_uV = RT5738_MIN_VOLTAGE,
	.uV_step = RT5738_VSEL_STEP,
	.n_voltages = 201,
	.ops = &rt5738_regulator_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
};

static inline struct regulator_dev *rt5738_regulator_register(struct
						      regulator_desc
						      *desc,
						      struct device
						      *dev,
						      struct
						      regulator_init_data
						      *init_data,
						      void *driver_data)
{
	struct regulator_config config = {
		.dev = dev,
		.init_data = init_data,
		.driver_data = driver_data,
		.of_node = dev->of_node,
	};

	return devm_regulator_register(dev, desc, &config);
}

static int rt5738_parse_dt(struct device *dev,
			   struct rt5738_regulator_info *info)
{
	struct device_node *np = of_find_node_by_name(NULL, "rt5738_buck");
	u32 val;
	int ret = 0;

	if (!np) {
		RT5738_ERR("%s cant find node (0x%02x)\n",
			   __func__, info->i2c->addr);
		return -ENODEV;
	}

	ret = of_get_named_gpio(np, "rt,vsel_gpio", 0);
	if (ret < 0)
		pr_info("%s: no vsel_gpio info\n", __func__);
	info->vsel_gpio = ret;

	ret = of_property_read_u32(np, "ramp_up", &val);
	if (ret < 0) {
		pr_info("%s: no ramp_up info, use default rate\n", __func__);
		info->ramp_up_val = 1;	/* 001 : 12mV/us */
	} else
		info->ramp_up_val = val;

	ret = of_property_read_u32(np, "ramp_down", &val);
	if (ret < 0) {
		pr_info("%s: no ramp_down info, use default rate\n", __func__);
		info->ramp_down_val = 3;	/* 011 : 3mV/us */
	} else
		info->ramp_down_val = val;

	return 0;
}

static struct regulator_init_data *rt_parse_regulator_init_data(struct device
								*dev,
								const char
								*node_name)
{
	struct regulator_init_data *init_data;
	struct device_node *np = dev->of_node;

	init_data = of_get_regulator_init_data(dev, np, NULL);
	if (init_data) {
		dev_info(dev,
			 "regulator_name = %s, min_uV = %d, max_uV = %d\n",
			 init_data->constraints.name,
			 init_data->constraints.min_uV,
			 init_data->constraints.max_uV);
	} else {
		dev_info(dev, "no init data for %s\n", node_name);
		return NULL;
	}

	init_data->constraints.valid_modes_mask |=
	    (REGULATOR_MODE_NORMAL | REGULATOR_MODE_FAST);
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_MODE;

	return init_data;
}

static const struct of_device_id rt_match_table[] = {
	{.compatible = "richtek,rt5738_l", .data = (void *)VSEL_LOW_DEFAULT,},
	{.compatible = "richtek,rt5738_h", .data = (void *)VSEL_HIGH_DEFAULT,},
	{.compatible = "richtek,rt5738l", .data = (void *)VSEL_ALWAYS_LOW,},
	{.compatible = "richtek,rt5738h", .data = (void *)VSEL_ALWAYS_HIGH,},
	{},
};

MODULE_DEVICE_TABLE(of, rt_match_table);

static const struct i2c_device_id rt_dev_id[] = {
	{"rt5738_l", (kernel_ulong_t) VSEL_LOW_DEFAULT,},
	{"rt5738_h", (kernel_ulong_t) VSEL_HIGH_DEFAULT,},
	{"rt5738l", (kernel_ulong_t) VSEL_ALWAYS_LOW,},
	{"rt5738h", (kernel_ulong_t) VSEL_ALWAYS_HIGH,},
	{},
};

MODULE_DEVICE_TABLE(i2c, rt_dev_id);

static int rt5738_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct rt5738_regulator_info *info;
	struct regulator_init_data *init_data = NULL;
	const struct of_device_id *of_id;
	long int dev_id;
	int ret;

	RT5738_INFO("%s ver(%s) slv(0x%02x)\n",
		    __func__, RT5738_DRV_VERSION, i2c->addr);

	info = devm_kzalloc(&i2c->dev,
			    sizeof(struct rt5738_regulator_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->regmap = devm_regmap_init_i2c(i2c, &rt5738_regmap_config);
	if (IS_ERR(info->regmap)) {
		ret = PTR_ERR(info->regmap);
		dev_info(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	if (i2c->dev.of_node) {
		of_id = of_match_node(rt_match_table, i2c->dev.of_node);
		if (!of_id)
			return -EINVAL;
		dev_id = (long int)of_id->data;
	} else
		dev_id = (long int)id->driver_data;

	if (dev_id == VSEL_ALWAYS_HIGH || dev_id == VSEL_LOW_DEFAULT)
		info->chip_data = &rt5738_chip_data_map[1];
	else
		info->chip_data = &rt5738_chip_data_map[0];

	init_data = rt_parse_regulator_init_data(&i2c->dev, "rt5738_buck");
	if (init_data == NULL) {
		dev_info(&i2c->dev, "no init data\n");
		return -EINVAL;
	}

	info->i2c = i2c;
	info->dev = &i2c->dev;
	info->desc = &rt5738_regulator_desc;
	i2c_set_clientdata(i2c, info);

	ret = rt5738_parse_dt(&i2c->dev, info);
	if (ret < 0) {
		RT5738_ERR("%s parse dt (%x) fail\n", __func__, i2c->addr);
		return ret;
	}

	/* soft reset : reset register before DVFS */
	ret = regmap_write(info->regmap, RT5738_REG_CTRL1, 0x04);
	if (ret < 0) {
		RT5738_ERR("%s: soft reset fail\n", __func__);
		return ret;
	}

	info->regulator = rt5738_regulator_register(info->desc,
						    &i2c->dev, init_data, info);
	if (IS_ERR(info->regulator)) {
		dev_info(&i2c->dev, "fail to register rt5738 regulator : %s\n",
			i2c->dev.of_node->name);
		return ret;
	}

	ret = rt5738_set_ramp_rate(info->regulator);
	if (ret < 0) {
		dev_info(&i2c->dev, "%s : fail to set ramp rate\n", __func__);
		return ret;
	}

	/* if compatible = VSEL_LOW_DEFAULT, default = VSEL_L,
	 * DVFS use VSEL_H, and vice visa.
	 */
	if (dev_id == VSEL_LOW_DEFAULT)
		devm_gpio_request_one(info->dev, info->vsel_gpio,
				      GPIOF_OUT_INIT_HIGH, "rt5738-VSEL");
	else if (dev_id == VSEL_HIGH_DEFAULT)
		devm_gpio_request_one(info->dev, info->vsel_gpio,
				      GPIOF_OUT_INIT_LOW, "rt5738-VSEL");

	pr_info("%s Successfully\n", __func__);
	return 0;
}

static int rt5738_i2c_remove(struct i2c_client *i2c)
{
	struct rt5738_regulator_info *info = i2c_get_clientdata(i2c);

	regulator_unregister(info->regulator);
	return 0;
}

static struct i2c_driver rt5738_i2c_driver = {
	.driver = {
		   .name = "rt5738",
		   .owner = THIS_MODULE,
		   .of_match_table = rt_match_table,
		   },
	.probe = rt5738_i2c_probe,
	.remove = rt5738_i2c_remove,
	.id_table = rt_dev_id,
};

module_i2c_driver(rt5738_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("gene_chen <gene_chen@richtek.com>");
MODULE_AUTHOR("menghui lin <menghui.lin@mediatek.com>");
MODULE_VERSION("1.1.0_MTK");
MODULE_DESCRIPTION("Regulator driver for RT5738");
