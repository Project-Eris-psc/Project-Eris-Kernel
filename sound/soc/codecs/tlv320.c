/*
 * Codec TLV320ADC3101 driver
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_ARCH_SUN8IW5
#include <mach/sys_config.h>
#include <mach/gpio.h>
#include <linux/pinctrl/pinconf-sunxi.h>
#endif

enum {
    LOG_WARNING  = 1U << 0,
    LOG_DEBUG = 1U << 1,
    LOG_VERBOSE = 1U << 2
};

static u32 gDebugMask = LOG_WARNING | LOG_DEBUG;

#define print_vdbg(format, arg...)    \
({  \
    if (gDebugMask & LOG_VERBOSE)  \
        printk(format, ##arg); \
})

#define print_ddbg(format, arg...)    \
({  \
    if (gDebugMask & LOG_DEBUG)  \
        printk(format, ##arg); \
})

#define print_wdbg(format, arg...)    \
({  \
    if (gDebugMask & LOG_WARNING)  \
        printk(format, ##arg); \
})

#define TLV320_I2C_CHANNEL_ID         0
#define TLV320_I2C_MSG_LENGTH_MAX     8
#if 1
#define TLV320_I2C_ADC0_ADDR        0x18
#define TLV320_I2C_ADC1_ADDR        0x19
#define TLV320_I2C_ADC2_ADDR        0x1A
#define TLV320_I2C_ADC3_ADDR        0x1B
#endif
//PGA Gain. Increments in 0.5dB steps.
#define TLV320_PGA_GAIN_MAX        80
#if 1
#define TLV320_PGA_GAIN_CH1_DEF    58
#define TLV320_PGA_GAIN_CH2_DEF    58
#define TLV320_PGA_GAIN_CH3_DEF    58
#define TLV320_PGA_GAIN_CH4_DEF    58
#define TLV320_PGA_GAIN_CH5_DEF    58
#define TLV320_PGA_GAIN_CH6_DEF    58
#define TLV320_PGA_GAIN_CH7_DEF    12
#define TLV320_PGA_GAIN_CH8_DEF    12
#endif
#define TLV320_REG_NUM_MAX          128

static struct workqueue_struct *tlv320_queue = NULL;
static struct work_struct   tlv320_init_work;
static int    tlv320_init_done = 0;

static int i2c_err_statistic = 0;
module_param(i2c_err_statistic, int, 0444);
module_param(gDebugMask, int, 0644);
module_param(tlv320_init_done, int, 0444);

enum {
    TLV320_I2C_CLIENT_ADC0 = 0,
    TLV320_I2C_CLIENT_ADC1,
    TLV320_I2C_CLIENT_ADC2,
    TLV320_I2C_CLIENT_ADC3,
    TLV320_I2C_CLIENT_NUM
};

enum {
    TLV320_PGA_CH1 = 0,
    TLV320_PGA_CH2,
    TLV320_PGA_CH3,
    TLV320_PGA_CH4,
    TLV320_PGA_CH5,
    TLV320_PGA_CH6,
    TLV320_PGA_CH7,
    TLV320_PGA_CH8,
    TLV320_PGA_NUM
};

struct tlv320_i2c_config {
    int client_id;
    unsigned char msg_buf[TLV320_I2C_MSG_LENGTH_MAX];
    int msg_len;
    int sleepms;
};

static struct tlv320_i2c_config tlv320_i2c_pga_configs[TLV320_PGA_NUM] = {
    {TLV320_I2C_CLIENT_ADC0 ,{59, TLV320_PGA_GAIN_CH1_DEF}, 2, 0},
    {TLV320_I2C_CLIENT_ADC0 ,{60, TLV320_PGA_GAIN_CH2_DEF}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1 ,{59, TLV320_PGA_GAIN_CH3_DEF}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1 ,{60, TLV320_PGA_GAIN_CH4_DEF}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2 ,{59, TLV320_PGA_GAIN_CH5_DEF}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2 ,{60, TLV320_PGA_GAIN_CH6_DEF}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3 ,{59, TLV320_PGA_GAIN_CH7_DEF}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3 ,{60, TLV320_PGA_GAIN_CH8_DEF}, 2, 0},
};

static const struct tlv320_i2c_config tlv320_i2c_codec_reset_configs[] = {
    /* {client id, {reg no, ...}, length, sleep(ms) } */
    /* Need select page first, page 0 */
    {TLV320_I2C_CLIENT_ADC0, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {0,0}, 2, 0},
    /* software reset */
    {TLV320_I2C_CLIENT_ADC0, {1,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {1,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {1,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {1,1}, 2, 0},
};

static struct i2c_client *tlv320_clients[TLV320_I2C_CLIENT_NUM] = {0};

static const struct tlv320_i2c_config tlv320_i2c_configs[] = {
    /* {client id, {reg no, ...}, length, sleep(ms) } */
    /* Need select page first, page 0 */
    {TLV320_I2C_CLIENT_ADC0, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {0,0}, 2, 0},
    /* software reset */
    {TLV320_I2C_CLIENT_ADC0, {1,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {1,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {1,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {1,1}, 2, 0},

    /* Clock-gen multiplexing */
    /* PLL_CLK_IN*R*K/P =12*1*6.8267/1 = 81.9204 */
    {TLV320_I2C_CLIENT_ADC0, {4,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {4,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {4,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {4,1}, 2, 0},

    /* NADC MADC AOSR device 0 */
    /* msg_buf[1] = 0x14; Divide-By for NADC = 20 -->4.096; DISABLED */
    /* msg_buf[2] = 0x80|0x02; Divide-By for MADC = 2-->2.048; Enabled */
    /* msg_buf[3] = 0x80; AOSR = 128 (default) */
    {TLV320_I2C_CLIENT_ADC0, {18,1,130,128}, 4, 0},
    {TLV320_I2C_CLIENT_ADC1, {18,1,130,128}, 4, 0},
    {TLV320_I2C_CLIENT_ADC2, {18,1,130,128}, 4, 0},
    {TLV320_I2C_CLIENT_ADC3, {18,1,130,128}, 4, 0},

    /* ADC audio interface control 1 */
    /* msg_buf[1] = 0x6D; DSP  24BIT */
    /* msg_buf[3] = 0x0E; BDIV = ADC_CLK */
    /* msg_buf[4] = 0x81; Source of BCLK is ADC_CLK; Divide-By = 1 16 */
    /*ADC DSP  slaver */
    {TLV320_I2C_CLIENT_ADC0, {27,65,0,14}, 4, 0},
    {TLV320_I2C_CLIENT_ADC1, {27,65,0,14}, 4, 0},
    {TLV320_I2C_CLIENT_ADC2, {27,65,0,14}, 4, 0},
    {TLV320_I2C_CLIENT_ADC3, {27,65,0,14}, 4, 0},

    {TLV320_I2C_CLIENT_ADC0, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {0,0}, 2, 0},

    {TLV320_I2C_CLIENT_ADC0, {53,16}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {53,16}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {53,16}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {53,16}, 2, 0},

    {TLV320_I2C_CLIENT_ADC0, {81,194,0}, 3, 0},
    {TLV320_I2C_CLIENT_ADC1, {81,194,0}, 3, 0},
    {TLV320_I2C_CLIENT_ADC2, {81,194,0}, 3, 0},
    {TLV320_I2C_CLIENT_ADC3, {81,194,0}, 3, 0},//66

    {TLV320_I2C_CLIENT_ADC0, {83,0,0,0}, 4, 0},
    {TLV320_I2C_CLIENT_ADC1, {83,0,0,0}, 4, 0},
    {TLV320_I2C_CLIENT_ADC2, {83,0,0,0}, 4, 0},
    {TLV320_I2C_CLIENT_ADC3, {83,0,0,0}, 4, 0},

    {TLV320_I2C_CLIENT_ADC0, {0,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {0,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {0,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {0,1}, 2, 0},

    {TLV320_I2C_CLIENT_ADC0, {51,120,63}, 3, 0},
    {TLV320_I2C_CLIENT_ADC1, {51,120,63}, 3, 0},
    {TLV320_I2C_CLIENT_ADC2, {51,120,63}, 3, 0},
    {TLV320_I2C_CLIENT_ADC3, {51,120,63}, 3, 0},

    {TLV320_I2C_CLIENT_ADC0, {54,63,63}, 3, 0},
    {TLV320_I2C_CLIENT_ADC1, {54,63,63}, 3, 0},
    {TLV320_I2C_CLIENT_ADC2, {54,63,63}, 3, 0},
    {TLV320_I2C_CLIENT_ADC3, {54,63,63}, 3, 0},

    {TLV320_I2C_CLIENT_ADC0, {57,63}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {57,63}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {57,63}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {57,63}, 2, 0},

    {TLV320_I2C_CLIENT_ADC0, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {0,0}, 2, 0},

    {TLV320_I2C_CLIENT_ADC0, {61,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {61,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {61,1}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {61,1}, 2, 0},

    {TLV320_I2C_CLIENT_ADC0, {18,129}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {18,129}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {18,129}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {18,129}, 2, 100},

    {TLV320_I2C_CLIENT_ADC0, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {0,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {0,0}, 2, 0},

    {TLV320_I2C_CLIENT_ADC0, {37,16,03}, 3, 0},
    {TLV320_I2C_CLIENT_ADC1, {37,16,03}, 3, 0},
    {TLV320_I2C_CLIENT_ADC2, {37,16,03}, 3, 0},
    {TLV320_I2C_CLIENT_ADC3, {37,16,03}, 3, 0},

    {TLV320_I2C_CLIENT_ADC0, {28,0}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {28,64}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {28,128}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {28,192}, 2, 0},

    {TLV320_I2C_CLIENT_ADC0, {53,18}, 2, 0},
    {TLV320_I2C_CLIENT_ADC1, {53,18}, 2, 0},
    {TLV320_I2C_CLIENT_ADC2, {53,18}, 2, 0},
    {TLV320_I2C_CLIENT_ADC3, {53,18}, 2, 0}
};

static int tlv320_i2c_config_send(const struct tlv320_i2c_config *config) {
    struct i2c_client *client;
    int ret, err = 0;

    client = tlv320_clients[config->client_id];
    if (!client) {
        print_wdbg("tlv320_clients[%d] is NULL!!!\n", config->client_id);
        return -1;
    }

    //print_ddbg("tlv320_i2c_config_send(), msg_buf[%d, 0x%02x, ...], len=%d\n", config->msg_buf[0], config->msg_buf[1], config->msg_len);
    ret = i2c_master_send(client, config->msg_buf, config->msg_len);
    if (ret != config->msg_len) {
        print_ddbg("send tlv320_i2c_config_send(), return err:%d\n", ret);
        i2c_err_statistic++;
        err = ret;
    }
    if (config->sleepms > 0) {
        print_ddbg("send tlv320_i2c_config_send(), sleep %d(ms)\n", config->sleepms);
        msleep(config->sleepms);
    }

    return err;
}

static int tlv320_i2c_pga_config_send(const struct tlv320_i2c_config *config) {
    struct tlv320_i2c_config    page_config = {
        .msg_buf = {0, 0x1},
        .msg_len = 2,
    };

    if (!tlv320_init_done) {
        print_ddbg("tlv320 has not been initialized!\n");
        return -1;
    }

    // Select page 1 first
    page_config.client_id = config->client_id;
    tlv320_i2c_config_send(&page_config);

    tlv320_i2c_config_send(config);

    return 0;
}

static void tlv320_i2c_init_work_func(struct work_struct *work)
{
    int i;

    print_ddbg("tlv320_i2c_init_work_func() begin\n");
    for(i = 0; i < sizeof(tlv320_i2c_configs)/sizeof(tlv320_i2c_configs[0]);i++) {
        tlv320_i2c_config_send(&tlv320_i2c_configs[i]);
    }
    tlv320_init_done = 1;

    for(i = 0; i < sizeof(tlv320_i2c_pga_configs)/sizeof(tlv320_i2c_pga_configs[0]);i++) {
        tlv320_i2c_pga_config_send(&tlv320_i2c_pga_configs[i]);
    }
    print_ddbg("tlv320_i2c_init_work_func() end\n");

    return;
}

static int tlv320_codec_startup(struct snd_pcm_substream *substream,
    struct snd_soc_dai *dai)
{
    int i;

    print_ddbg("tlv320_codec_startup()\n");
    tlv320_init_done = 0;
    for(i = 0; i < sizeof(tlv320_i2c_codec_reset_configs)/sizeof(tlv320_i2c_codec_reset_configs[0]);i++) {
        tlv320_i2c_config_send(&tlv320_i2c_codec_reset_configs[i]);
    }

    return 0;
}

static void tlv320_codec_shutdown(struct snd_pcm_substream *substream,
    struct snd_soc_dai *dai)
{
    int i;

    print_ddbg("tlv320_codec_shutdown()\n");
    tlv320_init_done = 0;
    for(i = 0; i < sizeof(tlv320_i2c_codec_reset_configs)/sizeof(tlv320_i2c_codec_reset_configs[0]);i++) {
        tlv320_i2c_config_send(&tlv320_i2c_codec_reset_configs[i]);
    }
}

static int tlv320_codec_hw_params(struct snd_pcm_substream *substream,
                struct snd_pcm_hw_params *params,
                struct snd_soc_dai *dai)
{
    print_ddbg("tlv320_codec_hw_params()\n");
    return 0;
}

static int tlv320_codec_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
    return 0;
}

static int tlv320_codec_set_sysclk(struct snd_soc_dai *dai,
                 int clk_id, unsigned int freq, int dir)
{
    return 0;
}

static int tlv320_codec_set_clkdiv(struct snd_soc_dai *dai,
                 int div_id, int div)
{
     return 0;
}


static int tlv320_codec_trigger(struct snd_pcm_substream *substream,
                              int cmd, struct snd_soc_dai *dai)
{
    print_ddbg("sunxi_i2s0_trigger(), cmd=%d\n", cmd);
    if (cmd == SNDRV_PCM_TRIGGER_START) {
        queue_work(tlv320_queue, &tlv320_init_work);
    }

    return 0;
}

static const struct snd_soc_dai_ops tlv320_codec_dai_ops = {
    .trigger    = tlv320_codec_trigger,
    .hw_params     = tlv320_codec_hw_params,
    .set_fmt     = tlv320_codec_set_fmt,
    .set_sysclk = tlv320_codec_set_sysclk,
    .set_clkdiv = tlv320_codec_set_clkdiv,
    .startup     = tlv320_codec_startup,
    .shutdown     = tlv320_codec_shutdown,
};

static struct snd_soc_dai_driver tlv320_codec_dai0 = {
    .name = "tlv320-pcm0",
    /* Capture capabilities */
    .capture = {
        .stream_name = "Capture",
        .channels_min = 2,
        //.channels_min = 2,
        .channels_max = 8,
        //.channels_max = 2,
        .rates = SNDRV_PCM_RATE_64000,
//        .rates = SNDRV_PCM_RATE_16000,
        .formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE,
    },
    /* pcm operations */
    .ops = &tlv320_codec_dai_ops,
};

static int tlv320_i2c_probe(struct i2c_client *i2c,
                      const struct i2c_device_id *i2c_id)
{
    print_ddbg("i2c_id number :%ld\n",i2c_id->driver_data);
    tlv320_clients[i2c_id->driver_data] = i2c;

    return 0;
}

static int tlv320_i2c_remove(struct i2c_client *i2c)
{
    int i;

    for (i=0; i<TLV320_I2C_CLIENT_NUM; i++) {
        if (tlv320_clients[i] == i2c) {
            tlv320_clients[i] = NULL;
            break;
        }
    }

    return 0;
}

static struct i2c_board_info tlv320_i2c_board_info[] = {
    {I2C_BOARD_INFO("tlv320_0", TLV320_I2C_ADC0_ADDR), },
    {I2C_BOARD_INFO("tlv320_1", TLV320_I2C_ADC1_ADDR), },
    {I2C_BOARD_INFO("tlv320_2", TLV320_I2C_ADC2_ADDR), },
    {I2C_BOARD_INFO("tlv320_3", TLV320_I2C_ADC3_ADDR), },
};

static const struct i2c_device_id tlv320_i2c_id[] = {
    { "tlv320_0", TLV320_I2C_CLIENT_ADC0 },
    { "tlv320_1", TLV320_I2C_CLIENT_ADC1 },
    { "tlv320_2", TLV320_I2C_CLIENT_ADC2 },
    { "tlv320_3", TLV320_I2C_CLIENT_ADC3 },
    {}
};
MODULE_DEVICE_TABLE(i2c, tlv320_i2c_id);

static struct i2c_driver tlv320_i2c_driver = {
    .driver = {
        .name = "tlv320-i2c",
        .owner = THIS_MODULE,
    },
    .probe = tlv320_i2c_probe,
    .remove = tlv320_i2c_remove,
    .id_table = tlv320_i2c_id,
};

static const DECLARE_TLV_DB_SCALE(tlv320_pga_setting_tlv, 0, 2, 0);

static int tlv320_pga1_setting_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    tlv320_i2c_pga_configs[TLV320_PGA_CH1].msg_buf[1] = ucontrol->value.integer.value[0];
    tlv320_i2c_pga_config_send(&tlv320_i2c_pga_configs[TLV320_PGA_CH1]);
    return 0;
}

static int tlv320_pga1_setting_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = tlv320_i2c_pga_configs[TLV320_PGA_CH1].msg_buf[1];
    return 0;
}

static int tlv320_pga2_setting_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    tlv320_i2c_pga_configs[TLV320_PGA_CH2].msg_buf[1] = ucontrol->value.integer.value[0];
    tlv320_i2c_pga_config_send(&tlv320_i2c_pga_configs[TLV320_PGA_CH2]);
    return 0;
}

static int tlv320_pga2_setting_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = tlv320_i2c_pga_configs[TLV320_PGA_CH2].msg_buf[1];
    return 0;
}

static int tlv320_pga3_setting_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    tlv320_i2c_pga_configs[TLV320_PGA_CH3].msg_buf[1] = ucontrol->value.integer.value[0];
    tlv320_i2c_pga_config_send(&tlv320_i2c_pga_configs[TLV320_PGA_CH3]);
    return 0;
}

static int tlv320_pga3_setting_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = tlv320_i2c_pga_configs[TLV320_PGA_CH3].msg_buf[1];
    return 0;
}

static int tlv320_pga4_setting_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    tlv320_i2c_pga_configs[TLV320_PGA_CH4].msg_buf[1] = ucontrol->value.integer.value[0];
    tlv320_i2c_pga_config_send(&tlv320_i2c_pga_configs[TLV320_PGA_CH4]);
    return 0;
}

static int tlv320_pga4_setting_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = tlv320_i2c_pga_configs[TLV320_PGA_CH4].msg_buf[1];
    return 0;
}

static int tlv320_pga5_setting_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    tlv320_i2c_pga_configs[TLV320_PGA_CH5].msg_buf[1] = ucontrol->value.integer.value[0];
    tlv320_i2c_pga_config_send(&tlv320_i2c_pga_configs[TLV320_PGA_CH5]);
    return 0;
}

static int tlv320_pga5_setting_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = tlv320_i2c_pga_configs[TLV320_PGA_CH5].msg_buf[1];
    return 0;
}

static int tlv320_pga6_setting_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    tlv320_i2c_pga_configs[TLV320_PGA_CH6].msg_buf[1] = ucontrol->value.integer.value[0];
    tlv320_i2c_pga_config_send(&tlv320_i2c_pga_configs[TLV320_PGA_CH6]);
    return 0;
}

static int tlv320_pga6_setting_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = tlv320_i2c_pga_configs[TLV320_PGA_CH6].msg_buf[1];
    return 0;
}

static int tlv320_pga7_setting_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    tlv320_i2c_pga_configs[TLV320_PGA_CH7].msg_buf[1] = ucontrol->value.integer.value[0];
    tlv320_i2c_pga_config_send(&tlv320_i2c_pga_configs[TLV320_PGA_CH7]);
    return 0;
}

