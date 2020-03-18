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
 *   mtk_pcm_capture.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio Ul1 data1 uplink
 *
 * Author:
 * -------
 * Chipeng Chang
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

#include "mtk-auddrv-common.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-platform.h"

#define AUDIO_ALLOCATE_SMP_RATE_DECLARE

/* #define CAPTURE_FORCE_USE_DRAM //foruse DRAM for record */

/* information about */
AFE_MEM_CONTROL_T  *VUL_Control_context;
static struct snd_dma_buffer *Capture_dma_buf;
static AudioDigtalI2S *mAudioDigitalI2S;
static bool mCaptureUseSram;

static bool vcore_dvfs_enable;
static int capture_hdinput_control;
static const void *irq_user_id;
static uint32 irq2_cnt;


/*
 *    function implementation
 */
static int mtk_capture_probe(struct platform_device *pdev);
static int mtk_capture_pcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_capture_pcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_capture_probe(struct snd_soc_platform *platform);

static const char * const capture_HD_input[] = {"Off", "On"};

static const struct soc_enum Audio_capture_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(capture_HD_input), capture_HD_input),
};

static int Audio_capture_hdinput_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpR_Get = %d\n", capture_hdinput_control);
	ucontrol->value.integer.value[0] = capture_hdinput_control;
	return 0;
}

static int Audio_capture_hdinput_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(capture_HD_input)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	capture_hdinput_control = ucontrol->value.integer.value[0];

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI) == true) {
		pr_err("return HDMI enabled\n");
		return 0;
	}

	return 0;
}

static int Audio_Irq2cnt_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	AudDrv_Clk_On();
	ucontrol->value.integer.value[0] = Afe_Get_Reg(AFE_IRQ_MCU_CNT2);
	AudDrv_Clk_Off();
	return 0;
}

static int Audio_Irq2cnt_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), irq_user_id = %p, irq2_cnt = %d, value = %ld\n",
		__func__,
		irq_user_id,
		irq2_cnt,
		ucontrol->value.integer.value[0]);

	if (irq2_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq2_cnt = ucontrol->value.integer.value[0];

	AudDrv_Clk_On();
	if (irq_user_id && irq2_cnt)
		irq_update_user(irq_user_id,
				Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE,
				0,
				irq2_cnt);
	else
		pr_warn("warn, cannot update irq counter, user_id = %p, irq2_cnt = %d\n",
			irq_user_id, irq2_cnt);

	AudDrv_Clk_Off();
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_capture_controls[] = {
	SOC_ENUM_EXT("Audio_capture_hd_Switch", Audio_capture_Enum[0],
		Audio_capture_hdinput_Get, Audio_capture_hdinput_Set),
	SOC_SINGLE_EXT("Audio IRQ2 CNT", SND_SOC_NOPM, 0, IRQ_MAX_RATE, 0,
		Audio_Irq2cnt_Get, Audio_Irq2cnt_Set),
};

static struct snd_pcm_hardware mtk_capture_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_RESUME |
	SNDRV_PCM_INFO_MMAP_VALID),
	.formats =      SND_SOC_ADV_MT_FMTS,
	.rates =        SOC_HIGH_USE_RATE,
	.rate_min =     SOC_HIGH_USE_RATE_MIN,
	.rate_max =     SOC_HIGH_USE_RATE_MAX,
	.channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = UL1_MAX_BUFFER_SIZE,
	.period_bytes_max = UL1_MAX_BUFFER_SIZE,
	.periods_min =      UL1_MIN_PERIOD_SIZE,
	.periods_max =      UL1_MAX_PERIOD_SIZE,
	.fifo_size =        0,
};

static int mtk_capture_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_warn("%s, format = %d, rate = %d\n", __func__, substream->runtime->format, substream->runtime->rate);

	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_VUL, substream);

	if (substream->runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		substream->runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_VUL, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_AFE_IO_Block_MEM_VUL);
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_VUL, AFE_WLEN_16_BIT);
		SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_AFE_IO_Block_MEM_VUL);
	}

	SetIntfConnection(Soc_Aud_InterCon_Connection, Soc_Aud_AFE_IO_Block_ADDA_UL, Soc_Aud_AFE_IO_Block_MEM_VUL);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL) == false) {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL, true);
		set_adc_in(substream->runtime->rate);
		set_adc_enable(true);
	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL, true);
	}

	return 0;
}

static int mtk_capture_alsa_stop(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);

	irq_user_id = NULL;
	irq_remove_user(substream, Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL, false);

	ClearMemBlock(Soc_Aud_Digital_Block_MEM_VUL);
	return 0;
}

static snd_pcm_uframes_t mtk_capture_pcm_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream,
		VUL_Control_context, Soc_Aud_Digital_Block_MEM_VUL);
}

static int mtk_capture_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	runtime->dma_bytes = params_buffer_bytes(hw_params);

	if (AllocateAudioSram(&substream->runtime->dma_addr,	&substream->runtime->dma_area,
		substream->runtime->dma_bytes, substream) == 0) {
		pr_aud("AllocateAudioSram success\n");
		SetHighAddr(Soc_Aud_Digital_Block_MEM_VUL, false, substream->runtime->dma_addr);
	} else if (Capture_dma_buf->area) {
		pr_aud("Capture_dma_buf = %p Capture_dma_buf->area = %p apture_dma_buf->addr = 0x%lx\n",
		       Capture_dma_buf, Capture_dma_buf->area, (long) Capture_dma_buf->addr);
		runtime->dma_area = Capture_dma_buf->area;
		runtime->dma_addr = Capture_dma_buf->addr;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_VUL, true, runtime->dma_addr);
		mCaptureUseSram = true;
		AudDrv_Emi_Clk_On();
	} else {
		pr_warn("mtk_capture_pcm_hw_params snd_pcm_lib_malloc_pages\n");
		ret =  snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
	}

	set_mem_block(substream, hw_params,
		VUL_Control_context, Soc_Aud_Digital_Block_MEM_VUL);

	pr_aud("mtk_capture_pcm_hw_params dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
	       substream->runtime->dma_bytes, substream->runtime->dma_area, (long)substream->runtime->dma_addr);
	return ret;
}

