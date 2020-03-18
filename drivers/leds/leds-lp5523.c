/*
 * lp5523.c - LP5523, LP55231 LED Driver
 *
 * Copyright (C) 2010 Nokia Corporation
 * Copyright (C) 2012 Texas Instruments
 *
 * Contact: Samu Onkalo <samu.p.onkalo@nokia.com>
 *          Milo(Woogyom) Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_data/leds-lp55xx.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>

#include "leds-lp55xx-common.h"

#define LP5523_PROGRAM_LENGTH		32	/* bytes */
/* Memory is used like this:
   0x00 engine 1 program
   0x10 engine 2 program
   0x20 engine 3 program
   0x30 engine 1 muxing info
   0x40 engine 2 muxing info
   0x50 engine 3 muxing info
*/
#define LP5523_MAX_LEDS			9

/* Registers */
#define LP5523_REG_ENABLE		0x00
#define LP5523_REG_OP_MODE		0x01
#define LP5523_REG_ENABLE_LEDS_MSB	0x04
#define LP5523_REG_ENABLE_LEDS_LSB	0x05
#define LP5523_REG_LED_CTRL_BASE	0x06
#define LP5523_REG_LED_PWM_BASE		0x16
#define LP5523_REG_LED_CURRENT_BASE	0x26
#define LP5523_REG_CONFIG		0x36
#define LP5523_REG_STATUS		0x3A
#define LP5523_REG_RESET		0x3D
#define LP5523_REG_LED_TEST_CTRL	0x41
#define LP5523_REG_LED_TEST_ADC		0x42
#define LP5523_REG_MASTER_FADER_BASE	0x48
#define LP5523_REG_CH1_PROG_START	0x4C
#define LP5523_REG_CH2_PROG_START	0x4D
#define LP5523_REG_CH3_PROG_START	0x4E
#define LP5523_REG_PROG_PAGE_SEL	0x4F
#define LP5523_REG_PROG_MEM		0x50

/* Bit description in registers */
#define LP5523_ENABLE			0x40
#define LP5523_AUTO_INC			0x40
#define LP5523_PWR_SAVE			0x20
#define LP5523_PWM_PWR_SAVE		0x04
#define LP5523_CP_AUTO			0x18
#define LP5523_AUTO_CLK			0x02

#define LP5523_EN_LEDTEST		0x80
#define LP5523_LEDTEST_DONE		0x80
#define LP5523_RESET			0xFF
#define LP5523_ADC_SHORTCIRC_LIM	80
#define LP5523_EXT_CLK_USED		0x08
#define LP5523_ENG_STATUS_MASK		0x07

#define LP5523_FADER_MAPPING_MASK	0xC0
#define LP5523_FADER_MAPPING_SHIFT	6

/* Memory Page Selection */
#define LP5523_PAGE_ENG1		0
#define LP5523_PAGE_ENG2		1
#define LP5523_PAGE_ENG3		2
#define LP5523_PAGE_MUX1		3
#define LP5523_PAGE_MUX2		4
#define LP5523_PAGE_MUX3		5

/* Program Memory Operations */
#define LP5523_MODE_ENG1_M		0x30	/* Operation Mode Register */
#define LP5523_MODE_ENG2_M		0x0C
#define LP5523_MODE_ENG3_M		0x03
#define LP5523_LOAD_ENG1		0x10
#define LP5523_LOAD_ENG2		0x04
#define LP5523_LOAD_ENG3		0x01

#define LP5523_ENG1_IS_LOADING(mode)	\
	((mode & LP5523_MODE_ENG1_M) == LP5523_LOAD_ENG1)
#define LP5523_ENG2_IS_LOADING(mode)	\
	((mode & LP5523_MODE_ENG2_M) == LP5523_LOAD_ENG2)
#define LP5523_ENG3_IS_LOADING(mode)	\
	((mode & LP5523_MODE_ENG3_M) == LP5523_LOAD_ENG3)

#define LP5523_EXEC_ENG1_M		0x30	/* Enable Register */
#define LP5523_EXEC_ENG2_M		0x0C
#define LP5523_EXEC_ENG3_M		0x03
#define LP5523_EXEC_M			0x3F
#define LP5523_RUN_ENG1			0x20
#define LP5523_RUN_ENG2			0x08
#define LP5523_RUN_ENG3			0x02

#define LED_ACTIVE(mux, led)		(!!(mux & (0x0001 << led)))

enum lp5523_chip_id {
	LP5523,
	LP55231,
};

static struct i2c_client		*lp5523_i2c_client;
static struct lp55xx_chip *lp5523_chip[4];
static struct i2c_client * lp5523_client[4];
static int lp5523_probe_nums = -1;
static struct task_struct *led_task;
static int led_task_flag = true;

#define ALL_LED_RED     "000000111"
#define ALL_LED_GREEN   "010101000"
#define ALL_LED_BLUE    "101010000"
#define ALL_LED_WHITE   "111111111"
#define LOAD  "load"
#define PATTERN_NAME  "9d80400004ff05ff437f0000"
#define BITS  "101010000"
#define RUN  "run"
#define DISABLE  "disable"

char color_string[10] = {0};

#define LP5523_LED_ON (1)
#define LP5523_LED_OFF (0)
#define LP55231_LED_ALL_ON (0xa)
#define LP55231_LED_ALL_OFF (0xb)

#define FIRST_DEVICE (0x00)
#define SECOND_DEVICE (0x01)
#define THIRD_DEVICE (0x02)
#define FOURTH_DEVICE (0x03)
#define ALL_DEVICE (0x04)

#define FISRT_DEVICE_STOP (0x5)
#define SECOND_DEVICE_STOP   (0x6)
#define THIRD_DEVICE_STOP  (0x7)
#define FOURTH_DEVICE_STOP  (0x8)
#define COLOR_RED  (0x0)
#define COLOR_GREEN (0x1)
#define COLOR_BLUE  (0x2)
#define LED_ALL_COLOR (0x4)
#define LED_STOP_ALL (0xff)

#define LED_INDEX_FIRST (0)
#define LED_INDEX_SECOND (1)
#define LED_INDEX_THIRD (2)