static int tlv320_pga7_setting_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = tlv320_i2c_pga_configs[TLV320_PGA_CH7].msg_buf[1];
    return 0;
}

static int tlv320_pga8_setting_set(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    tlv320_i2c_pga_configs[TLV320_PGA_CH8].msg_buf[1] = ucontrol->value.integer.value[0];
    tlv320_i2c_pga_config_send(&tlv320_i2c_pga_configs[TLV320_PGA_CH8]);
    return 0;
}

static int tlv320_pga8_setting_get(struct snd_kcontrol *kcontrol,
                               struct snd_ctl_elem_value *ucontrol)
{
    ucontrol->value.integer.value[0] = tlv320_i2c_pga_configs[TLV320_PGA_CH8].msg_buf[1];
    return 0;
}

static const struct snd_kcontrol_new tlv320_snd_controls[] = {

    SOC_SINGLE_EXT_TLV("PGA1_setting",
                       0, 0, TLV320_PGA_GAIN_MAX, 0,
                       tlv320_pga1_setting_get, tlv320_pga1_setting_set,
                       tlv320_pga_setting_tlv),
    SOC_SINGLE_EXT_TLV("PGA2_setting",
                       0, 0, TLV320_PGA_GAIN_MAX, 0,
                       tlv320_pga2_setting_get, tlv320_pga2_setting_set,
                       tlv320_pga_setting_tlv),
    SOC_SINGLE_EXT_TLV("PGA3_setting",
                       0, 0, TLV320_PGA_GAIN_MAX, 0,
                       tlv320_pga3_setting_get, tlv320_pga3_setting_set,
                       tlv320_pga_setting_tlv),
    SOC_SINGLE_EXT_TLV("PGA4_setting",
                       0, 0, TLV320_PGA_GAIN_MAX, 0,
                       tlv320_pga4_setting_get, tlv320_pga4_setting_set,
                       tlv320_pga_setting_tlv),
    SOC_SINGLE_EXT_TLV("PGA5_setting",
                       0, 0, TLV320_PGA_GAIN_MAX, 0,
                       tlv320_pga5_setting_get, tlv320_pga5_setting_set,
                       tlv320_pga_setting_tlv),
    SOC_SINGLE_EXT_TLV("PGA6_setting",
                       0, 0, TLV320_PGA_GAIN_MAX, 0,
                       tlv320_pga6_setting_get, tlv320_pga6_setting_set,
                       tlv320_pga_setting_tlv),
    SOC_SINGLE_EXT_TLV("PGA7_setting",
                       0, 0, TLV320_PGA_GAIN_MAX, 0,
                       tlv320_pga7_setting_get, tlv320_pga7_setting_set,
                       tlv320_pga_setting_tlv),
    SOC_SINGLE_EXT_TLV("PGA8_setting",
                       0, 0, TLV320_PGA_GAIN_MAX, 0,
                       tlv320_pga8_setting_get, tlv320_pga8_setting_set,
                       tlv320_pga_setting_tlv),
};

