/*
* Copyright (C) 2015 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/


/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_btcvsd_rx.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio btcvsd capture
 *
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include <linux/module.h>

#include "mtk-soc-pcm-btcvsd.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

/* extern int AudDrv_BTCVSD_IRQ_handler(void); */
volatile kal_uint32 *bt_hw_REG_PACKET_W, *bt_hw_REG_PACKET_R;
volatile kal_uint32 *bt_hw_REG_CONTROL;
volatile void *BTSYS_PKV_BASE_ADDRESS;
volatile void *BTSYS_SRAM_BANK2_BASE_ADDRESS;
volatile void *AUDIO_INFRA_BASE_VIRTUAL;
kal_uint32 disableBTirq;
static bool mPrepareDone;
u32 btcvsd_irq_number = AP_BT_CVSD_IRQ_LINE;
/* to mask BT CVSD IRQ when AP-side CVSD disable. Note: 72 is bit1 */
static volatile void *INFRA_MISC_ADDRESS;
DEFINE_SPINLOCK(auddrv_btcvsd_rx_lock);


struct timeval begin_rx;
int prev_sec_rx; /* define 0 @ open */
long prev_usec_rx;
long diff_msec_rx;


void Disable_CVSD_Wakeup(void)
{
	volatile kal_uint32 *INFRA_MISC_REGISTER = (volatile kal_uint32 *)(INFRA_MISC_ADDRESS);
	*INFRA_MISC_REGISTER |= conn_bt_cvsd_mask;
	pr_err("Disable_CVSD_Wakeup\n");
}

void Enable_CVSD_Wakeup(void)
{
	volatile kal_uint32 *INFRA_MISC_REGISTER = (volatile kal_uint32 *)(INFRA_MISC_ADDRESS);
	*INFRA_MISC_REGISTER &= ~(conn_bt_cvsd_mask);
	pr_err("Enable_CVSD_Wakeup\n");
}

#ifdef CONFIG_OF
static int Auddrv_BTCVSD_Irq_Map(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,audio_bt_cvsd");
	if (node == NULL)
		pr_debug("BTCVSD get node failed\n");

	/* get btcvsd irq num */
	btcvsd_irq_number = irq_of_parse_and_map(node, 0);
	pr_debug("[BTCVSD] btcvsd_irq_number=%d\n", btcvsd_irq_number);
	if (!btcvsd_irq_number) {
		pr_debug("[BTCVSD] get btcvsd_irq_number failed!!!\n");
		return -1;
	}
	pr_warn("%s, btcvsd_irq_number=%d\n", __func__, btcvsd_irq_number);
	return 0;
}

static int Auddrv_BTCVSD_Address_Map(void)
{
	struct device_node *node = NULL;
	void __iomem *base;
	u32 offset[5] = { 0, 0, 0, 0, 0 };

	node = of_find_compatible_node(NULL, NULL, "mediatek,audio_bt_cvsd");
	if (node == NULL)
		pr_debug("BTCVSD get node failed\n");

	/*get INFRA_MISC offset, conn_bt_cvsd_mask, cvsd_mcu_read_offset, write_offset, packet_indicator */
	of_property_read_u32_array(node, "offset", offset, ARRAY_SIZE(offset));
	infra_misc_offset = offset[0];
	conn_bt_cvsd_mask = offset[1];
	cvsd_mcu_read_offset = offset[2];
	cvsd_mcu_write_offset = offset[3];
	cvsd_packet_indicator = offset[4];

	/*get infra base address */
	base = of_iomap(node, 0);
	infra_base = (unsigned long)base;

	/*get btcvsd sram address */
	base = of_iomap(node, 1);
	btsys_pkv_physical_base = (unsigned long)base;
	base = of_iomap(node, 2);
	btsys_sram_bank2_physical_base = (unsigned long)base;

	/*print for debug */
	pr_err("[BTCVSD] Auddrv_BTCVSD_Address_Map:\n");
	pr_err("[BTCVSD] infra_misc_offset=0x%lx\n", infra_misc_offset);
	pr_err("[BTCVSD] conn_bt_cvsd_mask=0x%lx\n", conn_bt_cvsd_mask);
	pr_err("[BTCVSD] read_off=0x%lx\n", cvsd_mcu_read_offset);
	pr_err("[BTCVSD] write_off=0x%lx\n", cvsd_mcu_write_offset);
	pr_err("[BTCVSD] packet_ind=0x%lx\n", cvsd_packet_indicator);
	pr_err("[BTCVSD] infra_base=0x%lx\n", infra_base);
	pr_err("[BTCVSD] btsys_pkv_physical_base=0x%lx\n", btsys_pkv_physical_base);
	pr_err("[BTCVSD] btsys_sram_bank2_physical_base=0x%lx\n", btsys_sram_bank2_physical_base);

	if (!infra_base) {
		pr_err("[BTCVSD] get infra_base failed!!!\n");
		return -1;
	}
	if (!btsys_pkv_physical_base) {
		pr_err("[BTCVSD] get btsys_pkv_physical_base failed!!!\n");
		return -1;
	}
	if (!btsys_sram_bank2_physical_base) {
		pr_err("[BTCVSD] get btsys_sram_bank2_physical_base failed!!!\n");
		return -1;
	}
	return 0;
}