static u8 msb[4] = {0} ;
static u8 lsb[4] = {0} ;


static int lp5523_init_program_engine(struct lp55xx_chip *chip);

static inline void lp5523_wait_opmode_done(void)
{
	usleep_range(1000, 2000);
}

static void lp5523_set_led_current(struct lp55xx_led *led, u8 led_current)
{
	led->led_current = led_current;
	lp55xx_write(led->chip, LP5523_REG_LED_CURRENT_BASE + led->chan_nr,
		led_current);
}

static int lp5523_post_init_device(struct lp55xx_chip *chip)
{
	int ret;

	ret = lp55xx_write(chip, LP5523_REG_ENABLE, LP5523_ENABLE);
	if (ret)
		return ret;

	/* Chip startup time is 500 us, 1 - 2 ms gives some margin */
	usleep_range(1000, 2000);

	ret = lp55xx_write(chip, LP5523_REG_CONFIG,
			    LP5523_AUTO_INC | LP5523_PWR_SAVE |
			    LP5523_CP_AUTO | LP5523_AUTO_CLK |
			    LP5523_PWM_PWR_SAVE);
	if (ret)
		return ret;

	/* turn on all leds */
	ret = lp55xx_write(chip, LP5523_REG_ENABLE_LEDS_MSB, 0x01);
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_ENABLE_LEDS_LSB, 0xff);
	if (ret)
		return ret;

	return lp5523_init_program_engine(chip);
}

static void lp5523_load_engine(struct lp55xx_chip *chip)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 mask[] = {
		[LP55XX_ENGINE_1] = LP5523_MODE_ENG1_M,
		[LP55XX_ENGINE_2] = LP5523_MODE_ENG2_M,
		[LP55XX_ENGINE_3] = LP5523_MODE_ENG3_M,
	};

	u8 val[] = {
		[LP55XX_ENGINE_1] = LP5523_LOAD_ENG1,
		[LP55XX_ENGINE_2] = LP5523_LOAD_ENG2,
		[LP55XX_ENGINE_3] = LP5523_LOAD_ENG3,
	};

	lp55xx_update_bits(chip, LP5523_REG_OP_MODE, mask[idx], val[idx]);

	lp5523_wait_opmode_done();
}

static void lp5523_load_engine_and_select_page(struct lp55xx_chip *chip)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 page_sel[] = {
		[LP55XX_ENGINE_1] = LP5523_PAGE_ENG1,
		[LP55XX_ENGINE_2] = LP5523_PAGE_ENG2,
		[LP55XX_ENGINE_3] = LP5523_PAGE_ENG3,
	};

	lp5523_load_engine(chip);

	lp55xx_write(chip, LP5523_REG_PROG_PAGE_SEL, page_sel[idx]);
}

static void lp5523_stop_all_engines(struct lp55xx_chip *chip)
{
	lp55xx_write(chip, LP5523_REG_OP_MODE, 0);
	lp5523_wait_opmode_done();
}

static void lp5523_stop_engine(struct lp55xx_chip *chip)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 mask[] = {
		[LP55XX_ENGINE_1] = LP5523_MODE_ENG1_M,
		[LP55XX_ENGINE_2] = LP5523_MODE_ENG2_M,
		[LP55XX_ENGINE_3] = LP5523_MODE_ENG3_M,
	};

	lp55xx_update_bits(chip, LP5523_REG_OP_MODE, mask[idx], 0);

	lp5523_wait_opmode_done();
}

static void lp5523_turn_off_channels(struct lp55xx_chip *chip)
{
	int i;

	for (i = 0; i < LP5523_MAX_LEDS; i++)
		lp55xx_write(chip, LP5523_REG_LED_PWM_BASE + i, 0);
}

static void lp5523_run_engine(struct lp55xx_chip *chip, bool start)
{
	int ret;
	u8 mode;
	u8 exec;

	/* stop engine */
	if (!start) {
		lp5523_stop_engine(chip);
		lp5523_turn_off_channels(chip);
		return;
	}

	/*
	 * To run the engine,
	 * operation mode and enable register should updated at the same time
	 */

	ret = lp55xx_read(chip, LP5523_REG_OP_MODE, &mode);
	if (ret)
		return;

	ret = lp55xx_read(chip, LP5523_REG_ENABLE, &exec);
	if (ret)
		return;

	/* change operation mode to RUN only when each engine is loading */
	if (LP5523_ENG1_IS_LOADING(mode)) {
		mode = (mode & ~LP5523_MODE_ENG1_M) | LP5523_RUN_ENG1;
		exec = (exec & ~LP5523_EXEC_ENG1_M) | LP5523_RUN_ENG1;
	}

	if (LP5523_ENG2_IS_LOADING(mode)) {
		mode = (mode & ~LP5523_MODE_ENG2_M) | LP5523_RUN_ENG2;
		exec = (exec & ~LP5523_EXEC_ENG2_M) | LP5523_RUN_ENG2;
	}

	if (LP5523_ENG3_IS_LOADING(mode)) {
		mode = (mode & ~LP5523_MODE_ENG3_M) | LP5523_RUN_ENG3;
		exec = (exec & ~LP5523_EXEC_ENG3_M) | LP5523_RUN_ENG3;
	}

	lp55xx_write(chip, LP5523_REG_OP_MODE, mode);
	lp5523_wait_opmode_done();

	lp55xx_update_bits(chip, LP5523_REG_ENABLE, LP5523_EXEC_M, exec);
}

