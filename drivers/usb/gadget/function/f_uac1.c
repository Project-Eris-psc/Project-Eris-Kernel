/*
 * f_audio.c -- USB Audio class function driver
  *
 * Copyright (C) 2008 Bryan Wu <cooloney@kernel.org>
 * Copyright (C) 2008 Analog Devices, Inc
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/atomic.h>

#include "u_uac1.c"

#define INPUT_TERMINAL_STREAM_ID	1
#define INPUT_TERMINAL_MICROPHONE_ID	2
#define OUTPUT_TERMINAL_SPEAKER_ID	6
#define OUTPUT_TERMINAL_STREAM_ID	7
#define SELECTOR_UNIT_ID		8
#define FEATURE_UNIT_0_ID		9
#define FEATURE_UNIT_1_ID		10

#define OUT_EP_MAX_PACKET_SIZE	512
static int out_req_buf_size = OUT_EP_MAX_PACKET_SIZE;
module_param(out_req_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(out_req_buf_size, "ISO OUT endpoint request buffer size");

#define OUT_EP_REQUEST_COUNT	256
static int out_req_count = OUT_EP_REQUEST_COUNT;
module_param(out_req_count, int, S_IRUGO);
MODULE_PARM_DESC(out_req_count, "ISO OUT endpoint request count");

#define IN_EP_MAX_PACKET_SIZE	100
static int in_req_buf_size = IN_EP_MAX_PACKET_SIZE;
module_param(in_req_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(in_req_buf_size, "ISO IN endpoint request buffer size");

#define IN_EP_REQUEST_COUNT	128
static int in_req_count = IN_EP_REQUEST_COUNT;
module_param(in_req_count, int, S_IRUGO);
MODULE_PARM_DESC(in_req_count, "ISO IN endpoint request count");

static int audio_buf_size = UAC1_PLAYBACK_BUFFER_SIZE;
module_param(audio_buf_size, int, S_IRUGO);
MODULE_PARM_DESC(audio_buf_size, "Audio buffer size");

static int generic_set_cmd(struct usb_audio_control *con, u8 cmd, int value);
static int generic_get_cmd(struct usb_audio_control *con, u8 cmd);

static struct f_audio *g_audio;
static int perframe_capturesize;

enum {
	STR_UAC1_MAIN,
	STR_UAC1_OUT_ALT0,
	STR_UAC1_OUT_ALT1,
	STR_UAC1_IN_ALT0,
	STR_UAC1_IN_ALT1,
};

static struct usb_string strings_uac1dev[] = {
	[STR_UAC1_MAIN].s = "MTK UAC 1.0",
	[STR_UAC1_OUT_ALT0].s = "Playback Inactive",
	[STR_UAC1_OUT_ALT1].s = "Playback Active",
	[STR_UAC1_IN_ALT0].s = "Capture Inactive",
	[STR_UAC1_IN_ALT1].s = "Capture Active",
	{  } /* end of list */
};

static struct usb_gadget_strings stringtab_uac1dev = {
	.language = 0x0409,	/* en-us */
	.strings = strings_uac1dev,
};

static struct usb_gadget_strings *uac1_strings[] = {
	&stringtab_uac1dev,
	NULL,
};

/*
 * DESCRIPTORS ... most are static, but strings and full
 * configuration descriptors are built on demand.
 */

/*
 * We have two interfaces- AudioControl and AudioStreaming
 * TODO: only supcard playback currently
 */
#define F_AUDIO_OUT_INTERFACE	1
#define F_AUDIO_IN_INTERFACE	2
#define F_AUDIO_NUM_INTERFACES	2

/* B.3.1  Standard AC Interface Descriptor */
static struct usb_interface_descriptor uac1_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOCONTROL,
};

static struct uac_input_terminal_descriptor input_terminal_stream_desc = {
	.bLength =		UAC_DT_INPUT_TERMINAL_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_INPUT_TERMINAL,
	.bTerminalID =		INPUT_TERMINAL_STREAM_ID,
	.wTerminalType =	UAC_TERMINAL_STREAMING,
	.bAssocTerminal =	0,
};

static struct uac_input_terminal_descriptor input_terminal_microphone_desc = {
	.bLength =		UAC_DT_INPUT_TERMINAL_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_INPUT_TERMINAL,
	.bTerminalID =		INPUT_TERMINAL_MICROPHONE_ID,
	.wTerminalType =	UAC_INPUT_TERMINAL_MICROPHONE,
	.bAssocTerminal =	0,
};

#define UAC_SELECTOR_UNIT_DESCRIPTOR(ch)			\
struct uac_selector_unit_descriptor_##ch {			\
	__u8 bLength;						\
	__u8 bDescriptorType;				\
	__u8 bDescriptorSubtype;			\
	__u8 bUintID;						\
	__u8 bNrInPins;						\
	__u8 baSourceID[ch + 1];					\
	__u8 iSelector;						\
} __attribute__ ((packed))

UAC_SELECTOR_UNIT_DESCRIPTOR(1);

#define UAC_SELECTOR_UNIT_SIZE(ch) (sizeof(struct uac_selector_unit_descriptor) + 1 + ch)

static struct uac_selector_unit_descriptor_1 output_selector_unit_desc = {
	.bLength = UAC_SELECTOR_UNIT_SIZE(1),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubtype = UAC_SELECTOR_UNIT,
	.bUintID = SELECTOR_UNIT_ID,
	.bNrInPins = 1,
	.baSourceID[0] = FEATURE_UNIT_1_ID,
	.iSelector = 0,
};

DECLARE_UAC_FEATURE_UNIT_DESCRIPTOR(0);

static struct uac_feature_unit_descriptor_0 feature_unit_0_desc = {
	.bLength		= UAC_DT_FEATURE_UNIT_SIZE(0),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_FEATURE_UNIT,
	.bUnitID		= FEATURE_UNIT_0_ID,
	.bSourceID		= INPUT_TERMINAL_STREAM_ID,
	.bControlSize		= 2,
	.bmaControls[0]		= (UAC_FU_MUTE | UAC_FU_VOLUME),
};

DECLARE_UAC_FEATURE_UNIT_DESCRIPTOR(1);

