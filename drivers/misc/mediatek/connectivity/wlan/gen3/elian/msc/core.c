#include "core.h"

/* gen3 API header file */
#include "precomp.h"

unsigned int ElianDebugLevel = ELIAN_DBG_WARN;

#define BMAP_MAX_LEN 14
#define AES_BLK_SIZE 16
#define EAPP_DEFAULT_KEY "McdwCnwCdss2_18p"
#define ELIAN_VER_CODE "1.1.1"
#define ELIAN_VER_PROTO "4.0"
typedef unsigned char BITMAP_T[BMAP_MAX_LEN];	// (0x7F-0x15+1)/8 = 14


#define M80211_HEADER_LEN_MIN 24
#define ETH_MAC_LEN 6
#define ETL_IDX_MIN 0x15	/*the start value of index.*/
#define ETL_IDX_MAX 0x7F	/*the max value of index.*/

#define ETL_TIMEOUT_MAX 45
#define ETL_TOTAL_LEN_MAX 214  /* (0x7f-0x15+1) * 2*/
#define ELIAN_V1_0_FLAG 1

enum etl_fsm_state {
	ETL_STATE_WAIT_SYNC0=0x0,
	ETL_STATE_WAIT_SYNC1,
	ETL_STATE_WAIT_SYNC2,
	ETL_STATE_WAIT_DATA,
	ETL_STATE_DONE,
	ETL_STATE_MAX
};

enum etl_rst_reason {
	REASON_INIT=0x0,
	REASON_USER,
	REASON_SYNC_FAIL,
	REASON_RX_TIMEDOUT,
	REASON_CHKSUM_ERR,
	REASON_IDX_OOR
};

struct etl_hdr {
	unsigned char ta[ETH_MAC_LEN];
	unsigned char ver:4;
	unsigned char rsv1:4;
	unsigned char rsv2;
	unsigned char hl;
	unsigned char dl;
	unsigned char csum[2];
}__attribute__((__packed__));

struct eapp_data {
	char *p;
	int len;
	unsigned char key[AES_BLK_SIZE];
};

struct etl_fsm {
	/* Local MAC addr. */
	char la[ETH_MAC_LEN];

	/* Timer used to reset SC. */
	struct etimer rst_timer;

	/* Call back function table. */
	struct efunc_table tbl;

	enum etl_fsm_state state;

	/* Source MAC Addr of SP, used to filter package. */
	char sa[ETH_MAC_LEN];

	unsigned char sync_succ;

	/* the data len of the input buffer, used to filter package*/
	unsigned short dot11_pkg_len;

	char rx_buf[ETL_TOTAL_LEN_MAX];

	/*bit map used to record received data. */
	BITMAP_T bitmap;
};

static struct etl_fsm efsm;
static struct eapp_data app_data;
static unsigned char syn[3] = {0x12, 0x13, 0x14};
#if 0
static unsigned char eth_bc_addr[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
#endif

extern struct msc_contex  msc_ctx;
extern int msc_get_current_channel_info(void);

static void bmap_set(unsigned char *p, int idx)
{
	p[idx>>3] |= ((unsigned char)0x01)<<(idx&0x7);
	return;
}

#if 0
static void bmap_clr(unsigned char *p, int idx)
{
	p[idx>>3] &= ~(((unsigned char)0x01)<<(idx&0x7));
	return;
}
#endif

static int bmap_test(unsigned char *p, int idx, int span)
{
	int offset = idx;

	while((p[offset>>3] & ((unsigned char)0x1)<<(offset&0x7)) && (offset-idx<span)) {
		offset++;
	}
	return (offset-idx<span)?0:1;
}

static void _lock(void)
{
	return;
}

static void _unlock(void)
{
	return;
}

static int _trylock(void)
{
	return 1;
}

static int _sc_rst(int reason)
{
	DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] reset %d-->0, reason:%d\n", efsm.state, reason));
	efsm.state = ETL_STATE_WAIT_SYNC0;
	memset((char *)&efsm.sa, 0, ETH_MAC_LEN);
	efsm.sync_succ = 0;
	efsm.dot11_pkg_len = 0;
	memset(efsm.rx_buf, 0, sizeof(efsm.rx_buf));
	memset(efsm.bitmap, 0, BMAP_MAX_LEN);

	app_data.p = NULL;
	app_data.len = 0;

	msc_ctx.is_stop_switch = 2;
	msc_ctx.find_channel = 0;
	msc_ctx.find_centralCh = 0;

	return 0;
}