#endif

static int mtk_pcm_btcvsd_rx_stop(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);
	Set_BTCVSD_State(BT_SCO_RXSTATE_ENDING);
	return 0;
}

static snd_pcm_uframes_t prev_frame;
static kal_int32 prev_iPacket_w;

static snd_pcm_uframes_t mtk_pcm_btcvsd_rx_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t frame = 0;
	kal_uint32 byte = 0;
	static kal_int32 packet_diff;

	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */

	LOGBT("%s\n", __func__);

	spin_lock_irqsave(&auddrv_btcvsd_rx_lock, flags);

	/* get packet diff from last time */
	LOGBT("%s(), btsco.pRX->iPacket_w = %d, prev_iPacket_w = %d\n",
	      __func__,
	      btsco.pRX->iPacket_w,
	      prev_iPacket_w);
	if (btsco.pRX->iPacket_w >= prev_iPacket_w) {
		packet_diff = btsco.pRX->iPacket_w - prev_iPacket_w;
	} else {
		/* integer overflow */
		packet_diff = (INT_MAX - prev_iPacket_w) +
			      (btsco.pRX->iPacket_w - INT_MIN) + 1;
	}
	prev_iPacket_w = btsco.pRX->iPacket_w;
	/* increased bytes */
	byte = packet_diff * (SCO_RX_PLC_SIZE + BTSCO_CVSD_PACKET_VALID_SIZE);
	frame = btcvsd_bytes_to_frame(substream, byte);
	frame += prev_frame;
	frame %= substream->runtime->buffer_size;

	prev_frame = frame;
	LOGBT("%s(), frame %lu, byte=%d, btsco.pRX->iPacket_w=%d\n",
	      __func__,
	      frame,
	      byte,
	      btsco.pRX->iPacket_w);

	spin_unlock_irqrestore(&auddrv_btcvsd_rx_lock, flags);

	return frame;

}


static int mtk_pcm_btcvsd_rx_hw_params(struct snd_pcm_substream *substream,
										struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;

	pr_warn("%s\n", __func__);

#if 1

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	/* pr_warn("mtk_pcm_hw_params dma_bytes = %d\n",substream->runtime->dma_bytes); */

	if (BT_CVSD_Mem.RX_btcvsd_dma_buf.area) {
		substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
		substream->runtime->dma_area = BT_CVSD_Mem.RX_btcvsd_dma_buf.area;
		substream->runtime->dma_addr = BT_CVSD_Mem.RX_btcvsd_dma_buf.addr;
	}

	pr_warn("%s, 1 dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n", __func__,
			substream->runtime->dma_bytes, substream->runtime->dma_area,
			(long)substream->runtime->dma_addr);

#endif

	return ret;

}