static struct uac_feature_unit_descriptor_1 feature_unit_1_desc = {
	.bLength		= UAC_DT_FEATURE_UNIT_SIZE(1),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_FEATURE_UNIT,
	.bUnitID		= FEATURE_UNIT_1_ID,
	.bSourceID		= INPUT_TERMINAL_MICROPHONE_ID,
	.bControlSize		= 2,
	.bmaControls[0]		= (UAC_FU_MUTE | UAC_FU_VOLUME),
	.bmaControls[1] 	= 0,
};

static struct usb_audio_control mute_control = {
	.list = LIST_HEAD_INIT(mute_control.list),
	.name = "Mute Control",
	.type = UAC_FU_MUTE,
	/* Todo: add real Mute control code */
	.set = generic_set_cmd,
	.get = generic_get_cmd,
};

static struct usb_audio_control volume_control = {
	.list = LIST_HEAD_INIT(volume_control.list),
	.name = "Volume Control",
	.type = UAC_FU_VOLUME,
	/* Todo: add real Volume control code */
	.set = generic_set_cmd,
	.get = generic_get_cmd,
};

static struct usb_audio_control_selector feature_0_unit = {
	.list = LIST_HEAD_INIT(feature_0_unit.list),
	.id = FEATURE_UNIT_0_ID,
	.name = "Mute & Volume Control",
	.type = UAC_FEATURE_UNIT,
	.desc = (struct usb_descriptor_header *)&feature_unit_0_desc,
};

static struct usb_audio_control_selector feature_1_unit = {
	.list = LIST_HEAD_INIT(feature_1_unit.list),
	.id = FEATURE_UNIT_1_ID,
	.name = "Mute & Volume Control",
	.type = UAC_FEATURE_UNIT,
	.desc = (struct usb_descriptor_header *)&feature_unit_1_desc,
};

static struct uac1_output_terminal_descriptor output_terminal_speaker_desc = {
	.bLength		= UAC_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_OUTPUT_TERMINAL,
	.bTerminalID		= OUTPUT_TERMINAL_SPEAKER_ID,
	.wTerminalType		= UAC_OUTPUT_TERMINAL_SPEAKER,
	.bAssocTerminal		= 0,
	.bSourceID		= FEATURE_UNIT_0_ID,
};

static struct uac1_output_terminal_descriptor output_terminal_stream_desc = {
	.bLength		= UAC_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_OUTPUT_TERMINAL,
	.bTerminalID		= OUTPUT_TERMINAL_STREAM_ID,
	.wTerminalType		= UAC_TERMINAL_STREAMING,
	.bAssocTerminal		= 0,
	.bSourceID		= SELECTOR_UNIT_ID,
};

/* B.4.1  Standard AS Interface Descriptor */
static struct usb_interface_descriptor as_interface_alt_0_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

static struct usb_interface_descriptor as_interface_alt_1_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	1,
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

static struct usb_interface_descriptor as_interface_alt_2_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

static struct usb_interface_descriptor as_interface_alt_3_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	1,
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

/* B.4.2  Class-Specific AS Interface Descriptor */
static struct uac1_as_header_descriptor as_header_desc = {
	.bLength =		UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_AS_GENERAL,
	.bTerminalLink =	INPUT_TERMINAL_STREAM_ID,
	.bDelay =		1,
	.wFormatTag =		UAC_FORMAT_TYPE_I_PCM,
};

static struct uac1_as_header_descriptor as_header_in_desc = {
	.bLength =		UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_AS_GENERAL,
	.bTerminalLink =	OUTPUT_TERMINAL_STREAM_ID,
	.bDelay =		1,
	.wFormatTag =		UAC_FORMAT_TYPE_I_PCM,
};

DECLARE_UAC_FORMAT_TYPE_I_DISCRETE_DESC(1);

static struct uac_format_type_i_discrete_descriptor_1 as_type_i_desc = {
	.bLength =		UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(1),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_FORMAT_TYPE,
	.bFormatType =		UAC_FORMAT_TYPE_I,
	.bSubframeSize =	2,
	.bBitResolution =	16,
	.bSamFreqType =		1,
};

static struct uac_format_type_i_discrete_descriptor_1 as_type_i_in_desc = {
	.bLength =		UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(1),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_FORMAT_TYPE,
	.bFormatType =		UAC_FORMAT_TYPE_I,
	.bSubframeSize =	2,
	.bBitResolution =	16,
	.bSamFreqType =		1,
};

/* Standard ISO OUT Endpoint Descriptor */
static struct usb_endpoint_descriptor as_out_ep_desc  = {
	.bLength =		USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_SYNC_ADAPTIVE
				| USB_ENDPOINT_XFER_ISOC,
	.wMaxPacketSize =	__constant_cpu_to_le16(OUT_EP_MAX_PACKET_SIZE),
	.bInterval =		1,
};

/* Standard ISO in Endpoint Descriptor */
static struct usb_endpoint_descriptor as_in_ep_desc  = {
	.bLength =		USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_SYNC_ADAPTIVE
				| USB_ENDPOINT_XFER_ISOC,
	.wMaxPacketSize =	__constant_cpu_to_le16(IN_EP_MAX_PACKET_SIZE),
	.bInterval =		1,
};

/* Class-specific AS ISO OUT Endpoint Descriptor */
static struct uac_iso_endpoint_descriptor as_iso_out_desc = {
	.bLength =		UAC_ISO_ENDPOINT_DESC_SIZE,
	.bDescriptorType =	USB_DT_CS_ENDPOINT,
	.bDescriptorSubtype =	UAC_EP_GENERAL,
	.bmAttributes = 	1,
	.bLockDelayUnits =	1,
	.wLockDelay =		__constant_cpu_to_le16(1),
};

/* Class-specific AS ISO IN Endpoint Descriptor */
static struct uac_iso_endpoint_descriptor as_iso_in_desc = {
	.bLength =		UAC_ISO_ENDPOINT_DESC_SIZE,
	.bDescriptorType =	USB_DT_CS_ENDPOINT,
	.bDescriptorSubtype =	UAC_EP_GENERAL,
	.bmAttributes = 	1,
	.bLockDelayUnits =	0,
	.wLockDelay =		0,
};

DECLARE_UAC_AC_HEADER_DESCRIPTOR(2);