static void rx_timeout(unsigned long data)
{
	DBGPRINT(ELIAN_DBG_DEBUG, ("[msc rx timeout]\n"));

	_sc_rst(REASON_RX_TIMEDOUT);
	(*efsm.tbl.report_evt)(EVT_ID_TIMEOUT);
	msc_ctx.elian_status.stat = ELIAN_RX_TIMEDOUT;

	return;
}

static int _elian_reset(int reason)
{
	_sc_rst(reason);
	if(*efsm.tbl.stop_timer && (&efsm.rst_timer))
		(*efsm.tbl.stop_timer)(&efsm.rst_timer);
	return 0;
}

static int elian_rx_done(struct etl_hdr *ehdr, BITMAP_T b)
{
	int idx_max;

	/* check if HL&DL received. */
	if(!bmap_test(b, 4, 1))
		return 0;

	idx_max = ((ehdr->hl + ehdr->dl + 1) & (~1))/2;

	if (idx_max > ETL_IDX_MAX) {
		_elian_reset(REASON_IDX_OOR);
		msc_ctx.elian_status.stat = ELIAN_IDX_OOR;
		return 0;
	} else
		return bmap_test(b, 0, idx_max);
}

static int check_sum(struct etl_hdr *ehdr)
{
	unsigned char value = 0;
	unsigned char *p = (unsigned char *)ehdr;
	int i;

	for (i=0; i < ehdr->hl+ehdr->dl; i++) {
		value += p[i];
	}
	return value?0:1;
}

static void goto_state(enum etl_fsm_state state)
{
	DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] state %d --> %d\n", efsm.state, state));
	efsm.state = state;
	return;
}

static void aes128_decrypt(char *p, int len, char *key)
{
	unsigned char *pCipter, Plain[256] = {0};
	int i, encrypt_data_len;
	unsigned int iCipterLen = AES_BLK_SIZE;

	pCipter = (unsigned char *)p;

	encrypt_data_len = (len+AES_BLK_SIZE-1)&(~(AES_BLK_SIZE-1));

	for (i = 0; i < encrypt_data_len/AES_BLK_SIZE; i++)
	{
		(*efsm.tbl.aes128_decrypt)(pCipter + i * AES_BLK_SIZE,
					AES_BLK_SIZE,
					(unsigned char *)key,
					AES_BLK_SIZE,
					Plain + i * AES_BLK_SIZE,
					&iCipterLen);
	}

	memcpy(pCipter, Plain, encrypt_data_len);

	return;
}

static int eapp_input(char *p, int len)
{
	int i = 0;
	aes128_decrypt(p, len, app_data.key);
	app_data.p = p;
	app_data.len = len;

	DBGPRINT(ELIAN_DBG_ERROR,("\n[MSC] Info Get Begin:"));
	for (i=0; i < len; i++) {
		if(i%32 ==0) DBGPRINT(ELIAN_DBG_ERROR, ("\n[MSC] "));
		DBGPRINT(ELIAN_DBG_ERROR,("%02x ",0xff&p[i]));
	}

	DBGPRINT(ELIAN_DBG_ERROR,("\n[MSC] Info Get End!\n"));

	(*efsm.tbl.report_evt)(EVT_ID_INFOGET);
	return 0;
}