static int mtk_pcm_btcvsd_rx_hw_free(struct snd_pcm_substream *substream)
{
	LOGBT("%s\n", __func__);

	if (BT_CVSD_Mem.RX_btcvsd_dma_buf.area)
		return 0;
	else
		return snd_pcm_lib_free_pages(substream);

	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(bt_supported_sample_rates),
	.list = bt_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_btcvsd_rx_close(struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_warn("%s\n", __func__);

	Set_BTCVSD_State(BT_SCO_RXSTATE_IDLE);
	ret = AudDrv_btcvsd_Free_Buffer(1);

	BT_CVSD_Mem.RX_substream = NULL;

	if (mPrepareDone == true)
		mPrepareDone = false;

	return 0;
}

static int mtk_pcm_btcvsd_rx_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_warn("%s\n", __func__);

	ret = AudDrv_btcvsd_Allocate_Buffer(1);

	runtime->hw = mtk_btcvsd_hardware;

	memcpy((void *)(&(runtime->hw)), (void *)&mtk_btcvsd_hardware, sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE, &constraints_sample_rates);


	BT_CVSD_Mem.RX_substream = substream;

	do_gettimeofday(&begin_rx);
	prev_sec_rx = begin_rx.tv_sec;
	prev_usec_rx = begin_rx.tv_usec;

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	if (ret < 0) {
		pr_err("ret < 0 mtk_pcm_btcvsd_rx_close\n");
		mtk_pcm_btcvsd_rx_close(substream);
		return ret;
	}

	/* pr_warn("mtk_pcm_btcvsd_open return\n"); */
	return 0;
}

static int mtk_pcm_btcvsd_rx_prepare(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);

	Set_BTCVSD_State(BT_SCO_RXSTATE_RUNNING);

	return 0;
}

static int mtk_pcm_btcvsd_rx_start(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);
	prev_frame = 0;
	prev_iPacket_w = btsco.pRX->iPacket_w;

	return 0;
}

static int mtk_pcm_btcvsd_rx_trigger(struct snd_pcm_substream *substream, int cmd)
{
	LOGBT("%s\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_btcvsd_rx_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_btcvsd_rx_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_btcvsd_rx_copy(struct snd_pcm_substream *substream,
				  int channel, snd_pcm_uframes_t pos,
				  void __user *dst, snd_pcm_uframes_t count)
{
	/* get total bytes to copy */
	char *Read_Data_Ptr = (char *)dst;

	count = btcvsd_frame_to_bytes(substream, count);

	AudDrv_btcvsd_read(Read_Data_Ptr, count);

	LOGBT("pcm_copy return\n");
	return 0;
}

static int mtk_pcm_btcvsd_rx_silence(struct snd_pcm_substream *substream,
				     int channel, snd_pcm_uframes_t pos,
				     snd_pcm_uframes_t count)
{
	LOGBT("%s\n", __func__);
	return 0; /* do nothing */
}

static int mtk_asoc_pcm_btcvsd_rx_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	LOGBT("%s\n", __func__);
	return ret;
}

static struct snd_pcm_ops mtk_btcvsd_rx_ops = {
	.open =     mtk_pcm_btcvsd_rx_open,
	.close =    mtk_pcm_btcvsd_rx_close,
	.ioctl =    snd_pcm_lib_ioctl,
	.hw_params =    mtk_pcm_btcvsd_rx_hw_params,
	.hw_free =  mtk_pcm_btcvsd_rx_hw_free,
	.prepare =  mtk_pcm_btcvsd_rx_prepare,
	.trigger =  mtk_pcm_btcvsd_rx_trigger,
	.pointer =  mtk_pcm_btcvsd_rx_pointer,
	.copy =     mtk_pcm_btcvsd_rx_copy,
	.silence =  mtk_pcm_btcvsd_rx_silence,
};

/* btsco band info */
static const char * const btsco_band_str[] = {"NB", "WB"};
static const char * const btsco_mute_str[] = {"Off", "On"};
static const char * const irq_received_str[] = {"No", "Yes"};
static const char * const rx_timeout_str[] = {"No", "Yes"};

static const struct soc_enum btcvsd_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(btsco_band_str), btsco_band_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(btsco_mute_str), btsco_mute_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(irq_received_str), irq_received_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_timeout_str), rx_timeout_str),
};

static int btcvsd_band_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), band %d\n", __func__, get_btcvsd_band());
	ucontrol->value.integer.value[0] = get_btcvsd_band();
	return 0;
}

static int btcvsd_band_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(btsco_band_str)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	set_btcvsd_band(ucontrol->value.integer.value[0]);
	return 0;
}

static int btcvsd_tx_mute_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	if (!btsco.pTX) {
		ucontrol->value.integer.value[0] = 0;
		return 0;
	}

	pr_debug("%s(), band %d\n", __func__, btsco.pTX->mute);
	ucontrol->value.integer.value[0] = btsco.pTX->mute;

	return 0;
}