#define UAC_DT_AC_HEADER_LENGTH	UAC_DT_AC_HEADER_SIZE(F_AUDIO_NUM_INTERFACES)
/* 1 input terminal, 1 output terminal and 1 feature unit */
#define UAC_DT_TOTAL_LENGTH (UAC_DT_AC_HEADER_LENGTH + UAC_DT_INPUT_TERMINAL_SIZE \
	+ UAC_DT_INPUT_TERMINAL_SIZE + UAC_DT_OUTPUT_TERMINAL_SIZE \
	+ UAC_DT_OUTPUT_TERMINAL_SIZE + UAC_DT_FEATURE_UNIT_SIZE(0) \
	+ UAC_DT_FEATURE_UNIT_SIZE(1) + UAC_SELECTOR_UNIT_SIZE(1))
/* B.3.2  Class-Specific AC Interface Descriptor */
static struct uac1_ac_header_descriptor_2 uac1_header_desc = {
	.bLength =		UAC_DT_AC_HEADER_LENGTH,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_HEADER,
	.bcdADC =		__constant_cpu_to_le16(0x0100),
	.wTotalLength =		__constant_cpu_to_le16(UAC_DT_TOTAL_LENGTH),
	.bInCollection =	F_AUDIO_NUM_INTERFACES,
	.baInterfaceNr = {
		[0] =		F_AUDIO_OUT_INTERFACE,
		[1] =		F_AUDIO_IN_INTERFACE,
	}
};

static struct usb_descriptor_header *f_audio_desc[] = {
	(struct usb_descriptor_header *)&uac1_interface_desc,
	(struct usb_descriptor_header *)&uac1_header_desc,

	(struct usb_descriptor_header *)&input_terminal_stream_desc,
	(struct usb_descriptor_header *)&input_terminal_microphone_desc,
	(struct usb_descriptor_header *)&output_terminal_speaker_desc,
	(struct usb_descriptor_header *)&output_terminal_stream_desc,
	(struct usb_descriptor_header *)&output_selector_unit_desc,
	(struct usb_descriptor_header *)&feature_unit_0_desc,
	(struct usb_descriptor_header *)&feature_unit_1_desc,

	(struct usb_descriptor_header *)&as_interface_alt_0_desc,

	(struct usb_descriptor_header *)&as_interface_alt_1_desc,
	(struct usb_descriptor_header *)&as_header_desc,
	(struct usb_descriptor_header *)&as_type_i_desc,
	(struct usb_descriptor_header *)&as_out_ep_desc,
	(struct usb_descriptor_header *)&as_iso_out_desc,

	(struct usb_descriptor_header *)&as_interface_alt_2_desc,

	(struct usb_descriptor_header *)&as_interface_alt_3_desc,
	(struct usb_descriptor_header *)&as_header_in_desc,
	(struct usb_descriptor_header *)&as_type_i_in_desc,
	(struct usb_descriptor_header *)&as_in_ep_desc,
	(struct usb_descriptor_header *)&as_iso_in_desc,


	NULL,
};

/*
 * This function is an ALSA sound card following USB Audio Class Spec 1.0.
 */

/*-------------------------------------------------------------------------*/
struct f_audio_buf {
	u8 *buf;
	int actual;
	struct list_head list;
};

static struct f_audio_buf *f_audio_buffer_alloc(int buf_size)
{
	struct f_audio_buf *copy_buf;

	copy_buf = kzalloc(sizeof *copy_buf, GFP_ATOMIC);
	if (!copy_buf)
		return ERR_PTR(-ENOMEM);

	copy_buf->buf = kzalloc(buf_size, GFP_ATOMIC);
	if (!copy_buf->buf) {
		kfree(copy_buf);
		return ERR_PTR(-ENOMEM);
	}

	return copy_buf;
}

static void f_audio_buffer_free(struct f_audio_buf *audio_buf)
{
	kfree(audio_buf->buf);
	kfree(audio_buf);
}

/*-------------------------------------------------------------------------*/
struct uac1_req {
	struct usb_request *req;
};

#define UAC1_CAPTURE_UNKNOWN               0
#define UAC1_CAPTURE_READY                 1
#define UAC1_CAPTURE_PRE_START             2
#define UAC1_CAPTURE_BUSY                  3
#define UAC1_CAPTURE_PAUSE                 4
#define UAC1_CAPTURE_PRE_OFF               5
#define UAC1_CAPTURE_OFFLINE               6

struct uac1_cdatabuf {
	u8*					pstart;
	u8*					pread;
	u8*					pwrite;
	snd_pcm_uframes_t	avildsize;
	snd_pcm_uframes_t	bufsize;
	snd_pcm_uframes_t	startthre;
};

struct f_audio {
	struct gaudio			card;

	/* endpoints handle full and/or high speeds */
	struct usb_ep			*out_ep;
	struct uac1_req	out_req[OUT_EP_REQUEST_COUNT];

	struct usb_ep			*in_ep;
	struct workqueue_struct *capture_wq;
	struct work_struct capture_buffer_work;
	struct work_struct capture_ep_work;
	wait_queue_head_t capture_wait;
	struct list_head capture_list;
	spinlock_t			capture_req_lock;
	spinlock_t			capture_buf_lock;
	struct uac1_cdatabuf *capture_buf;
	u8 capture_enable;
	u8 capture_status;

	spinlock_t			lock;

	struct f_audio_buf *copy_buf;
	struct work_struct playback_work;
	struct list_head play_queue;
	spinlock_t			list_lock;

	/* Control Set command */
	struct list_head cs;
	u8 set_cmd;
	struct usb_audio_control *set_con;
};

static inline struct f_audio *func_to_audio(struct usb_function *f)
{
	return container_of(f, struct f_audio, card.func);
}

/*-------------------------------------------------------------------------*/
/* add a request to the tail of a list */
static void f_audio_req_put(struct f_audio *audio, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&audio->capture_req_lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&audio->capture_req_lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request
*f_audio_req_get(struct f_audio *audio, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&audio->capture_req_lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&audio->capture_req_lock, flags);
	return req;
}