static int lp5523_init_program_engine(struct lp55xx_chip *chip)
{
	int i;
	int j;
	int ret;
	u8 status;
	/* one pattern per engine setting LED MUX start and stop addresses */
	static const u8 pattern[][LP5523_PROGRAM_LENGTH] =  {
		{ 0x9c, 0x30, 0x9c, 0xb0, 0x9d, 0x80, 0xd8, 0x00, 0},
		{ 0x9c, 0x40, 0x9c, 0xc0, 0x9d, 0x80, 0xd8, 0x00, 0},
		{ 0x9c, 0x50, 0x9c, 0xd0, 0x9d, 0x80, 0xd8, 0x00, 0},
	};

	/* hardcode 32 bytes of memory for each engine from program memory */
	ret = lp55xx_write(chip, LP5523_REG_CH1_PROG_START, 0x00);
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_CH2_PROG_START, 0x10);
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_CH3_PROG_START, 0x20);
	if (ret)
		return ret;

	/* write LED MUX address space for each engine */
	for (i = LP55XX_ENGINE_1; i <= LP55XX_ENGINE_3; i++) {
		chip->engine_idx = i;
		lp5523_load_engine_and_select_page(chip);

		for (j = 0; j < LP5523_PROGRAM_LENGTH; j++) {
			ret = lp55xx_write(chip, LP5523_REG_PROG_MEM + j,
					pattern[i - 1][j]);
			if (ret)
				goto out;
		}
	}

	lp5523_run_engine(chip, true);

	/* Let the programs run for couple of ms and check the engine status */
	usleep_range(3000, 6000);
	lp55xx_read(chip, LP5523_REG_STATUS, &status);
	status &= LP5523_ENG_STATUS_MASK;

	if (status != LP5523_ENG_STATUS_MASK) {
		dev_err(&chip->cl->dev,
			"cound not configure LED engine, status = 0x%.2x\n",
			status);
		ret = -1;
	}

out:
	lp5523_stop_all_engines(chip);
	return ret;
}

static int lp5523_update_program_memory(struct lp55xx_chip *chip,
					const u8 *data, size_t size)
{
	u8 pattern[LP5523_PROGRAM_LENGTH] = {0};
	unsigned cmd;
	char c[3];
	int nrchars;
	int ret;
	int offset = 0;
	int i = 0;

	while ((offset < size - 1) && (i < LP5523_PROGRAM_LENGTH)) {
		/* separate sscanfs because length is working only for %s */
		ret = sscanf(data + offset, "%2s%n ", c, &nrchars);
		if (ret != 1)
			goto err;

		ret = sscanf(c, "%2x", &cmd);
		if (ret != 1)
			goto err;

		pattern[i] = (u8)cmd;
		offset += nrchars;
		i++;
	}

	/* Each instruction is 16bit long. Check that length is even */
	if (i % 2)
		goto err;

	for (i = 0; i < LP5523_PROGRAM_LENGTH; i++) {
		ret = lp55xx_write(chip, LP5523_REG_PROG_MEM + i, pattern[i]);
		if (ret)
			return -EINVAL;
	}

	return size;

err:
	dev_err(&chip->cl->dev, "wrong pattern format\n");
	return -EINVAL;
}

static void lp5523_firmware_loaded(struct lp55xx_chip *chip)
{
	const struct firmware *fw = chip->fw;

	if (fw->size > LP5523_PROGRAM_LENGTH) {
		dev_err(&chip->cl->dev, "firmware data size overflow: %zu\n",
			fw->size);
		return;
	}

	/*
	 * Program momery sequence
	 *  1) set engine mode to "LOAD"
	 *  2) write firmware data into program memory
	 */

	lp5523_load_engine_and_select_page(chip);
	lp5523_update_program_memory(chip, fw->data, fw->size);
}

static ssize_t show_engine_mode(struct device *dev,
				struct device_attribute *attr,
				char *buf, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	enum lp55xx_engine_mode mode = chip->engines[nr - 1].mode;

	switch (mode) {
	case LP55XX_ENGINE_RUN:
		return sprintf(buf, "run\n");
	case LP55XX_ENGINE_LOAD:
		return sprintf(buf, "load\n");
	case LP55XX_ENGINE_DISABLED:
	default:
		return sprintf(buf, "disabled\n");
	}
}
show_mode(1)
show_mode(2)
show_mode(3)

static ssize_t store_engine_mode(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	struct lp55xx_engine *engine = &chip->engines[nr - 1];

	mutex_lock(&chip->lock);

	chip->engine_idx = nr;

	if (!strncmp(buf, "run", 3)) {
		lp5523_run_engine(chip, true);
		engine->mode = LP55XX_ENGINE_RUN;
	} else if (!strncmp(buf, "load", 4)) {
		lp5523_stop_engine(chip);
		lp5523_load_engine(chip);
		engine->mode = LP55XX_ENGINE_LOAD;
	} else if (!strncmp(buf, "disabled", 8)) {
		lp5523_stop_engine(chip);
		engine->mode = LP55XX_ENGINE_DISABLED;
	}

	mutex_unlock(&chip->lock);

	return len;
}
store_mode(1)
store_mode(2)
store_mode(3)

static int lp5523_mux_parse(const char *buf, u16 *mux, size_t len)
{
	u16 tmp_mux = 0;
	int i;

	len = min_t(int, len, LP5523_MAX_LEDS);

	for (i = 0; i < len; i++) {
		switch (buf[i]) {
		case '1':
			tmp_mux |= (1 << i);
			break;
		case '0':
			break;
		case '\n':
			i = len;
			break;
		default:
			return -1;
		}
	}
	*mux = tmp_mux;

	return 0;
}

static void lp5523_mux_to_array(u16 led_mux, char *array)
{
	int i, pos = 0;
	for (i = 0; i < LP5523_MAX_LEDS; i++)
		pos += sprintf(array + pos, "%x", LED_ACTIVE(led_mux, i));

	array[pos] = '\0';
}

static ssize_t show_engine_leds(struct device *dev,
			    struct device_attribute *attr,
			    char *buf, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	char mux[LP5523_MAX_LEDS + 1];

	lp5523_mux_to_array(chip->engines[nr - 1].led_mux, mux);

	return sprintf(buf, "%s\n", mux);
}
show_leds(1)
show_leds(2)
show_leds(3)

static int lp5523_load_mux(struct lp55xx_chip *chip, u16 mux, int nr)
{
	struct lp55xx_engine *engine = &chip->engines[nr - 1];
	int ret;
	u8 mux_page[] = {
		[LP55XX_ENGINE_1] = LP5523_PAGE_MUX1,
		[LP55XX_ENGINE_2] = LP5523_PAGE_MUX2,
		[LP55XX_ENGINE_3] = LP5523_PAGE_MUX3,
	};

	lp5523_load_engine(chip);

	ret = lp55xx_write(chip, LP5523_REG_PROG_PAGE_SEL, mux_page[nr]);
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_PROG_MEM , (u8)(mux >> 8));
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_PROG_MEM + 1, (u8)(mux));
	if (ret)
		return ret;

	engine->led_mux = mux;
	return 0;
}