static int tlv320_codec_suspend(struct snd_soc_codec *codec)
{
    return 0;
}

static int tlv320_codec_resume(struct snd_soc_codec *codec)
{
    return 0;
}

static int tlv320_codec_reset(struct platform_device *pdev)
{
    int gpio, flag;

#ifdef CONFIG_ARCH_SUN8IW5
    script_item_u               item;
    script_item_value_type_e    type;

    type = script_get_item("tlv320-codec", "reset_pin", &item);
    if (SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
        print_wdbg("script_get_item return type err\n");
        return -EFAULT;
    }
    gpio = item.gpio.gpio;

#else

    gpio = of_get_named_gpio_flags(pdev->dev.of_node, "reset-pin", 0, (enum of_gpio_flags *)&flag);
    if (!gpio_is_valid(gpio)){
        print_wdbg("invalid led-power: %d\n", gpio);
        return -EFAULT;
    }
#endif

    /*request gpio*/
    if (gpio_request(gpio, "reset-pin") != 0) {
        print_wdbg("gpio %d request failed!\n", gpio);
        return -EFAULT;
    }
    printk("............tlv320_codec_reset: %d\n", gpio);
    // Wait 10us at least
   // print_ddbg("tlv320_codec_reset(), pull down gpio %d for codec reset last 10us.\n", gpio);
    gpio_direction_output(gpio, 1);
    udelay(10);
    gpio_set_value(gpio, 0);
    udelay(10);
    gpio_set_value(gpio, 1);

    return 0;
}