static void f_audio_ep_free(struct f_audio *audio, struct usb_ep *ep)
{
	struct usb_request *req;
	int i;

	if ((ep->desc != NULL) &&
		usb_endpoint_dir_out(ep->desc)) {
		for (i = 0; i < out_req_count; i++) {
			if (audio->out_req[i].req) {
				usb_ep_dequeue(ep, audio->out_req[i].req);
				kfree(audio->out_req[i].req->buf);
				usb_ep_free_request(ep, audio->out_req[i].req);
				audio->out_req[i].req = NULL;
			}
		}
	}

	if ((ep->desc != NULL) &&
		usb_endpoint_dir_in(ep->desc)) {
		while ((req = f_audio_req_get(audio, &audio->capture_list)) && req) {
			usb_ep_dequeue(ep, req);
			kfree(req->buf);
			usb_ep_free_request(ep, req);
		}
	}

	if (usb_ep_disable(ep))
		pr_err("uac1 %s\n", __func__);
}

static struct uac1_cdatabuf
*f_audio_capture_buf_alloc(struct f_audio *audio)
{
	struct gaudio *card = &audio->card;
	struct gaudio_snd_dev	*snd = &card->capture;
	int cch, cbit, crate;
	int bufsize = 0;
	struct uac1_cdatabuf *ptr;

	pr_info("uac1 enter %s!\n", __func__);

	cch = u_audio_get_capture_channels(snd->card);
	cbit =  u_audio_get_bit_format(snd->card);
	crate = u_audio_get_capture_rate(snd->card);

	perframe_capturesize = cch * cbit * crate / 1000;

	/* buffer 200ms */
	bufsize = perframe_capturesize * 200;
	if (in_req_buf_size > bufsize)
		bufsize = in_req_buf_size * 2;

	spin_lock_irq(&audio->capture_buf_lock);
	ptr = kzalloc(sizeof *ptr, GFP_ATOMIC);
	if (!ptr) {
		pr_info("error, memory not enough[%d]!\n", __LINE__);
		goto exit;
	}
	ptr->pstart = kzalloc(bufsize, GFP_ATOMIC);
	if (!ptr->pstart) {
		pr_info("error, memory not enough[%d]!\n", __LINE__);
		kfree(ptr);
		ptr = NULL;
		goto exit;
	}

	ptr->pread = ptr->pstart;
	ptr->pwrite = ptr->pstart;
	ptr->bufsize = bufsize;
	ptr->avildsize = 0;

	/* buffer 50ms */
	ptr->startthre = cch * cbit * crate * 50 / 1000;

	pr_info("start[0x%lx], buff[0x%lx]!\n", ptr->startthre, ptr->bufsize);

exit:
	spin_unlock_irq(&audio->capture_buf_lock);

	return ptr;
}

static void f_audio_capture_buf_free(struct f_audio *audio)
{
	struct uac1_cdatabuf *pbuf = audio->capture_buf;

	pr_info("uac1 enter %s!\n", __func__);

	spin_lock_irq(&audio->capture_buf_lock);

	if (audio->capture_buf == NULL) {
		spin_unlock_irq(&audio->capture_buf_lock);
		return;
	}

	pbuf->pstart = NULL;
	pbuf->pread = NULL;
	pbuf->pwrite = NULL;
	pbuf->bufsize = 0;
	pbuf->avildsize = 0;
	pbuf->startthre = 0;

	kfree(pbuf->pstart);
	kfree(pbuf);

	audio->capture_buf = NULL;
	spin_unlock_irq(&audio->capture_buf_lock);
}

static u8 f_audio_capture_is_data_availd(struct f_audio *audio)
{
	struct uac1_cdatabuf *pbuf;
	u8 cavaild = 0;

	spin_lock_irq(&audio->capture_buf_lock);
	pbuf = audio->capture_buf;
	if (!pbuf)
		cavaild = 0;
	else if (pbuf->avildsize > 0 )
		cavaild = 1;
	else
		cavaild = 0;
	spin_unlock_irq(&audio->capture_buf_lock);

	return cavaild;
}

static snd_pcm_uframes_t
f_audio_capture_update_readp(struct f_audio *audio, u8 *pdata, snd_pcm_uframes_t size)
{
	struct uac1_cdatabuf *pbuf;
	snd_pcm_uframes_t datasize, tmpsize, sizeremain;
	u8 *pread, *pstart;

	spin_lock_irq(&audio->capture_buf_lock);
	pbuf = audio->capture_buf;
	if (!pbuf)
		return 0;

	pread = pbuf->pread;
	pstart = pbuf->pstart;

	datasize = pbuf->avildsize;

	if (size > datasize)
		pr_err("uac1 capture data not enough. DataSize[0x%lx], ReqSize[0x%lx]\n",
				datasize, size);
	else
		datasize = size;

	tmpsize = pbuf->bufsize - (pread - pstart);

	if (tmpsize >= datasize) {
		memcpy(pdata, pread, datasize);
		if (tmpsize == datasize)
			pread = pstart;
		else
			pread += datasize;
	} else {
		sizeremain = datasize - tmpsize;
		memcpy(pdata, pread, tmpsize);
		pread = pstart;
		memcpy((pdata + tmpsize), pread, sizeremain);
		pread += sizeremain;
	}

	pbuf->avildsize -= datasize;

	pbuf->pread = pread;

	/* pr_info("uac1 read pwrite[%p] pread[%p] pstart[%p]\n",
			pbuf->pwrite, pbuf->pread, pbuf->pstart); */

	spin_unlock_irq(&audio->capture_buf_lock);

	return datasize;
}