static int btcvsd_tx_mute_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(btsco_mute_str)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	if (!btsco.pTX)
		return 0;

	btsco.pTX->mute = ucontrol->value.integer.value[0];
	return 0;
}

static int btcvsd_rx_irq_received_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), rx irq received %d\n",
		 __func__, btcvsd_rx_irq_received());
	ucontrol->value.integer.value[0] = btcvsd_rx_irq_received();
	return 0;
}

static int btcvsd_rx_irq_received_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(irq_received_str)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	return 0;
}

static int btcvsd_rx_timeout_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), btcvsd rx timeout %d\n",
		 __func__, btcvsd_rx_timeout() ? 1 : 0);
	ucontrol->value.integer.value[0] = btcvsd_rx_timeout() ? 1 : 0;
	btcvsd_rx_reset_timeout();
	return 0;
}

static int btcvsd_rx_timeout_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(rx_timeout_str)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	return 0;
}

static const struct snd_kcontrol_new btcvsd_controls[] = {
	SOC_ENUM_EXT("btcvsd_band",
		     btcvsd_enum[0], btcvsd_band_get, btcvsd_band_set),
	SOC_ENUM_EXT("btcvsd_tx_mute",
		     btcvsd_enum[1], btcvsd_tx_mute_get, btcvsd_tx_mute_set),
	SOC_ENUM_EXT("btcvsd_rx_irq_received",
		     btcvsd_enum[2],
		     btcvsd_rx_irq_received_get,
		     btcvsd_rx_irq_received_set),
	SOC_ENUM_EXT("btcvsd_rx_timeout",
		     btcvsd_enum[3],
		     btcvsd_rx_timeout_get,
		     btcvsd_rx_timeout_set),
};

static int mtk_asoc_pcm_btcvsd_rx_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, btcvsd_controls,
				      ARRAY_SIZE(btcvsd_controls));
	return 0;
}

static struct snd_soc_platform_driver mtk_btcvsd_rx_soc_platform = {
	.ops        = &mtk_btcvsd_rx_ops,
	.pcm_new    = mtk_asoc_pcm_btcvsd_rx_new,
	.probe = mtk_asoc_pcm_btcvsd_rx_probe,
};

static int mtk_btcvsd_rx_probe(struct platform_device *pdev)
{
	int ret = 0;

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_BTCVSD_RX_PCM);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev_btcvsd_rx = &pdev->dev;

#ifdef CONFIG_OF
	Auddrv_BTCVSD_Irq_Map();
#endif

	ret = Register_BTCVSD_Irq(pdev, btcvsd_irq_number);

	if (ret < 0)
		pr_warn("%s request_irq btcvsd_irq_number(%d) Fail\n", __func__, btcvsd_irq_number);

	/* inremap to INFRA sys */
#ifdef CONFIG_OF
	Auddrv_BTCVSD_Address_Map();
	INFRA_MISC_ADDRESS = (volatile kal_uint32 *)(infra_base + infra_misc_offset);
#else
	AUDIO_INFRA_BASE_VIRTUAL = ioremap_nocache(AUDIO_INFRA_BASE_PHYSICAL, 0x1000);
	INFRA_MISC_ADDRESS = (volatile kal_uint32 *)(AUDIO_INFRA_BASE_VIRTUAL + INFRA_MISC_OFFSET);
#endif
	pr_warn("[BTCVSD probe] INFRA_MISC_ADDRESS = %p\n", INFRA_MISC_ADDRESS);

	pr_warn("%s disable BT IRQ disableBTirq = %d,irq=%d\n", __func__, disableBTirq, btcvsd_irq_number);
	if (disableBTirq == 0) {
		disable_irq(btcvsd_irq_number);
		Disable_CVSD_Wakeup();
		disableBTirq = 1;
	}

	if (!isProbeDone) {
		memset((void *)&BT_CVSD_Mem, 0, sizeof(CVSD_MEMBLOCK_T));
		isProbeDone = 1;
	}

	memset((void *)&btsco, 0, sizeof(btsco));
	btsco.uTXState = BT_SCO_TXSTATE_IDLE;
	btsco.uRXState = BT_SCO_RXSTATE_IDLE;

	/* ioremap to BT HW register base address */