static int tlv320_codec_probe(struct snd_soc_codec *codec)
{
    int ret = 0;

    print_ddbg("tlv320_codec_probe()\n");
    ret = snd_soc_add_codec_controls(codec, tlv320_snd_controls, ARRAY_SIZE(tlv320_snd_controls));

    return ret;
}

static int tlv320_codec_remove(struct snd_soc_codec *codec)
{
    return 0;
}

static u32 _atoi(char s[])
{
    int i, len;
    u32 n, temp = 0;

    len=strlen(s);
    for(i=0;i<len;i++) {
        if (s[i]>='0' &&s [i]<='9') {
            n=s[i]-'0';
        } else {
            continue;
        }
        temp=temp*10+n;
    }
    return temp;
}

static int tlv320_codec_param_write(int client_id, unsigned char *msg, int msg_len) {
    struct i2c_client *client;
    int ret;

    if ((client_id >= TLV320_I2C_CLIENT_NUM) || (msg_len > TLV320_I2C_MSG_LENGTH_MAX)) {
        print_wdbg("client_id %d or msg_len %d is illegal!!!\n", client_id, msg_len);
        return -1;
    }

    client = tlv320_clients[client_id];
    if (!client) {
        print_wdbg("tlv320_clients[%d] is NULL!!!\n", client_id);
        return -1;
    }

    print_ddbg("tlv320_codec_param_write, client_id %d, msg_len %d, msg[0x%02x,0x%02x,0x%02x,0x%02x]\n", client_id, msg_len, msg[0], msg[1], msg[2], msg[3]);
    ret = i2c_master_send(client, msg, msg_len);
    if (ret != msg_len) {
        print_wdbg("i2c_master_send, client_id %d return err:%d\n", client_id, ret);
    }
    print_ddbg("tlv320_codec_param_write() end\n");

    return 0;
}