static u8
f_audio_capture_update_writep(struct f_audio *audio, u8 *pdata, snd_pcm_uframes_t size)
{
	struct uac1_cdatabuf *pbuf = audio->capture_buf;
	snd_pcm_uframes_t sizefree, sizeremain;
	u8 *pwrite, *pread, *pstart;
	u8 ret = 0;

	spin_lock_irq(&audio->capture_buf_lock);
	if (audio->capture_buf == NULL) {
		ret = 1;
		goto exit;
	}

	pwrite = pbuf->pwrite;
	pread = pbuf->pread;
	pstart = pbuf->pstart;

	if (pwrite > pread)
		sizefree = pbuf->bufsize - (pwrite - pread);
	else if (pwrite < pread)
		sizefree = pread - pwrite;
	else
		sizefree = pbuf->bufsize;

	if (sizefree < size) {
		pr_err("uac1 capture overrun. FreeSize[0x%lx], WriteSize[0x%lx]\n",
				sizefree, size);
		pr_info("uac1 write pwrite[%p] pread[%p] pstart[%p]\n",
			pbuf->pwrite, pbuf->pread, pbuf->pstart);
	}

	sizefree = pbuf->bufsize - (pwrite - pstart);

	if (sizefree >= size) {
		memcpy(pwrite, pdata, size);
		if (sizefree == size)
			pwrite = pstart;
		else
			pwrite += size;
	} else {
		sizeremain = size - sizefree;
		memcpy(pwrite, pdata, sizefree);
		pwrite = pstart;
		memcpy(pwrite, (pdata + sizefree), sizeremain);
		pwrite += sizeremain;
	}

	pbuf->avildsize += size;
	if (pbuf->avildsize > pbuf->bufsize)
		pbuf->avildsize = pbuf->bufsize;

	pbuf->pwrite = pwrite;

	/* pr_info("uac1 write pwrite[%p] pread[%p] pstart[%p]\n",
			pbuf->pwrite, pbuf->pread, pbuf->pstart); */

exit:
	spin_unlock_irq(&audio->capture_buf_lock);

	return ret;
}

static void f_audio_playback_work(struct work_struct *data)
{
	struct f_audio *audio = container_of(data, struct f_audio,
					playback_work);
	struct f_audio_buf *play_buf;

	spin_lock_irq(&audio->list_lock);
	if (list_empty(&audio->play_queue)) {
		spin_unlock_irq(&audio->list_lock);
		return;
	}
	play_buf = list_first_entry(&audio->play_queue,
			struct f_audio_buf, list);
	list_del(&play_buf->list);
	spin_unlock_irq(&audio->list_lock);

	u_audio_playback(&audio->card, play_buf->buf, play_buf->actual);
	spin_lock_irq(&audio->list_lock);
	f_audio_buffer_free(play_buf);
	spin_unlock_irq(&audio->list_lock);

	//goto loop0;
}

static void f_audio_capture_ep_work(struct work_struct *data)
{
	struct f_audio *audio = container_of(data, struct f_audio,
					capture_ep_work);
	struct usb_request *req = 0;
	int avild, ret;

	if(!f_audio_capture_is_data_availd(audio)) {
		return;
	}

	spin_lock_irq(&audio->lock);
	if (audio->capture_status != UAC1_CAPTURE_BUSY){
		spin_unlock_irq(&audio->lock);
		return;
	}
	spin_unlock_irq(&audio->lock);

again:
	req = f_audio_req_get(audio, &audio->capture_list);
	if (!req) {
		pr_info("uac1 %s there is no req\n", __func__);
		return;
	}

	avild =	f_audio_capture_update_readp(audio, req->buf, perframe_capturesize);

	spin_lock_irq(&audio->lock);

	req->length = avild;
	req->context = audio;
	ret = usb_ep_queue(audio->in_ep, req, GFP_KERNEL);
	if (ret < 0)
		pr_err("uac1 %s usb_ep_queue fail[%d]\n", __func__, ret);

	spin_unlock_irq(&audio->lock);

	if(f_audio_capture_is_data_availd(audio))
		goto again;
}


static void f_audio_capture_buffer_work(struct work_struct *data)
{
	struct f_audio *audio = container_of(data, struct f_audio,
					capture_buffer_work);
	struct uac1_cdatabuf *pbuf;
	u8 *pdata;
	//u8 cnt;
	int ret;

	pr_info("uac1 enter %s\n", __func__);

	pdata =	kzalloc(in_req_buf_size, GFP_ATOMIC);
	if (!pdata) {
		pr_err("uac1 alloc error %s\n", __func__);
		return;
	}

	while (audio->capture_enable) {
		ret = wait_event_interruptible(audio->capture_wait,
			(((audio->capture_buf != NULL) &&
			((audio->capture_status == UAC1_CAPTURE_PRE_START) ||
			(audio->capture_status == UAC1_CAPTURE_BUSY))) ||
			(audio->capture_status == UAC1_CAPTURE_PRE_OFF)));

		if (audio->capture_status == UAC1_CAPTURE_PRE_OFF)
			break;

		memset(pdata, 0, in_req_buf_size);
		ret = u_audio_capture(&audio->card, pdata, perframe_capturesize);
		if (ret < 0) {
			pr_err("uac1 %s u_audio_capture fail[%d]\n", __func__, ret);
			break;
		}

		if (f_audio_capture_update_writep(audio, pdata, perframe_capturesize))
			continue;

		//for(cnt = f_audio_capture_start_cnt(audio); cnt !=0; cnt--)
		spin_lock_irq(&audio->capture_buf_lock);
		pbuf = audio->capture_buf;
		if (pbuf == NULL) {
			spin_unlock_irq(&audio->capture_buf_lock);
			continue;
		}
		spin_unlock_irq(&audio->capture_buf_lock);

		spin_lock_irq(&audio->lock);

		if ((audio->capture_status == UAC1_CAPTURE_PRE_START) &&
			(pbuf->startthre <= pbuf->avildsize)) {
			audio->capture_status = UAC1_CAPTURE_BUSY;
			schedule_work(&audio->capture_ep_work);
		} else if (audio->capture_status == UAC1_CAPTURE_BUSY) {
			schedule_work(&audio->capture_ep_work);
		}
		spin_unlock_irq(&audio->lock);
	}

	kfree(pdata);
	audio->capture_status = UAC1_CAPTURE_OFFLINE;

	pr_info("uac1 exit %s\n", __func__);
}


static int f_audio_out_ep_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_audio *audio = req->context;
	struct f_audio_buf *copy_buf = audio->copy_buf;
	int err;

	if (!copy_buf)
		return -EINVAL;

	/* Copy buffer is full, add it to the play_queue */
	if (audio_buf_size - copy_buf->actual < req->actual) {
		list_add_tail(&copy_buf->list, &audio->play_queue);
		schedule_work(&audio->playback_work);
		copy_buf = f_audio_buffer_alloc(audio_buf_size);
		if (IS_ERR(copy_buf))
			return -ENOMEM;
	}

	memcpy(copy_buf->buf + copy_buf->actual, req->buf, req->actual);
	copy_buf->actual += req->actual;
	audio->copy_buf = copy_buf;

	err = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (err)
		pr_err("uac1 %s queue req: %d\n", ep->name, err);

	return 0;

}