int elian_input(char *p, int len)
{
	char *s_addr;
	char *s_bssid;
	unsigned char idx, c0, c1;
	char *rxb;
	int ret = EDROPPED, lock = 0;
	struct etl_hdr *tl_hdr = (struct etl_hdr *)efsm.rx_buf;
#define IS_MULTICAST_ADDR(Addr)	((((Addr[0]) & 0x01) == 0x01) && ((Addr[0]) != 0xff))

	/*we drop the data if it's NOT multicast data or NOT data frame. */
	if (p == NULL || len < M80211_HEADER_LEN_MIN ||
		(!IS_MULTICAST_ADDR((p+4)) && !IS_MULTICAST_ADDR((p+16))) ||
		(p[0]&0x0C) != 0x08 ) {
		return -EPARAM;
	}

#if 1
	DBGPRINT(ELIAN_DBG_DEBUG,
		("[MSC]len=%d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			len,
			p[0]&0xff, p[1]&0xff, p[2]&0xff, p[3]&0xff, p[4]&0xff, p[5]&0xff, p[6]&0xff, p[7]&0xff,
			p[8]&0xff, p[9]&0xff, p[10]&0xff, p[11]&0xff, p[12]&0xff, p[13]&0xff,p[14]&0xff, p[15]&0xff,
			p[16]&0xff, p[17]&0xff, p[18]&0xff, p[19]&0xff, p[20]&0xff, p[21]&0xff));
#endif
	/*
	 *TO DS:	| FC(2B) | DI(2B) |   BSSID   |    SA     |     DA    | ... |
	 *FROM DS:	| FC(2B) | DI(2B) |    DA     |   BSSID   |     SA    | ... |
	*/
	if ((p[1]&3) == 2) {
		/* from DS. */
		idx = p[7] & 0x7f;
		c0 = p[8];
		c1 = p[9];
		s_addr = p + 16;
		s_bssid = p + 10;
	} else if ((p[1]&3) == 1) {
		/* To DS. */
		idx = p[19] & 0x7f;
		c0 = p[20];
		c1 = p[21];
		s_addr = p + 10;
		s_bssid = p + 4;
	} else {
		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] SC IBSS/WDS data not support. %x\n", p[1]));
		return -EPARAM;
	}

	if( (p[4]==0x01 && p[5]==0x00 && p[6]==0x5e) ||
	(p[16]==0x01 && p[17]==0x00 && p[18]==0x5e)){
		if(efsm.state < ETL_STATE_WAIT_SYNC1) {
			DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] got multi-cast packet\n"));
			msc_ctx.got_mCast_packet = 1;
		}
	}

	if (msc_ctx.is_stop_switch == 2){
		return -EPARAM;
	}

	rxb = efsm.rx_buf;

	lock = _trylock();
	if (!lock)
		return -ELOCK;

	/* check if we are at the correct state, after RX done, we will enter
	   ETL_STATE_DONE and will NOT accept any data. */
	if (efsm.state < ETL_STATE_WAIT_SYNC0 || efsm.state > ETL_STATE_WAIT_DATA) {
		if(lock)
			_unlock();

		return -ESTATE;
	}

	/* check if SRC MAC addr match. */
	if (efsm.state > ETL_STATE_WAIT_SYNC0) {
		if (memcmp(efsm.sa, s_addr, ETH_MAC_LEN)) {
			if (lock)
				_unlock();
			return -EMAC;
		}
	}

	/* To check if the length is OK. */
	if (efsm.state > ETL_STATE_WAIT_SYNC1) {
		if (efsm.dot11_pkg_len != len) {
			if (lock)
				_unlock();
			return -ELENGTH;
		}
	}

	switch (efsm.state) {
	case ETL_STATE_WAIT_SYNC0:
		if (idx==syn[0] && c0==syn[0] && c1==syn[0]) {

			/* Start Reset timer. */
			(*efsm.tbl.start_timer)(&efsm.rst_timer);

			/* record SRC MAC addr and data len. */
			memcpy(efsm.sa, s_addr, ETH_MAC_LEN);

			efsm.dot11_pkg_len = len;
			goto_state(ETL_STATE_WAIT_SYNC1);
			msc_ctx.elian_status.stat = ELIAN_SYNC1;
			ret = ESUCCESS;
		}
		break;
	case ETL_STATE_WAIT_SYNC1:
		if (idx==syn[1] && c0==syn[1] && c1==syn[1]) {
			/* To differ from Elian V0.1/V0.2/V0.3, the length of SYNC[0]
			  of Elian V1.0 is one byte less than other packages. */
			if (len-efsm.dot11_pkg_len == ELIAN_V1_0_FLAG) {
#if 1
				efsm.dot11_pkg_len = len;
				goto_state(ETL_STATE_WAIT_SYNC2);
				msc_ctx.elian_status.stat = ELIAN_SYNC2;
				ret = ESUCCESS;
#endif
#if 0
				ULONG idx;
				PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)(msc_ctx.priv);

				DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] bssid addr.[%2x:%2x:%2x:%2x:%2x:%2x] \n",
					s_bssid[0],s_bssid[1],s_bssid[2],s_bssid[3],s_bssid[4],s_bssid[5]));

				idx = BssTableSearchByBSSID(&pAd->ScanTab,s_bssid);

				DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] receive sync scanning idx=%d\n",(int)idx));

				if (idx != (ULONG) BSS_NOT_FOUND){
					DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] bss.Channel=%d, bss.centralCh=%d,Cur.channel=%d\n",
						pAd->ScanTab.BssEntry[idx].Channel,pAd->ScanTab.BssEntry[idx].CentralChannel,
						pAd->CommonCfg.Channel));

					if( pAd->ScanTab.BssEntry[idx].Channel == pAd->CommonCfg.Channel)
					{
						DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] receive sync stop scanning\n"));
						msc_ctx.is_stop_switch = 1;
						msc_ctx.find_centralCh = pAd->ScanTab.BssEntry[idx].CentralChannel;
						efsm.dot11_pkg_len = len;
						goto_state(ETL_STATE_WAIT_SYNC2);
						msc_ctx.elian_status.stat = ELIAN_SYNC2;
						ret = ESUCCESS;
					} else {
						msc_ctx.find_channel = pAd->ScanTab.BssEntry[idx].Channel;
						msc_ctx.find_centralCh = pAd->ScanTab.BssEntry[idx].CentralChannel;
						break;
					}
				}

				//DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] receive sync stop scanning\n"));
				//msc_ctx.is_stop_switch = 1;