static ssize_t store_engine_leds(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	struct lp55xx_engine *engine = &chip->engines[nr - 1];
	u16 mux = 0;
	ssize_t ret;

	if (lp5523_mux_parse(buf, &mux, len))
		return -EINVAL;

	mutex_lock(&chip->lock);

	chip->engine_idx = nr;
	ret = -EINVAL;

	if (engine->mode != LP55XX_ENGINE_LOAD)
		goto leave;

	if (lp5523_load_mux(chip, mux, nr))
		goto leave;

	ret = len;
leave:
	mutex_unlock(&chip->lock);
	return ret;
}
store_leds(1)
store_leds(2)
store_leds(3)

static ssize_t store_engine_load(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	int ret;

	mutex_lock(&chip->lock);

	chip->engine_idx = nr;
	lp5523_load_engine_and_select_page(chip);
	ret = lp5523_update_program_memory(chip, buf, len);

	mutex_unlock(&chip->lock);

	return ret;
}
store_load(1)
store_load(2)
store_load(3)

static ssize_t lp5523_selftest(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	struct lp55xx_platform_data *pdata = chip->pdata;
	int i, ret, pos = 0;
	u8 status, adc, vdd;

	mutex_lock(&chip->lock);

	ret = lp55xx_read(chip, LP5523_REG_STATUS, &status);
	if (ret < 0)
		goto fail;

	/* Check that ext clock is really in use if requested */
	if (pdata->clock_mode == LP55XX_CLOCK_EXT) {
		if  ((status & LP5523_EXT_CLK_USED) == 0)
			goto fail;
	}

	/* Measure VDD (i.e. VBAT) first (channel 16 corresponds to VDD) */
	lp55xx_write(chip, LP5523_REG_LED_TEST_CTRL, LP5523_EN_LEDTEST | 16);
	usleep_range(3000, 6000); /* ADC conversion time is typically 2.7 ms */
	ret = lp55xx_read(chip, LP5523_REG_STATUS, &status);
	if (ret < 0)
		goto fail;

	if (!(status & LP5523_LEDTEST_DONE))
		usleep_range(3000, 6000); /* Was not ready. Wait little bit */

	ret = lp55xx_read(chip, LP5523_REG_LED_TEST_ADC, &vdd);
	if (ret < 0)
		goto fail;

	vdd--;	/* There may be some fluctuation in measurement */

	for (i = 0; i < LP5523_MAX_LEDS; i++) {
		/* Skip non-existing channels */
		if (pdata->led_config[i].led_current == 0)
			continue;

		/* Set default current */
		lp55xx_write(chip, LP5523_REG_LED_CURRENT_BASE + i,
			pdata->led_config[i].led_current);

		lp55xx_write(chip, LP5523_REG_LED_PWM_BASE + i, 0xff);
		/* let current stabilize 2 - 4ms before measurements start */
		usleep_range(2000, 4000);
		lp55xx_write(chip, LP5523_REG_LED_TEST_CTRL,
			     LP5523_EN_LEDTEST | i);
		/* ADC conversion time is 2.7 ms typically */
		usleep_range(3000, 6000);
		ret = lp55xx_read(chip, LP5523_REG_STATUS, &status);
		if (ret < 0)
			goto fail;

		if (!(status & LP5523_LEDTEST_DONE))
			usleep_range(3000, 6000);/* Was not ready. Wait. */

		ret = lp55xx_read(chip, LP5523_REG_LED_TEST_ADC, &adc);
		if (ret < 0)
			goto fail;

		if (adc >= vdd || adc < LP5523_ADC_SHORTCIRC_LIM)
			pos += sprintf(buf + pos, "LED %d FAIL\n", i);

		lp55xx_write(chip, LP5523_REG_LED_PWM_BASE + i, 0x00);

		/* Restore current */
		lp55xx_write(chip, LP5523_REG_LED_CURRENT_BASE + i,
			led->led_current +  50);
		led++;
	}
	if (pos == 0)
		pos = sprintf(buf, "OK\n");
	goto release_lock;
fail:
	pos = sprintf(buf, "FAIL\n");

release_lock:
	mutex_unlock(&chip->lock);

	return pos;
}

static int lp5523_brightness_map(int brightness_value)
{
	int brightness_default = 0;

	if (0 == brightness_value)
	{
		brightness_default = 100;
	}
	if (brightness_value > 100)
	{
		brightness_default = 0xff;
	}

	brightness_default = (brightness_value) * 0xff / 100;

	return brightness_default;
}

static int lp5523_light_device_idx(int dev_idx, int color_idx, int brightness)
{
	u8 msb = 0, lsb = 0;
	int i = 0;
	struct lp55xx_chip *chip_index = NULL;
	int brightness_val[4] = {0};

	if (FIRST_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[FIRST_DEVICE];
		brightness_val[FIRST_DEVICE] = lp5523_brightness_map(brightness);
	}
	if (SECOND_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[SECOND_DEVICE];
		brightness_val[SECOND_DEVICE] = lp5523_brightness_map(brightness);
	}
	 if (THIRD_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[THIRD_DEVICE];
		brightness_val[THIRD_DEVICE] = lp5523_brightness_map(brightness);
	}
	if(FOURTH_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[FOURTH_DEVICE];
		brightness_val[FOURTH_DEVICE] = lp5523_brightness_map(brightness);
	}

	switch(color_idx)
	{
		case COLOR_GREEN:
			msb = 0x00;
			lsb = 0x2A;
			break;
		case COLOR_RED:
			msb = 0x01;
			lsb = 0xC0;
			break;
		case COLOR_BLUE:
			msb = 0x00;
			lsb = 0x15;
	}

	lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_MSB, msb);
	lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB, lsb);
    for (i = 0; i < LP5523_MAX_LEDS; i++)
	{
		lp55xx_write(chip_index, LP5523_REG_LED_CURRENT_BASE + i,0x20);
		lp55xx_write(chip_index, LP5523_REG_LED_PWM_BASE + i, brightness_val[dev_idx]);
	}
	return 0;
}