static int f_audio_in_ep_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_audio *audio = req->context;

	memset(req->buf, 0, in_req_buf_size);
	f_audio_req_put(audio, &audio->capture_list, req);

	return 0;
}

static void f_audio_set_epin_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_audio *audio = req->context;
	int uac1_rate;

	memset(&uac1_rate, 0, sizeof(int));
	memcpy(&uac1_rate, req->buf, req->actual);

	pr_info("uac1 set cur %uHz!\n", uac1_rate);

	if(audio->capture_status == UAC1_CAPTURE_PRE_START) {
		audio->capture_buf = f_audio_capture_buf_alloc(audio);
		wake_up(&audio->capture_wait);
	}
}

static void f_audio_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_audio *audio = req->context;
	int status = req->status;
	u32 data = 0;
	struct usb_ep *out_ep = audio->out_ep;
	struct usb_ep *in_ep = audio->in_ep;

	switch (status) {
	case -EOVERFLOW:
		pr_info("uac1 overrun\n");
	case 0:				/* normal completion? */
		if (ep == out_ep)
			f_audio_out_ep_complete(ep, req);
		else if (ep == in_ep)
			f_audio_in_ep_complete(ep, req);
		else if (audio->set_con) {
			memcpy(&data, req->buf, req->length);
			audio->set_con->set(audio->set_con, audio->set_cmd,
					le16_to_cpu(data));
			audio->set_con = NULL;
		}
		break;
	case -ECONNRESET:
		pr_info("uac1 %s ECONNRESET, ep[%p], outep[%p] inep[%p]\n", __func__, ep, out_ep, in_ep);
		break;
	default:
		pr_info("uac1 %s status %d, ep[%p], outep[%p] inep[%p]\n", __func__, status, ep, out_ep, in_ep);
		break;
	}
}

static int audio_set_intf_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_audio		*audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	u8			id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u8			con_sel = (w_value >> 8) & 0xFF;
	u8			cmd = (ctrl->bRequest & 0x0F);
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;

	pr_info("uac1 bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
			ctrl->bRequest, w_value, len, id);

	list_for_each_entry(cs, &audio->cs, list) {
		if (cs->id == id) {
			list_for_each_entry(con, &cs->control, list) {
				if (con->type == con_sel) {
					audio->set_con = con;
					break;
				}
			}
			break;
		}
	}

	audio->set_cmd = cmd;
	req->context = audio;
	req->complete = f_audio_complete;

	return len;
}

static int audio_get_intf_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_audio		*audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u8			id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u8			con_sel = (w_value >> 8) & 0xFF;
	u8			cmd = (ctrl->bRequest & 0x0F);
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;

	pr_info("uac1 bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
			ctrl->bRequest, w_value, len, id);

	list_for_each_entry(cs, &audio->cs, list) {
		if (cs->id == id) {
			list_for_each_entry(con, &cs->control, list) {
				if (con->type == con_sel && con->get) {
					value = con->get(con, cmd);
					break;
				}
			}
			break;
		}
	}

	req->context = audio;
	req->complete = f_audio_complete;
	len = min_t(size_t, sizeof(value), len);
	memcpy(req->buf, &value, len);

	return len;
}

static int audio_set_endpoint_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_audio		*audio = func_to_audio(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			ep = le16_to_cpu(ctrl->wIndex);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);

	pr_info("uac1 set bRequest 0x%x, w_value 0x%04x, len %d, endpoint %d\n",
			ctrl->bRequest, w_value, len, ep);

	switch (ctrl->bRequest) {
	case UAC_SET_CUR:
		req->context = audio;
		value = len;
		if ((ep & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN)
			req->complete = f_audio_set_epin_complete;
		else
			req->complete = f_audio_complete;
		break;

	case UAC_SET_MIN:
		break;

	case UAC_SET_MAX:
		break;

	case UAC_SET_RES:
		break;

	case UAC_SET_MEM:
		break;

	default:
		break;
	}

	return value;
}

static int audio_get_endpoint_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	int value = -EOPNOTSUPP;
	u16 len = le16_to_cpu(ctrl->wLength);

	switch (ctrl->bRequest) {
	case UAC_GET_CUR:
	case UAC_GET_MIN:
	case UAC_GET_MAX:
	case UAC_GET_RES:
		value = len;
		break;
	case UAC_GET_MEM:
		break;
	default:
		break;
	}

	return value;
}