//static int tlv320_proc_params_write(struct file *file, const char __user *buffer,
static ssize_t tlv320_proc_params_write(struct file *file, const char __user *buffer,
                    size_t count, loff_t * offset) {
    char paramArray[50];
    char *paramsTmp;
    char *pSubStr;
    int  i, len;
    unsigned char param[TLV320_I2C_MSG_LENGTH_MAX + 2] = {0};

    len = count;
    if (count >= sizeof(paramArray)) {
        len = sizeof(paramArray) - 1;
    }

    if(copy_from_user(paramArray, buffer, len)) {
        printk("copy from user fail\n");
        return -EFAULT;
    }
    paramArray[len -1] = 0;

    printk("tlv320_proc_params_write, len=%d, [%s]\n", len, paramArray);
    /* format: client-id,param-len,param1,param2,... */
    paramsTmp = (char *)&paramArray[0];
    pSubStr = strsep(&paramsTmp, ",");
    printk("tlv320_proc_params_write, pSubStr[%s]\n", pSubStr);
    for (i=0; (i < sizeof(param)) && (pSubStr != NULL); i++) {
        param[i] = (unsigned char)_atoi(pSubStr);
        pSubStr = strsep(&paramsTmp, ",");
    }

    tlv320_codec_param_write(param[0], &param[2], param[1]);

    return len;
}