static int lp5523_stop_device_idx(int dev_idx)
{
	int i = 0;
	struct lp55xx_chip *chip_index = NULL;

	if (FIRST_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[FIRST_DEVICE];
		msb[FIRST_DEVICE] = 0;
		lsb[FIRST_DEVICE] = 0;
	}
	if (SECOND_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[SECOND_DEVICE];
		msb[SECOND_DEVICE] = 0;
		lsb[SECOND_DEVICE] = 0;
	}
	 if (THIRD_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[THIRD_DEVICE];
		msb[THIRD_DEVICE] = 0;
		lsb[THIRD_DEVICE] = 0;
	}
	if(FOURTH_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[FOURTH_DEVICE];
		msb[FOURTH_DEVICE] = 0;
		lsb[FOURTH_DEVICE] = 0;
	}
    for (i = 0; i < LP5523_MAX_LEDS; i++)
	{
		lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_MSB , 0x00);
		lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB , 0x00);
	}
	return 0;
}

static int lp5523_lsb_clean(struct lp55xx_chip *chip_index,int led_idx, int msb, int lsb)
{
	printk("### lp5523_lsb_clean led_idx=%d. msb=%d,lsb=%d\n\n",led_idx, msb, lsb);
	switch(led_idx)
	{
	 case LED_INDEX_THIRD:
	 	//if (0x00 == msb)
	 	{
			if (0x20 == (lsb & 0x20)) // 000001000  111110111
			{
				lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB , lsb & 0xdf);
			}
			if (0x10 == (lsb & 0x10)) // 000010000  11110111
			{
				lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB, lsb & 0xef);
			}
	 	}
		//if (0x01 == msb)
		{
			if ( (0x01 == msb))  //000000001  111110111
			{
				lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_MSB , 0x00);
			}
		}
		break;
	case LED_INDEX_SECOND:
		//if (0x00 == msb)
		{
			if (0x08 == (lsb & 0x08)) // 00010000  11101111
			{
				lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB, lsb & 0xf7);
			}
			if (0x80== (lsb & 0x80)) // 00000001 11111110
			{
				lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB, lsb & 0x7f);
			}
			if (0x04 == (lsb & 0x04)) // 00100000  11011111
			{
				lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB, lsb & 0xfb);
			}
		}
		break;
	case LED_INDEX_FIRST:
		//if (0x00 == msb)
		{
			if (0x02 == (lsb & 0x02)) // 01000000  10111111
			{
				lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB, lsb & 0xfd);
			}
			if (0x01 == (lsb & 0x01)) // 10000000  01111111
			{
				lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB, lsb & 0xfe);
			}
			if (0x40 == (lsb & 0x40)) // 00000010  11111101
			{
				lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB, lsb & 0xbf);
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

static int lp5523_stop_led_idx(int dev_idx, int led_idx)
{
	u8  msb[4] = {0}, lsb[4] = {0};
	struct lp55xx_chip *chip_index = NULL;

	printk("### lp5523_stop_led_idx dev_idx=%d,led_idx=%d. \n",dev_idx, led_idx);
	if (FIRST_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[FIRST_DEVICE];
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_MSB, &msb[FIRST_DEVICE]);
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_LSB, &lsb[FIRST_DEVICE]);
		lp5523_lsb_clean(chip_index, led_idx, msb[FIRST_DEVICE], lsb[FIRST_DEVICE]);
	}
	if (SECOND_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[SECOND_DEVICE];
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_MSB, &msb[SECOND_DEVICE]);
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_LSB, &lsb[SECOND_DEVICE]);
		lp5523_lsb_clean(chip_index, led_idx, msb[SECOND_DEVICE], lsb[SECOND_DEVICE]);
	}
	 if (THIRD_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[THIRD_DEVICE];
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_MSB, &msb[THIRD_DEVICE]);
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_LSB, &lsb[THIRD_DEVICE]);
		lp5523_lsb_clean(chip_index, led_idx, msb[THIRD_DEVICE], lsb[THIRD_DEVICE]);
	}
	if(FOURTH_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[FOURTH_DEVICE];
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_MSB, &msb[FOURTH_DEVICE]);
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_LSB, &lsb[FOURTH_DEVICE]);
		lp5523_lsb_clean(chip_index, led_idx, msb[FOURTH_DEVICE], lsb[FOURTH_DEVICE]);
	}

	return 0;
}

static int lp5523_msb_lsb_map(int led_idx,int color_idx, u8 msb, u8 lsb, u8 *msb_value, u8 *lsb_value)
{
	printk("### lp5523_msb_lsb_map msb = %d, lsb = %d\n\n",msb, lsb);
	switch(color_idx)
	{
		 case COLOR_GREEN:
		 	if (LED_INDEX_THIRD == led_idx)
		 	{
		 		msb = 0x00 | msb;
				lsb = 0x20 | lsb;
		 	}
			if (LED_INDEX_SECOND == led_idx)
		 	{
		 		msb = 0x00 | msb;
				lsb = 0x08 | lsb;
		 	}
			if (LED_INDEX_FIRST == led_idx)
		 	{
		 		msb = 0x00 | msb;
				lsb = 0x02 | lsb;
		 	}
			break;
		case COLOR_BLUE:
		 	if (LED_INDEX_THIRD == led_idx)
		 	{
		 		msb = 0x00 | msb;
				lsb = 0x10 | lsb;
		 	}
			if (LED_INDEX_SECOND == led_idx)
		 	{
		 		msb = 0x00 | msb;
				lsb = 0x04 | lsb;
		 	}
			if (LED_INDEX_FIRST == led_idx)
		 	{
		 		msb = 0x00 | msb;
				lsb = 0x01 | lsb;
		 	}
			break;
		case COLOR_RED:
		 	if (LED_INDEX_THIRD == led_idx)
		 	{
		 		msb = 0x01 | msb;
				lsb = 0x00 | lsb;
		 	}
			if (LED_INDEX_SECOND == led_idx)
		 	{
		 		msb = 0x00 | msb;
				lsb = 0x80 | lsb;
		 	}
			if (LED_INDEX_FIRST == led_idx)
		 	{
		 		msb = 0x00 | msb;
				lsb = 0x40 | lsb;
		 	}
			break;
	}

	*msb_value = msb;
	*lsb_value = lsb;
	printk("### lp5523_msb_lsb_map *msb_value = %d, *lsb_value = %d\n\n",*msb_value, *lsb_value);
	return 0;
}