static int
f_audio_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	/* composite driver infrastructure handles everything; interface
	 * activation uses set_alt().
	 */
	switch (ctrl->bRequestType) {
	case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
		value = audio_set_intf_req(f, ctrl);
		break;

	case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
		value = audio_get_intf_req(f, ctrl);
		break;

	case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
		value = audio_set_endpoint_req(f, ctrl);
		break;

	case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
		value = audio_get_endpoint_req(f, ctrl);
		break;

	default:
		pr_err("uac1 invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		/*pr_info("uac1 audio req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);*/
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			pr_err("uac1 audio response on err %d\n", value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int f_audio_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_gadget *gadget = cdev->gadget;
	struct f_audio		*audio = func_to_audio(f);
	struct usb_ep *out_ep = audio->out_ep;
	struct usb_ep *in_ep = audio->in_ep;
	struct f_audio_buf *copy_buf, *tmpbuf;
	struct usb_request *req;
	int i = 0, err = 0;

	pr_info("uac1 intf %d, alt %d\n", intf, alt);

	if (intf == 1) {
		config_ep_by_speed(gadget, f, out_ep);
		if (alt == 1) {
			usb_ep_enable(out_ep);
			out_ep->driver_data = audio;
			audio->copy_buf = f_audio_buffer_alloc(audio_buf_size);
			if (IS_ERR(audio->copy_buf))
				return -ENOMEM;

			/*
			 * allocate a bunch of read buffers
			 * and queue them all at once.
			 */
			for (i = 0; i < out_req_count && err == 0; i++) {
				if (audio->out_req[i].req) {
					err = usb_ep_queue(out_ep, audio->out_req[i].req, GFP_ATOMIC);
					if (err)
						pr_err("uac1 %s queue req: %d\n", out_ep->name, err);
					continue;
				}

				req = usb_ep_alloc_request(out_ep, GFP_ATOMIC);
				if (req) {
					req->buf = kzalloc(out_req_buf_size,
							GFP_ATOMIC);
					if (req->buf) {
						audio->out_req[i].req = req;
						req->length = out_req_buf_size;
						req->context = audio;
						req->complete =
							f_audio_complete;
						err = usb_ep_queue(out_ep,
							req, GFP_ATOMIC);
						if (err)
							pr_err("uac1 %s queue req: %d\n",
							out_ep->name, err);
					} else
						err = -ENOMEM;
				} else
					err = -ENOMEM;
			}

		} else {
			f_audio_ep_free(audio, out_ep);

			spin_lock_irq(&audio->list_lock);
			copy_buf = audio->copy_buf;
			if (copy_buf) {
				audio->copy_buf = NULL;
				f_audio_buffer_free(copy_buf);
			}

			if (!list_empty(&audio->play_queue)) {
				list_for_each_entry_safe(copy_buf, tmpbuf,
					&audio->play_queue, list) {
					list_del(&copy_buf->list);
					f_audio_buffer_free(copy_buf);
				}
			}
			spin_unlock_irq(&audio->list_lock);
		}
	}

	if (intf == 2) {
		config_ep_by_speed(gadget, f, in_ep);
		if (alt == 1) {
			usb_ep_enable(in_ep);
			in_ep->driver_data = audio;

			for (i = 0; i < in_req_count && err == 0; i++) {
				req = usb_ep_alloc_request(in_ep, GFP_ATOMIC);
				if (req) {
					req->buf = kzalloc(in_req_buf_size,
							GFP_ATOMIC);
					if (req->buf) {
						req->context = audio;
						req->complete =
							f_audio_complete;
						f_audio_req_put(audio, &audio->capture_list, req);
						audio->capture_status = UAC1_CAPTURE_PRE_START;
					} else
						err = -ENOMEM;
				} else
					err = -ENOMEM;
			}
		} else {
			spin_lock_irq(&audio->lock);
			audio->capture_status = UAC1_CAPTURE_PAUSE;
			f_audio_capture_buf_free(audio);
			f_audio_ep_free(audio, in_ep);
			spin_unlock_irq(&audio->lock);
		}
	}

	return err;
}

void audio_disconnect(void)
{
	struct f_audio *audio = g_audio;
	struct usb_ep *out_ep = audio->out_ep;
	struct usb_ep *in_ep = audio->in_ep;
	struct f_audio_buf *copy_buf, *tmpbuf;

	/* stop playback */
	spin_lock_irq(&audio->lock);
	f_audio_ep_free(audio, out_ep);
	spin_unlock_irq(&audio->lock);

	spin_lock_irq(&audio->list_lock);
	copy_buf = audio->copy_buf;
	if (copy_buf) {
		audio->copy_buf = NULL;
		f_audio_buffer_free(copy_buf);
	}

	if (!list_empty(&audio->play_queue)) {
		list_for_each_entry_safe(copy_buf, tmpbuf,
			&audio->play_queue, list) {
			list_del(&copy_buf->list);
			f_audio_buffer_free(copy_buf);
		}
	}
	spin_unlock_irq(&audio->list_lock);
	/* end */

	/* stop capture */
	spin_lock_irq(&audio->lock);
	audio->capture_status = UAC1_CAPTURE_PAUSE;
	f_audio_capture_buf_free(audio);
	f_audio_ep_free(audio, in_ep);
	spin_unlock_irq(&audio->lock);
	/* end */
}

static void f_audio_disable(struct usb_function *f)
{
	return;
}

/*-------------------------------------------------------------------------*/

static void f_audio_build_desc(struct f_audio *audio)
{
	struct gaudio *card = &audio->card;
	u8 *sam_freq;
	u8 i;
	int rate;

	/* Set channel numbers */
	as_type_i_desc.bNrChannels = u_audio_get_playback_channels(card);

	input_terminal_stream_desc.bNrChannels = u_audio_get_capture_channels(card);
	input_terminal_microphone_desc.bNrChannels = input_terminal_stream_desc.bNrChannels;
	as_type_i_in_desc.bNrChannels = input_terminal_stream_desc.bNrChannels;
	input_terminal_stream_desc.wChannelConfig = 0;
	for (i = 0; i < input_terminal_stream_desc.bNrChannels; i ++)
		input_terminal_stream_desc.wChannelConfig |= 1 << i;
	input_terminal_microphone_desc.wChannelConfig = input_terminal_stream_desc.wChannelConfig;
	pr_err("channel num[%d] config[%d]",input_terminal_stream_desc.bNrChannels, input_terminal_stream_desc.wChannelConfig);

	/* Set sample rates */
	rate = u_audio_get_playback_rate(card);
	sam_freq = as_type_i_desc.tSamFreq[0];
	memcpy(sam_freq, &rate, 3);

	sam_freq = as_type_i_in_desc.tSamFreq[0];
	memcpy(sam_freq, &rate, 3);

	/* Todo: Set Sample bits and other parameters */

	return;
}

/* audio function driver setup/binding */
static int
f_audio_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_audio		*audio = func_to_audio(f);
	int			status;
	struct usb_ep		*ep = NULL;
	struct usb_string *us;

	f_audio_build_desc(audio);

	us = usb_gstrings_attach(cdev, uac1_strings, ARRAY_SIZE(strings_uac1dev));
	if (IS_ERR(us))
		return PTR_ERR(us);
	uac1_interface_desc.iInterface = us[STR_UAC1_MAIN].id;
	as_interface_alt_0_desc.iInterface = us[STR_UAC1_OUT_ALT0].id;
	as_interface_alt_1_desc.iInterface = us[STR_UAC1_OUT_ALT1].id;
	as_interface_alt_2_desc.iInterface = us[STR_UAC1_IN_ALT0].id;
	as_interface_alt_3_desc.iInterface = us[STR_UAC1_IN_ALT1].id;

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	uac1_interface_desc.bInterfaceNumber = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	as_interface_alt_0_desc.bInterfaceNumber = status;
	as_interface_alt_1_desc.bInterfaceNumber = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	as_interface_alt_2_desc.bInterfaceNumber = status;
	as_interface_alt_3_desc.bInterfaceNumber = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &as_out_ep_desc);
	if (!ep)
		goto fail;
	audio->out_ep = ep;
	audio->out_ep->desc = &as_out_ep_desc;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &as_in_ep_desc);
	if (!ep)
		goto fail;
	audio->in_ep = ep;
	audio->in_ep->desc = &as_in_ep_desc;
	ep->driver_data = cdev;	/* claim */

	status = -ENOMEM;

	/* copy descriptors, and track endpoint copies */
	status = usb_assign_descriptors(f, f_audio_desc, NULL, NULL);
	if (status)
		goto fail;
	return 0;