static int tlv320_codec_page_read(int client_id, unsigned char *msg, int msg_len) {
    struct i2c_client *client;
    int ret;
    unsigned char reg[1] = {0};

    if (client_id >= TLV320_I2C_CLIENT_NUM) {
        print_wdbg("client_id %d is illegal!!!\n", client_id);
        return -1;
    }

    client = tlv320_clients[client_id];
    if (!client) {
        print_wdbg("tlv320_clients[%d] is NULL!!!\n", client_id);
        return -1;
    }

    print_ddbg("tlv320_codec_page_read, client_id %d, msg_len %d\n", client_id, msg_len);
    /*select reg num 0 to start read*/
    i2c_master_send(client, reg, sizeof(reg));//reg =0 ,len =1

    ret = i2c_master_recv(client, msg, msg_len);
    if (ret != msg_len) {
        print_wdbg("i2c_master_recv, client_id %d return err:%d\n", client_id, ret);
        return -1;
    }
    print_ddbg("tlv320_codec_page_read() end\n");

    return 0;
}

/* Assume that register page has been selected via params write,   */
/* read the all register values of current selected register page. */
/* This function is for test only.                                 */
static ssize_t tlv320_proc_params_read(struct file *file, char __user *buffer,
                    size_t count, loff_t * offset) {
    unsigned char i2c_msg[TLV320_REG_NUM_MAX]={0};
    unsigned char i2c_client_codec_regs[TLV320_I2C_CLIENT_NUM][TLV320_REG_NUM_MAX];
    int i,j;
    unsigned long missing;
    int ret;

    print_ddbg("tlv320_proc_params_read() begin, count=%d, offset=%d\n", (int)count, (int)*offset);
    if (*offset > 0) {
        //Read END
        return 0;
    }

    for (i=TLV320_I2C_CLIENT_ADC0; i<TLV320_I2C_CLIENT_NUM; i++) {
        if(tlv320_codec_page_read(i,&i2c_msg[0], TLV320_REG_NUM_MAX) == 0){
            memcpy(&i2c_client_codec_regs[i][0], i2c_msg, TLV320_REG_NUM_MAX);
            for(j = 0; j < TLV320_REG_NUM_MAX; j++) {
                print_ddbg(" ADC%d Register[%d] 0x%02x\n",i, j, i2c_msg[j]);
            }
        }
    }
    if(count >= sizeof(i2c_client_codec_regs)) {
        missing = copy_to_user(buffer, i2c_client_codec_regs, sizeof(i2c_client_codec_regs));
        if (missing > 0) print_wdbg("ERROR: copy to user missing %ld bytes\n", missing);
        ret = sizeof(i2c_client_codec_regs);
        *offset = sizeof(i2c_client_codec_regs);
    }else{
        ret = 0;
    }
    print_ddbg("tlv320_proc_params_read() end, count=%d, ret=%d\n", (int)count, ret);

    return ret;
}