static struct lp55xx_chip * lp5523_chipMap_ReadMsbLsb(int dev_idx, int brightness, int *bright_final)
{
	struct lp55xx_chip *chip_index = NULL;
	int brightness_temp[4] = {0};

	printk("### lp5523_chipMap_ReadMsbLsb dev_idex = %d\n\n", dev_idx);

	if (FIRST_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[FIRST_DEVICE];
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_MSB, &msb[FIRST_DEVICE]);
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_LSB, &lsb[FIRST_DEVICE]);
		brightness_temp[FIRST_DEVICE] = lp5523_brightness_map(brightness);
		printk("### lp5523_chipMap_ReadMsbLsb msb[0] = %d,lsb[0]:%d\n\n", msb[FIRST_DEVICE],lsb[FIRST_DEVICE]);
	}
	if (SECOND_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[SECOND_DEVICE];
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_MSB, &msb[SECOND_DEVICE]);
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_LSB, &lsb[SECOND_DEVICE]);
		brightness_temp[SECOND_DEVICE] = lp5523_brightness_map(brightness);
		printk("### lp5523_chipMap_ReadMsbLsb msb[0] = %d,lsb[0]:%d\n\n", msb[SECOND_DEVICE],lsb[SECOND_DEVICE]);
	}
	if (THIRD_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[THIRD_DEVICE];
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_MSB, &msb[THIRD_DEVICE]);
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_LSB, &lsb[THIRD_DEVICE]);
		brightness_temp[THIRD_DEVICE] = lp5523_brightness_map(brightness);
		printk("### lp5523_chipMap_ReadMsbLsb msb[0] = %d,lsb[0]:%d\n\n", msb[THIRD_DEVICE],lsb[THIRD_DEVICE]);
	}
	if(FOURTH_DEVICE == dev_idx)
	{
		chip_index = lp5523_chip[FOURTH_DEVICE];
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_MSB, &msb[FOURTH_DEVICE]);
		lp55xx_read(chip_index, LP5523_REG_ENABLE_LEDS_LSB, &lsb[FOURTH_DEVICE]);
		brightness_temp[FOURTH_DEVICE] = lp5523_brightness_map(brightness);
		printk("### lp5523_chipMap_ReadMsbLsb msb[0] = %d,lsb[0]:%d\n\n", msb[FOURTH_DEVICE],lsb[FOURTH_DEVICE]);
	}
	*bright_final = brightness_temp[dev_idx];
	printk("### lp5523_chipMap_ReadMsbLsb dev_idex = %d\n\n", dev_idx);
	return chip_index;
}


static int lp5523_light_led_idx(int dev_idx, int color_idx, int led_idx, int bright_value)
{
	u8  msb_temp = 0, lsb_temp = 0, msb_w = 0, lsb_w = 0;
	int i = 0;
	struct lp55xx_chip *chip_index = NULL;
	int bright_temp = 0;

	chip_index = lp5523_chipMap_ReadMsbLsb(dev_idx,bright_value,&bright_temp);
	switch(dev_idx)
	{
		case FIRST_DEVICE:
			msb_temp = msb[FIRST_DEVICE];
			lsb_temp = lsb[FIRST_DEVICE];
			break;
		case SECOND_DEVICE:
			msb_temp = msb[SECOND_DEVICE];
			lsb_temp = lsb[SECOND_DEVICE];
			break;
		case THIRD_DEVICE:
			msb_temp = msb[THIRD_DEVICE];
			lsb_temp = lsb[THIRD_DEVICE];
			break;
		case FOURTH_DEVICE:
			msb_temp = msb[FOURTH_DEVICE];
			lsb_temp = lsb[FOURTH_DEVICE];
			break;
		default:
			break;
	}

	printk("### lp5523_light_led_idx msb_temp:%d, lsb_temp:%d\n", msb_temp,msb_temp);
	lp5523_msb_lsb_map(led_idx,color_idx,msb_temp,lsb_temp, &msb_w, &lsb_w);
	printk("### lp5523_light_led_idx msb_w:%d, lsb_w:%d, bright_temp=%d\n", msb_w,lsb_w,bright_temp);
	lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_MSB, msb_w);
	lp55xx_write(chip_index, LP5523_REG_ENABLE_LEDS_LSB, lsb_w);
        for (i = 0; i < LP5523_MAX_LEDS; i++)
	{
		lp55xx_write(chip_index, LP5523_REG_LED_CURRENT_BASE + i,0x20);
		lp55xx_write(chip_index, LP5523_REG_LED_PWM_BASE + i, bright_temp);
	}
	return 0;
}


static int lp5523_light(int device_index, int color_index, int led_idx,int singal_all_led, int brightness_value)
{
	int cor_idx = color_index;
	
	printk("#### lp5523_light device_index:%d, cor_idx:%d, led_idx=%d, brightness_value=%d\n\n",device_index,cor_idx, led_idx,brightness_value );
	if (singal_all_led)
	{
		lp5523_light_device_idx(device_index,cor_idx,brightness_value);
	}
	else
	{
		lp5523_light_led_idx(device_index,cor_idx, led_idx,brightness_value);
	}
	return 0;
}

static int lp5523_stop(int device_index,int led_idx,int singal_all_led)
{
	if (singal_all_led)
	{
		lp5523_stop_device_idx(device_index);
	}
	else
	{
		lp5523_stop_led_idx(device_index, led_idx);
	}
	return 0;
}