static int mtk_capture_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_aud("mtk_capture_pcm_hw_free\n");
	if (Capture_dma_buf->area) {
		if (mCaptureUseSram == true) {
			AudDrv_Emi_Clk_Off();
			mCaptureUseSram = false;
		} else
			freeAudioSram((void *)substream);
		return 0;
	} else
		return snd_pcm_lib_free_pages(substream);

}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
};

static int mtk_capture_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_Clk_On();
	VUL_Control_context = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_VUL);

	runtime->hw = mtk_capture_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_capture_hardware, sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");



	if (ret < 0) {
		pr_err("mtk_capture_pcm_close\n");
		mtk_capture_pcm_close(substream);
		return ret;
	}

	pr_aud("mtk_capture_pcm_open return\n");
	return 0;
}

static int mtk_capture_pcm_close(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_AFE_IO_Block_ADDA_UL, Soc_Aud_AFE_IO_Block_MEM_VUL);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL, false);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL) == false)
		set_adc_enable(false);

	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_VUL, substream);

	EnableAfe(false);

	AudDrv_Clk_Off();
	vcore_dvfs(&vcore_dvfs_enable, true);
	return 0;
}

static int mtk_capture_alsa_start(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);

	/* here to set interrupt */
	irq_add_user(substream,
		     Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE,
		     substream->runtime->rate,
		     substream->runtime->period_size);
	irq_user_id = substream;
	/* set memory */
	SetSampleRate(Soc_Aud_Digital_Block_MEM_VUL, substream->runtime->rate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL, true);

	EnableAfe(true);
	return 0;
}

static int mtk_capture_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_aud("mtk_capture_pcm_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_capture_alsa_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_capture_alsa_stop(substream);
	}
	return -EINVAL;
}

static int mtk_capture_pcm_copy(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				void __user *dst, snd_pcm_uframes_t count)
{
	vcore_dvfs(&vcore_dvfs_enable, false);
	return mtk_memblk_copy(substream, channel, pos, dst, count, VUL_Control_context, Soc_Aud_Digital_Block_MEM_VUL);
}

static int mtk_capture_pcm_silence(struct snd_pcm_substream *substream,
				   int channel, snd_pcm_uframes_t pos,
				   snd_pcm_uframes_t count)
{
	pr_warn("dummy_pcm_silence\n");
	return 0; /* do nothing */
}


static void *dummy_page[2];

static struct page *mtk_capture_pcm_page(struct snd_pcm_substream *substream,
					 unsigned long offset)
{
	pr_warn("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}


static struct snd_pcm_ops mtk_afe_capture_ops = {
	.open =     mtk_capture_pcm_open,
	.close =    mtk_capture_pcm_close,
	.ioctl =    snd_pcm_lib_ioctl,
	.hw_params =    mtk_capture_pcm_hw_params,
	.hw_free =  mtk_capture_pcm_hw_free,
	.prepare =  mtk_capture_pcm_prepare,
	.trigger =  mtk_capture_pcm_trigger,
	.pointer =  mtk_capture_pcm_pointer,
	.copy =     mtk_capture_pcm_copy,
	.silence =  mtk_capture_pcm_silence,
	.page =     mtk_capture_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_platform = {
	.ops        = &mtk_afe_capture_ops,
	.pcm_new    = mtk_asoc_capture_pcm_new,
	.probe      = mtk_afe_capture_probe,
};

static int mtk_capture_probe(struct platform_device *pdev)
{
	pr_warn("mtk_capture_probe\n");

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_UL1_PCM);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_platform);
}

static int mtk_asoc_capture_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	pr_warn("mtk_asoc_capture_pcm_new\n");
	return 0;
}


static int mtk_afe_capture_probe(struct snd_soc_platform *platform)
{
	pr_warn("mtk_afe_capture_probe\n");
	snd_soc_add_platform_controls(platform, Audio_snd_capture_controls,
					 ARRAY_SIZE(Audio_snd_capture_controls));
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_VUL, UL1_MAX_BUFFER_SIZE);
	Capture_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_VUL);
	mAudioDigitalI2S =  kzalloc(sizeof(AudioDigtalI2S), GFP_KERNEL);
	return 0;
}


static int mtk_capture_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_capture_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_pcm_capture", },
	{}
};
#endif

static struct platform_driver mtk_afe_capture_driver = {
	.driver = {
		.name = MT_SOC_UL1_PCM,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_soc_pcm_capture_of_ids,
#endif
	},
	.probe = mtk_capture_probe,
	.remove = mtk_capture_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkafe_capture_dev;
#endif

static int __init mtk_soc_capture_platform_init(void)
{
	int ret = 0;

	pr_warn("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkafe_capture_dev = platform_device_alloc(MT_SOC_UL1_PCM, -1);
	if (!soc_mtkafe_capture_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkafe_capture_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkafe_capture_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_afe_capture_driver);
	return ret;
}
module_init(mtk_soc_capture_platform_init);

static void __exit mtk_soc_platform_exit(void)
{

	pr_warn("%s\n", __func__);
	platform_driver_unregister(&mtk_afe_capture_driver);
}

module_exit(mtk_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