static const struct file_operations tlv320_proc_params_entry_operations =
{
    .owner = THIS_MODULE,
    .write = tlv320_proc_params_write,
    .read = tlv320_proc_params_read,
};

static int tlv320_proc_params_init(struct proc_dir_entry* proc_root) {
    struct proc_dir_entry *p;

    p = proc_create_data("params", 0660, proc_root, &tlv320_proc_params_entry_operations, NULL);
    if(p == NULL) {
        printk("create proc error!\n");
        return -ENOMEM;;
    }

    return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_tlv320 = {
    .probe              = tlv320_codec_probe,
    .remove             = tlv320_codec_remove,
    .suspend            = tlv320_codec_suspend,
    .resume             = tlv320_codec_resume,
};

static int tlv320_probe(struct platform_device *pdev)
{
    struct proc_dir_entry *p;
    struct i2c_adapter *adapter;
    struct i2c_client *client;
    int i = 0;

    tlv320_queue = create_singlethread_workqueue("tlv320_init");
    INIT_WORK(&tlv320_init_work, tlv320_i2c_init_work_func);

    tlv320_codec_reset(pdev);
    adapter = i2c_get_adapter(TLV320_I2C_CHANNEL_ID);
    if (!adapter) {
        print_wdbg("i2c_get_adapter() fail!\n");
        return -ENODEV;
    }

    for(i = 0; i < TLV320_I2C_CLIENT_NUM; i++) {
        client = i2c_new_device(adapter, &tlv320_i2c_board_info[i]);
        print_ddbg("tlv320_probe() i2c_new_device\n");
        if (!client)
            return -ENODEV;
    }

    i2c_put_adapter(adapter);
    i2c_add_driver(&tlv320_i2c_driver);

    p = proc_mkdir("tlv320", NULL);
    if (p == NULL) {
        return -ENOMEM;
    }

    tlv320_proc_params_init(p);

    return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tlv320, &tlv320_codec_dai0, 1);
}