#endif
			}
		}
		break;
	case ETL_STATE_WAIT_SYNC2:
		if (idx==syn[2] && c0==syn[2] && c1==syn[2]) {
			goto_state(ETL_STATE_WAIT_DATA);
			msc_ctx.elian_status.stat = ELIAN_WAIT_DATA;
			ret = ESUCCESS;

			/* got syn0 & sync1 & sync2 pkt, fix channel */
			if(!efsm.sync_succ) {
				efsm.sync_succ = 1;
				(*efsm.tbl.report_evt)(EVT_ID_SYNCSUC);
			}
		}
		break;
	case ETL_STATE_WAIT_DATA:
		if (idx < ETL_IDX_MIN || idx > ETL_IDX_MAX) {
			ret = EIDXOOR;
			break;
		}

		if(msc_ctx.elian_status.stat == ELIAN_WAIT_DATA)
			msc_ctx.elian_status.stat = ELIAN_GETTING_DATA;

		//DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] idx:c0:c1[%02x:%02x:%02x]\n",idx,c0,c1));

		ret = ESUCCESS;
		bmap_set(efsm.bitmap, idx-ETL_IDX_MIN);
		{
			unsigned char *pbmap = efsm.bitmap;
			DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] BITMAP %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				pbmap[0]&0xff, pbmap[1]&0xff, pbmap[2]&0xff, pbmap[3]&0xff,
				pbmap[4]&0xff, pbmap[5]&0xff, pbmap[6]&0xff, pbmap[7]&0xff,
				pbmap[8]&0xff, pbmap[9]&0xff, pbmap[10]&0xff, pbmap[11]&0xff,
				pbmap[12]&0xff, pbmap[13]&0xff));
		}
		rxb[(idx-ETL_IDX_MIN)*2] = c0;
		rxb[(idx-ETL_IDX_MIN)*2+1] = c1;

#if 0
		/* check if MAC addr received done. */
		if (bmap_test(efsm.bitmap,0,3)) {
			/* check if configing me. */
			if (!memcmp(efsm.la, tl_hdr->ta, ETH_MAC_LEN)
				|| !memcmp(tl_hdr->ta, eth_bc_addr, ETH_MAC_LEN)) {
				if(!efsm.sync_succ) {
					efsm.sync_succ = 1;
					(*efsm.tbl.report_evt)(EVT_ID_SYNCSUC);
				}
			} else {
				(*efsm.tbl.report_evt)(EVT_ID_SYNFAIL);
				_elian_reset(REASON_SYNC_FAIL);

				msc_ctx.elian_status.stat = ELIAN_SYNC_FAIL;
				break;
			}
		}