static int lp5523_open(struct inode *inode, struct file *file)
{
	led_task_flag = false;
	if (led_task)
	{
		kthread_stop(led_task);
		led_task = NULL;
	}
    file->private_data = lp5523_i2c_client;
	if (file->private_data == NULL)
    {
	    return 0;
	}
	return nonseekable_open(inode, file);
}

static int lp5523_release(struct inode *inode, struct file *file)
{
   file->private_data = NULL;
        return 0;
}

static long lp5523_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int device_idx = (arg & 0xf000) >> 12;
	int color_index = (arg & 0x0f00) >> 8;
	int led_idx = arg & 0xf;
	int singal_all_led = (arg & 0xf0) >> 4;
	int brightness_value = (arg & 0xff0000) >> 16;
	
	printk("### lp5523_ioctl cmd = %d, device_idx:%d, cloor_index=%d,led_idx=%d,singal_all_led=%d, brightness_value=%d\n\n",
		cmd,device_idx, color_index,led_idx,singal_all_led,brightness_value);
	
	if (LP5523_LED_ON == cmd)
	{
		lp5523_light(device_idx,color_index,led_idx,singal_all_led,brightness_value);
	}
	else
	{
		lp5523_stop(device_idx,led_idx,singal_all_led);
	}
    return 0;
}

static const struct file_operations lp5523_fops = {
	.owner    = THIS_MODULE,
	.open     = lp5523_open,
	.release  = lp5523_release,
	.unlocked_ioctl = lp5523_ioctl,
};

static struct miscdevice lp5523_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "ti-led",
    .fops  = &lp5523_fops,
};

#define show_fader(nr)						\
static ssize_t show_master_fader##nr(struct device *dev,	\
			    struct device_attribute *attr,	\
			    char *buf)				\
{								\
	return show_master_fader(dev, attr, buf, nr);		\
}

#define store_fader(nr)						\
static ssize_t store_master_fader##nr(struct device *dev,	\
			     struct device_attribute *attr,	\
			     const char *buf, size_t len)	\
{								\
	return store_master_fader(dev, attr, buf, len, nr);	\
}

static ssize_t show_master_fader(struct device *dev,
				 struct device_attribute *attr,
				 char *buf, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	int ret;
	u8 val;

	mutex_lock(&chip->lock);
	ret = lp55xx_read(chip, LP5523_REG_MASTER_FADER_BASE + nr - 1, &val);
	mutex_unlock(&chip->lock);

	if (ret == 0)
		ret = sprintf(buf, "%u\n", val);

	return ret;
}
show_fader(1)
show_fader(2)
show_fader(3)

static ssize_t store_master_fader(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	int ret;
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val > 0xff)
		return -EINVAL;

	mutex_lock(&chip->lock);
	ret = lp55xx_write(chip, LP5523_REG_MASTER_FADER_BASE + nr - 1,
			   (u8)val);
	mutex_unlock(&chip->lock);

	if (ret == 0)
		ret = len;

	return ret;
}
store_fader(1)
store_fader(2)
store_fader(3)

static ssize_t show_master_fader_leds(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	int i, ret, pos = 0;
	u8 val;

	mutex_lock(&chip->lock);

	for (i = 0; i < LP5523_MAX_LEDS; i++) {
		ret = lp55xx_read(chip, LP5523_REG_LED_CTRL_BASE + i, &val);
		if (ret)
			goto leave;

		val = (val & LP5523_FADER_MAPPING_MASK)
			>> LP5523_FADER_MAPPING_SHIFT;
		if (val > 3) {
			ret = -EINVAL;
			goto leave;
		}
		buf[pos++] = val + '0';
	}
	buf[pos++] = '\n';
	ret = pos;
leave:
	mutex_unlock(&chip->lock);
	return ret;
}

static ssize_t store_master_fader_leds(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	int i, n, ret;
	u8 val;

	n = min_t(int, len, LP5523_MAX_LEDS);

	mutex_lock(&chip->lock);

	for (i = 0; i < n; i++) {
		if (buf[i] >= '0' && buf[i] <= '3') {
			val = (buf[i] - '0') << LP5523_FADER_MAPPING_SHIFT;
			ret = lp55xx_update_bits(chip,
						 LP5523_REG_LED_CTRL_BASE + i,
						 LP5523_FADER_MAPPING_MASK,
						 val);
			if (ret)
				goto leave;
		} else {
			ret = -EINVAL;
			goto leave;
		}
	}
	ret = len;
leave:
	mutex_unlock(&chip->lock);
	return ret;
}

static void lp5523_led_brightness_work(struct work_struct *work)
{
	struct lp55xx_led *led = container_of(work, struct lp55xx_led,
					      brightness_work);
	struct lp55xx_chip *chip = led->chip;

	mutex_lock(&chip->lock);
	lp55xx_write(chip, LP5523_REG_LED_PWM_BASE + led->chan_nr,
		     led->brightness);
	mutex_unlock(&chip->lock);
}

static LP55XX_DEV_ATTR_RW(engine1_mode, show_engine1_mode, store_engine1_mode);
static LP55XX_DEV_ATTR_RW(engine2_mode, show_engine2_mode, store_engine2_mode);
static LP55XX_DEV_ATTR_RW(engine3_mode, show_engine3_mode, store_engine3_mode);
static LP55XX_DEV_ATTR_RW(engine1_leds, show_engine1_leds, store_engine1_leds);
static LP55XX_DEV_ATTR_RW(engine2_leds, show_engine2_leds, store_engine2_leds);
static LP55XX_DEV_ATTR_RW(engine3_leds, show_engine3_leds, store_engine3_leds);
static LP55XX_DEV_ATTR_WO(engine1_load, store_engine1_load);
static LP55XX_DEV_ATTR_WO(engine2_load, store_engine2_load);
static LP55XX_DEV_ATTR_WO(engine3_load, store_engine3_load);
static LP55XX_DEV_ATTR_RO(selftest, lp5523_selftest);
static LP55XX_DEV_ATTR_RW(master_fader1, show_master_fader1,
			  store_master_fader1);
static LP55XX_DEV_ATTR_RW(master_fader2, show_master_fader2,
			  store_master_fader2);