#ifdef CONFIG_OF
	BTSYS_PKV_BASE_ADDRESS = (void *)btsys_pkv_physical_base;
	BTSYS_SRAM_BANK2_BASE_ADDRESS = (void *)btsys_sram_bank2_physical_base;
	bt_hw_REG_PACKET_R = BTSYS_PKV_BASE_ADDRESS + cvsd_mcu_read_offset;
	bt_hw_REG_PACKET_W = BTSYS_PKV_BASE_ADDRESS + cvsd_mcu_write_offset;
	bt_hw_REG_CONTROL = BTSYS_PKV_BASE_ADDRESS + cvsd_packet_indicator;
#else
	BTSYS_PKV_BASE_ADDRESS = ioremap_nocache(AUDIO_BTSYS_PKV_PHYSICAL_BASE, 0x10000);
	BTSYS_SRAM_BANK2_BASE_ADDRESS = ioremap_nocache(AUDIO_BTSYS_SRAM_BANK2_PHYSICAL_BASE, 0x10000);
	bt_hw_REG_PACKET_R = (volatile kal_uint32 *)(BTSYS_PKV_BASE_ADDRESS + CVSD_MCU_READ_OFFSET);
	bt_hw_REG_PACKET_W = (volatile kal_uint32 *)(BTSYS_PKV_BASE_ADDRESS + CVSD_MCU_WRITE_OFFSET);
	bt_hw_REG_CONTROL = (volatile kal_uint32 *)(BTSYS_PKV_BASE_ADDRESS + CVSD_PACKET_INDICATOR);
#endif
	pr_debug("[BTCVSD probe] BTSYS_PKV_BASE_ADDRESS = %p BTSYS_SRAM_BANK2_BASE_ADDRESS = %p\n",
				BTSYS_PKV_BASE_ADDRESS, BTSYS_SRAM_BANK2_BASE_ADDRESS);

	return snd_soc_register_platform(&pdev->dev, &mtk_btcvsd_rx_soc_platform);
}

static int mtk_btcvsd_rx_remove(struct platform_device *pdev)
{
	LOGBT("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

/***************************************************************************
 * FUNCTION
 *  mtk_btcvsd_soc_platform_init / mtk_btcvsd_soc_platform_exit
 *
 * DESCRIPTION
 *  Module init and de-init (only be called when system boot up)
 *
 **************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_btcvsd_rx_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_btcvsd_rx_pcm", },
	{}
};
#endif

static struct platform_driver mtk_btcvsd_rx_driver = {
	.driver = {
		.name = MT_SOC_BTCVSD_RX_PCM,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_soc_pcm_btcvsd_rx_of_ids,
#endif
	},
	.probe = mtk_btcvsd_rx_probe,
	.remove = mtk_btcvsd_rx_remove,
};


#ifndef CONFIG_OF
static struct platform_device *soc_mtk_btcvsd_rx_dev;
#endif

static int __init mtk_btcvsd_rx_soc_platform_init(void)
{
	int ret;

	pr_warn("+%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtk_btcvsd_rx_dev = platform_device_alloc(MT_SOC_BTCVSD_RX_PCM, -1);
	if (!soc_mtk_btcvsd_rx_dev) {
		pr_warn("-%s, platform_device_alloc() fail, return\n", __func__);
		return -ENOMEM;
	}

	ret = platform_device_add(soc_mtk_btcvsd_rx_dev);
	if (ret != 0) {
		pr_warn("-%s, platform_device_add() fail, return\n", __func__);
		platform_device_put(soc_mtk_btcvsd_rx_dev);
		return ret;
	}
#endif

	/* Register platform DRIVER */
	ret = platform_driver_register(&mtk_btcvsd_rx_driver);
	if (ret) {
		pr_warn("-%s platform_driver_register Fail:%d\n", __func__, ret);
		return ret;
	}

	return ret;

}
module_init(mtk_btcvsd_rx_soc_platform_init);

static void __exit mtk_btcvsd_rx_soc_platform_exit(void)
{
	pr_warn("%s\n", __func__);
	platform_driver_unregister(&mtk_btcvsd_rx_driver);
}
module_exit(mtk_btcvsd_rx_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");