fail:
	if (ep)
		ep->driver_data = NULL;
	return status;
}

static void
f_audio_unbind(struct usb_configuration *c, struct usb_function *f)
{
	usb_free_all_descriptors(f);
}

/*-------------------------------------------------------------------------*/

static int generic_set_cmd(struct usb_audio_control *con, u8 cmd, int value)
{
	con->data[cmd] = value;

	return 0;
}

static int generic_get_cmd(struct usb_audio_control *con, u8 cmd)
{
	return con->data[cmd];
}

/* Todo: add more control selecotor dynamically */
int control_selector_init(struct f_audio *audio)
{
	INIT_LIST_HEAD(&audio->cs);
	list_add(&feature_0_unit.list, &audio->cs);
	list_add(&feature_1_unit.list, &audio->cs);

	INIT_LIST_HEAD(&feature_0_unit.control);
	INIT_LIST_HEAD(&feature_1_unit.control);
	list_add(&mute_control.list, &feature_0_unit.control);
	list_add(&volume_control.list, &feature_0_unit.control);
	list_add(&mute_control.list, &feature_1_unit.control);
	list_add(&volume_control.list, &feature_1_unit.control);

	volume_control.data[UAC__CUR] = 0xffc0;
	volume_control.data[UAC__MIN] = 0xe3a0;
	volume_control.data[UAC__MAX] = 0xfff0;
	volume_control.data[UAC__RES] = 0x0030;

	return 0;
}

/**
 * audio_bind_config - add USB audio function to a configuration
 * @c: the configuration to supcard the USB audio function
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 */
int audio_bind_config(struct usb_configuration *c)
{
	int status;

	/* allocate and initialize one new instance */
	g_audio = kzalloc(sizeof *g_audio, GFP_KERNEL);
	if (!g_audio)
		return -ENOMEM;

	g_audio->card.func.name = "g_audio";
	g_audio->card.gadget = c->cdev->gadget;

	INIT_LIST_HEAD(&g_audio->play_queue);
	spin_lock_init(&g_audio->lock);
	spin_lock_init(&g_audio->list_lock);
	spin_lock_init(&g_audio->capture_req_lock);
	spin_lock_init(&g_audio->capture_buf_lock);

	/* set up ASLA audio devices */
	status = gaudio_setup(&g_audio->card);
	if (status < 0)
		goto setup_fail;

	g_audio->capture_status = UAC1_CAPTURE_UNKNOWN;

	if (g_audio->card.capture.substream == NULL) {
		g_audio->capture_status = UAC1_CAPTURE_OFFLINE;
		pr_info("uac1 there is no capture\n");
	}

	g_audio->card.func.strings = uac1_strings;
	g_audio->card.func.bind = f_audio_bind;
	g_audio->card.func.unbind = f_audio_unbind;
	g_audio->card.func.set_alt = f_audio_set_alt;
	g_audio->card.func.setup = f_audio_setup;
	g_audio->card.func.disable = f_audio_disable;

	control_selector_init(g_audio);

	INIT_WORK(&g_audio->playback_work, f_audio_playback_work);

	if (g_audio->capture_status != UAC1_CAPTURE_OFFLINE) {
		g_audio->capture_wq = create_singlethread_workqueue("f_uac1");
		if (!g_audio->capture_wq) {
			pr_err("uac1 create queue fail\n");
			goto add_fail;
		}
		INIT_WORK(&g_audio->capture_buffer_work, f_audio_capture_buffer_work);
		init_waitqueue_head(&g_audio->capture_wait);
		INIT_LIST_HEAD(&g_audio->capture_list);

		INIT_WORK(&g_audio->capture_ep_work, f_audio_capture_ep_work);

		g_audio->capture_enable = 1;
		g_audio->capture_status = UAC1_CAPTURE_READY;
		queue_work(g_audio->capture_wq, &g_audio->capture_buffer_work);
	}

	status = usb_add_function(c, &g_audio->card.func);
	if (status) {
		pr_err("uac1 add function fail[%d]\n", status);
		goto add_fail;
	}

	pr_notice("uac1 audio_buf_size %d, out_req_buf_size %d, out_req_count %d\n",
		audio_buf_size, out_req_buf_size, out_req_count);

	return status;

add_fail:
	gaudio_cleanup();
setup_fail:
	kfree(g_audio);
	return status;
}

static void audio_unbind_config(struct usb_configuration *c)
{
	struct f_audio_buf *copy_buf;

	if (g_audio->capture_status != UAC1_CAPTURE_OFFLINE) {
		g_audio->capture_enable = 0;
		g_audio->capture_status = UAC1_CAPTURE_PRE_OFF;
		wake_up(&g_audio->capture_wait);
		flush_workqueue(g_audio->capture_wq);

		if (g_audio->capture_status != UAC1_CAPTURE_OFFLINE)
			pr_err("uac1 %s UAC1 status[%d] errr\n", __func__, g_audio->capture_status);

		f_audio_ep_free(g_audio, g_audio->in_ep);
	}

	f_audio_ep_free(g_audio, g_audio->out_ep);

	spin_lock_irq(&g_audio->list_lock);
	copy_buf = g_audio->copy_buf;
	if (copy_buf) {
		pr_err("uac1 %s [%d]\n", __func__, __LINE__);
		g_audio->copy_buf = NULL;
		list_add_tail(&copy_buf->list,
				&g_audio->play_queue);
		schedule_work(&g_audio->playback_work);
		pr_err("uac1 %s [%d]\n", __func__, __LINE__);
	}
	spin_unlock_irq(&g_audio->list_lock);
	flush_scheduled_work();

	gaudio_cleanup();

	if (g_audio->capture_wq)
		destroy_workqueue(g_audio->capture_wq);

	kfree(g_audio);
	g_audio = NULL;
}

