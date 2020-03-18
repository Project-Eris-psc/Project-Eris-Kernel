
#ifndef __E_CORE_H__
#define __E_CORE_H__

#include "osal.h"
#include "sccb.h"

//TODO: thread or timer? which one is better.
#define MSC_CHANNEL_SWITCH_USE_THREAD 1

#define ESUCCESS	0x0000
#define	ESTATE		0x3000
#define	ELOCK		0x3001
#define	EMAC		0x3002
#define	EPARAM		0x3003
#define	ETIMEOUTRST	0x3004
#define	ELENGTH		0x3005
#define	ENOTREADY	0x3006
#define	ENOSUCHTYPE	0x3007
#define EDROPPED	0x3008
#define EIDXOOR		0x3009

enum eevent_id {
	EVT_ID_SYNFAIL=0x0,
	EVT_ID_SYNCSUC,
	EVT_ID_INFOGET,
	EVT_ID_TIMEOUT,
	EVT_ID_MAX
};

enum etype_id {
	TYPE_ID_BEGIN=0x0,
	TYPE_ID_AM,
	TYPE_ID_SSID,
	TYPE_ID_PWD,
	TYPE_ID_USER,
	TYPE_ID_PMK,
	TYPE_ID_CUST=0x7F,
	TYPE_ID_MAX=0xFF
};

enum _ELIAN_STAT {
	ELIAN_INIT=0,
	ELIAN_SYNC1,
	ELIAN_SYNC2,
	ELIAN_WAIT_DATA,
	ELIAN_GETTING_DATA,
	ELIAN_DONE,
	ELIAN_USER,
	ELIAN_SYNC_FAIL,
	ELIAN_RX_TIMEDOUT,
	ELIAN_CHKSUM_ERR,
	ELIAN_IDX_OOR,

  ELIAN_STOPED
};

typedef struct _ELIAN_STATUS {
	unsigned char stat;
	unsigned char fixChannel;
	unsigned char cur_channel;
}ELIAN_STATUS;
typedef void (*msc_evt_cb)(unsigned int eid);

struct msc_contex {
	/* event call back, when received ssid&pwd,
	 this function will be call with parameter
	 struct msc_param. */
	msc_evt_cb evt_cb;
#if MSC_CHANNEL_SWITCH_USE_THREAD
	OSAL_THREAD chl_thread;
	int chl_thread_running;
	bool is_thread_run_to_end;
	char is_stop_switch;
	bool got_mCast_packet;
	char find_channel;
	char find_centralCh;
	/* Source MAC Addr of SP, used to filter package. */
	char sa[6];
	unsigned char chl_rev;
#else
	OSAL_TIMER timer;
#endif
	unsigned int chl_num;
	unsigned char *channel;
	struct monitor_info m_info;
	spinlock_t lock;
	int cur_chl;

#define MSC_STATE_STARTED 1
#define MSC_STATE_STOPED 0
	int state;
	void *priv;
  ELIAN_STATUS elian_status;
};

struct etimer {
	void (*func) (unsigned long);
	unsigned long data;
	unsigned long expires;
};

typedef void (*event_cb)(enum eevent_id evt);
typedef	void (*aes128_decrypt_cb)(unsigned char *cipher_blk, unsigned int cipher_blk_size,
				unsigned char *key, unsigned int key_len,
				unsigned char *plain_blk, unsigned int *plain_blk_size);
typedef void (*start_timer_cb)(struct etimer *);
typedef int (*stop_timer_cb)(struct etimer *);

struct efunc_table {
	/* Func pointer used to indicate events from elian. */
	event_cb report_evt;

	start_timer_cb start_timer;
	stop_timer_cb stop_timer;

	/* AES128 decrypt func. */
	aes128_decrypt_cb aes128_decrypt;
};

/* This function is used to initialize ELIAN, this function
 * must be called first before call elian_input().
 * Parameters:
 *   la: Local MAC Address.
 *   tbl: call back functions needed by Elian.
 *   key: AES128 decrypt key, The length must be 16 bytes, if the
 *   key is NULL, ELIAN will use the default key.
 * Return value:
 *   0: success
 *   others: please refer to error code. */
int elian_init(char *la, struct efunc_table *tbl, unsigned char *key);


/* This function is used to reset ELIAN, after reset, Elian
 * Will start to accept and parse data again. NOTES: the Local
 * MAC Address, function table and key will NOT be reseted.
 * Return value:
 *   0: success
 *   others: please refer to error code. */
int elian_reset(void);


/* This function is to accept and parse 802.11 data package.
 * Parameters:
 *   p: the start address of 802.11 header.
 *   len: the length of package.
 * Return value:
 *   0: success
 *   others: please refer to error code. */
int elian_input(char *p, int len);


/* This function is to get the result of ELIAN. like SSID, PASSWORD.
 * Parameters:
 *    id: please refer to the enum etype_id.
 *    buf: buffer use to store the result.
 *    len: the length of the buffer, after this function returned,
 *         this len is the actual length of the info
 * Return value:
 *   0: success
 *   others: please refer to error code. */
int elian_get(enum etype_id id, char *buf, int *len);


/* This function is used to reset ELIAN, except stop the timer. After reset, Elian
 * Will start to accept and parse data again. NOTES: the Local
 * MAC Address, function table and key will NOT be reseted.
 * Return value:
 *   0: success
 *   others: please refer to error code. */
int sc_rst(void);

/* This function is used to stop the timer.
 * Return value:
 *   0: success
 *   others: please refer to error code. */
int elian_stop(void);

/* This function is for customer to get the customize-TLV buf and length.
 * Parameters:
 *      buf:(output) point to the customize-TLV
 *      len:(output) return the length of customize-TLV
 * Return value:
 *   0: success
 *   others: please refer to error code. */
int build_cust_tlv_buf(char **buf, int *len);


/* This function is used to get the code version.
 * Parameters:
 *      ver:(output) buffer to store the version string
 *      len:(input): the buffer length, cannot be shorter than the length of version string
 *
 * Return value:
 *   0: success
 *   others: please refer to error code. */
int get_ver_code(char *ver, int len);


/* This function is used to get the smart connection protocol version.
 * Parameters:
 *      ver:(output) buffer to store the version string
 *      len:(input): the buffer length, cannot be shorter than the length of version string
 *
 * Return value:
 *   0: success
 *   others: please refer to error code. */
int get_ver_proto(char *ver, int len);
#endif