static int tlv320_remove(struct platform_device *pdev)
{
    i2c_del_driver(&tlv320_i2c_driver);
    snd_soc_unregister_codec(&pdev->dev);

    destroy_workqueue(tlv320_queue);

    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tlv320_codec_match[] = {
    { .compatible = "codec-tlv320", },
    {},
};
MODULE_DEVICE_TABLE(of, tlv320_codec_match);
#else
static struct platform_device tlv320_device = {
    .name = "codec-tlv320",
    .id   = -1,
};
#endif

/*method relating*/
static struct platform_driver tlv320_driver = {
    .driver = {
        .name = "codec-tlv320",
        .owner = THIS_MODULE,
        #ifdef CONFIG_OF
        .of_match_table = of_match_ptr(tlv320_codec_match),
        #endif
    },
    .probe = tlv320_probe,
    .remove = tlv320_remove,
};

static int __init tlv320_init(void)
{
    int err = 0;

#ifndef CONFIG_OF
    if((err = platform_device_register(&tlv320_device)) < 0) {
        print_wdbg("tlv320_init() register device err %d\n", err);
        return err;
    }
#endif

    if ((err = platform_driver_register(&tlv320_driver)) < 0) {
        print_wdbg("tlv320_init() register driver err %d\n", err);
        return err;
    }

    print_ddbg("tlv320_init() end\n");
    return 0;
}
module_init(tlv320_init);

static void __exit tlv320_exit(void)
{
#ifndef CONFIG_OF
    platform_device_unregister(&tlv320_device);
#endif
    platform_driver_unregister(&tlv320_driver);
}
module_exit(tlv320_exit);

MODULE_DESCRIPTION("ASoC codec tlv320 driver");
MODULE_LICENSE("GPL");