static LP55XX_DEV_ATTR_RW(master_fader3, show_master_fader3,
			  store_master_fader3);
static LP55XX_DEV_ATTR_RW(master_fader_leds, show_master_fader_leds,
			  store_master_fader_leds);

static struct attribute *lp5523_attributes[] = {
	&dev_attr_engine1_mode.attr,
	&dev_attr_engine2_mode.attr,
	&dev_attr_engine3_mode.attr,
	&dev_attr_engine1_load.attr,
	&dev_attr_engine2_load.attr,
	&dev_attr_engine3_load.attr,
	&dev_attr_engine1_leds.attr,
	&dev_attr_engine2_leds.attr,
	&dev_attr_engine3_leds.attr,
	&dev_attr_selftest.attr,
	&dev_attr_master_fader1.attr,
	&dev_attr_master_fader2.attr,
	&dev_attr_master_fader3.attr,
	&dev_attr_master_fader_leds.attr,
	NULL,
};

static const struct attribute_group lp5523_group = {
	.attrs = lp5523_attributes,
};

/* Chip specific configurations */
static struct lp55xx_device_config lp5523_cfg = {
	.reset = {
		.addr = LP5523_REG_RESET,
		.val  = LP5523_RESET,
	},
	.enable = {
		.addr = LP5523_REG_ENABLE,
		.val  = LP5523_ENABLE,
	},
	.max_channel  = LP5523_MAX_LEDS,
	.post_init_device   = lp5523_post_init_device,
	.brightness_work_fn = lp5523_led_brightness_work,
	.set_led_current    = lp5523_set_led_current,
	.firmware_cb        = lp5523_firmware_loaded,
	.run_engine         = lp5523_run_engine,
	.dev_attr_group     = &lp5523_group,
};

static int lp5523_led_thread(void *data)
{
	int i = 0 ;

	for (i = 0; i < ALL_DEVICE; i++)
	{
		lp5523_stop_device_idx(i);
	}
	while(led_task_flag)
	{
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (kthread_should_stop())
			break;
		for (i = 0 ; i < ALL_DEVICE; i++)
		{
			lp5523_light_device_idx(i, COLOR_BLUE, 200);
			usleep_range(200000, 200000);
			lp5523_stop_device_idx(i);
		}
	}
	return 0;
}

static int lp5523_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct lp55xx_chip *chip;
	struct lp55xx_led *led;
	struct lp55xx_platform_data *pdata = dev_get_platdata(&client->dev);
	struct device_node *np = client->dev.of_node;
	lp5523_probe_nums++;

	if (!pdata) {
		if (np) {
			pdata = lp55xx_of_populate_pdata(&client->dev, np);
			if (IS_ERR(pdata))
				return PTR_ERR(pdata);
		} else {
			dev_err(&client->dev, "no platform data\n");
			return -EINVAL;
		}
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	led = devm_kzalloc(&client->dev,
			sizeof(*led) * pdata->num_channels, GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	chip->cl = client;
	chip->pdata = pdata;
	chip->cfg = &lp5523_cfg;

	lp5523_client[lp5523_probe_nums] = client;
	lp5523_chip[lp5523_probe_nums] = chip;
	mutex_init(&chip->lock);

	i2c_set_clientdata(client, led);

    lp5523_i2c_client = client; //for ioctl 
	ret = lp55xx_init_device(chip);
	if (ret)
		goto err_init;

	dev_info(&client->dev, "%s Programmable led chip found\n", id->name);

	ret = lp55xx_register_leds(led, chip);
	if (ret)
		goto err_register_leds;
	lp55xx_write(chip, LP5523_REG_ENABLE_LEDS_MSB, 0);
	lp55xx_write(chip, LP5523_REG_ENABLE_LEDS_LSB, 0);

	ret = lp55xx_register_sysfs(chip);
	if (ret) {
		dev_err(&client->dev, "registering sysfs failed\n");
		goto err_register_sysfs;
	}

    if (1 == lp5523_probe_nums ) //for ioctl
    {
	    ret = misc_register(&lp5523_device);
	    if(ret)
		{
	        goto err_misc_deregister;
	    }
	}
	if (3 == lp5523_probe_nums)
	{
		led_task = kthread_run(lp5523_led_thread, NULL, "LED_THREAD");
		if (IS_ERR(led_task))
		{
			dev_err(&client->dev, "kthread_run failed\n");
		}
	}
	return 0;
err_misc_deregister:
	misc_deregister(&lp5523_device);

err_register_sysfs:
	lp55xx_unregister_leds(led, chip);
err_register_leds:
	lp55xx_deinit_device(chip);
err_init:
	return ret;
}

static int lp5523_remove(struct i2c_client *client)
{
	struct lp55xx_led *led = i2c_get_clientdata(client);
	struct lp55xx_chip *chip = led->chip;

	lp5523_stop_all_engines(chip);
	lp55xx_unregister_sysfs(chip);
	lp55xx_unregister_leds(led, chip);
	lp55xx_deinit_device(chip);

	return 0;
}

static const struct i2c_device_id lp5523_id[] = {
	{ "lp5523",  LP5523 },
	{ "lp55231", LP55231 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, lp5523_id);

#ifdef CONFIG_OF
static const struct of_device_id of_lp5523_leds_match[] = {
	{ .compatible = "national,lp5523", },
	{ .compatible = "ti,lp55231", },
	{},
};

MODULE_DEVICE_TABLE(of, of_lp5523_leds_match);
#endif

static struct i2c_driver lp5523_driver = {
	.driver = {
		.name	= "lp5523x",
		.of_match_table = of_match_ptr(of_lp5523_leds_match),
	},
	.probe		= lp5523_probe,
	.remove		= lp5523_remove,
	.id_table	= lp5523_id,
};

module_i2c_driver(lp5523_driver);

MODULE_AUTHOR("Mathias Nyman <mathias.nyman@nokia.com>");
MODULE_AUTHOR("Milo Kim <milo.kim@ti.com>");
MODULE_DESCRIPTION("LP5523 LED engine");
MODULE_LICENSE("GPL");