#endif

		/* check if we received all body.*/
		if (elian_rx_done(tl_hdr, efsm.bitmap)) {
			if (check_sum(tl_hdr)) {
				eapp_input(&efsm.rx_buf[tl_hdr->hl], tl_hdr->dl);
				(*efsm.tbl.stop_timer)(&efsm.rst_timer);
				goto_state(ETL_STATE_DONE);
				msc_ctx.elian_status.stat = ELIAN_DONE;
				msc_ctx.cur_chl = msc_get_current_channel_info();
				//bakup sa for continue command
				memcpy(msc_ctx.sa,efsm.sa, ETH_MAC_LEN);
			} else {
				_elian_reset(REASON_CHKSUM_ERR);
				msc_ctx.elian_status.stat = ELIAN_CHKSUM_ERR;
			}

			msc_ctx.find_channel = 0;
			msc_ctx.find_centralCh =0;
		}
		break;
	default:
		break;
	}

	if (lock)
		_unlock();

	return ret;
}

int elian_init(char *la, struct efunc_table *tbl, unsigned char *key)
{
	int ret;

	DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] version: %s\n", ELIAN_VER_CODE));

	if (la == NULL || tbl == NULL ||
		tbl->report_evt == NULL ||
		tbl->start_timer == NULL ||
		tbl->stop_timer == NULL ||
		tbl->aes128_decrypt == NULL)
		return -EPARAM;

	_lock();
	memcpy(efsm.la, la, ETH_MAC_LEN);
	efsm.tbl.report_evt = tbl->report_evt;
	efsm.tbl.start_timer = tbl->start_timer;
	efsm.tbl.stop_timer = tbl->stop_timer;
	efsm.tbl.aes128_decrypt = tbl->aes128_decrypt;

	efsm.rst_timer.data = 0;
	efsm.rst_timer.expires = ETL_TIMEOUT_MAX;
	efsm.rst_timer.func = rx_timeout;

	if (key) {
		memcpy(app_data.key, key, AES_BLK_SIZE);
	} else {
		memcpy(app_data.key, EAPP_DEFAULT_KEY, AES_BLK_SIZE);
	}
	ret = _sc_rst(REASON_INIT);

	_unlock();

	return ret;
}

int elian_get(enum etype_id id, char *buf, int *plen)
{
	int ret = ENOSUCHTYPE;
	int buf_len = *plen;
	int data_len = app_data.len;
	char *p = app_data.p;

	if(buf == NULL || plen == NULL)
		return -EPARAM;

	*plen = 0;
	if(data_len>ETL_TOTAL_LEN_MAX || data_len <= 0) {
		return -ENOTREADY;
	}

	while (data_len > 0) {
		if(p[0] == id) {
			*plen = p[1];
			if (buf_len >= p[1]) {
				memcpy(buf, &p[2], p[1]);
				if (buf_len>p[1]) buf[(int)p[1]] = '\0';
				ret = ESUCCESS;
			} else {
				ret = -ELENGTH;
			}
			break;
		}
		data_len -= p[1]+2;
		p += p[1]+2;
	}

	return ret;
}

int elian_stop(void)
{
	(*efsm.tbl.stop_timer)(&efsm.rst_timer);
	return 0;
}

int elian_reset(void)
{
	msc_ctx.elian_status.stat = ELIAN_USER;
	msc_ctx.find_channel=0;
	msc_ctx.find_centralCh =0;
	msc_ctx.is_stop_switch=0;
	_elian_reset(REASON_USER);
	return 0;
}

int sc_rst(void)
{
	return _sc_rst(REASON_USER);
}


int build_cust_tlv_buf(char **buf, int *len)
{
	char *p;
	int occupied_len = 0;

	p = app_data.p;
	while( (p[0] == TYPE_ID_AM) ||
			(p[0] == TYPE_ID_SSID) ||
			(p[0] == TYPE_ID_PWD)  ||
			(p[0] == TYPE_ID_PMK)
	){
		occupied_len += p[1] + 2;
		p = &p[2] + p[1];
		if(occupied_len >= app_data.len)
			return ELENGTH;
	}

	*buf = p;
	*len = app_data.len - occupied_len;

	return ESUCCESS;
}

int get_ver_code(char *ver, int len)
{
	if(len < sizeof(ELIAN_VER_CODE))
		return ELENGTH;

	memcpy(ver, ELIAN_VER_CODE, sizeof(ELIAN_VER_CODE));
	return ESUCCESS;
}

int get_ver_proto(char *ver, int len)
{
	if(len < sizeof(ELIAN_VER_PROTO))
	return ELENGTH;

	memcpy(ver, ELIAN_VER_PROTO, sizeof(ELIAN_VER_PROTO));
	return ESUCCESS;
}
