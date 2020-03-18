
#include "osal.h"
#include "msc.h"
#include "plat.h"

#include "precomp.h"

/*
static unsigned char msc_listen_chls[] = {1, 6, 11, 3, 8, 13,
                                          149,153,157,161,165,
                                          2,7,12,4,5,9,10,1, 6, 11,
                                          149,153,157,161,165};
*/

static unsigned char msc_listen_chls[38] = {0};

#define MSC_CHANNEL_DWELL_TIME 130 //ms in unit
#define MSC_CHANNEL_DWELL_TIME2 200 //ms in unit

#if 0
struct chan_info msc_listen_chls[] = {
	{1, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{6, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{6, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{11, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{2, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{5, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{5, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{7, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{7, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{1, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{6, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{6, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{11, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{10, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{12, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{3, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{1, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{6, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{6, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{11, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{8, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{13, 	MSC_CHAN_WIDTH_40_MINUS, 0},
	{4, 	MSC_CHAN_WIDTH_40_PLUS, 0},
	{9, 	MSC_CHAN_WIDTH_40_MINUS, 0},
};
#endif

/* On linux OS, timer handler is running in INTR context.
   It can't call any function which may cause sleep. we have
   to handle this case carefully.
*/

struct msc_contex  msc_ctx;

static void msc_core_evt_cbk(enum eevent_id eid);
#if MSC_CHANNEL_SWITCH_USE_THREAD
static int chl_switch_thread(void *data);
#else
static void timeout_handler(unsigned long data);
#endif
static int msc_init(struct msc_param *param, void *priv);

int msc_get_current_channel_info(void)
{
	int cur_chl = 0;

	cur_chl = sc_plt_get_cur_channel(msc_ctx.priv);

	DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] current channel = %d\n",cur_chl));
	return cur_chl;
}


static void msc_core_evt_cbk(enum eevent_id eid)
{
	osal_lock(&msc_ctx.lock);
	switch(eid) {
	case EVT_ID_SYNCSUC:
		if (msc_ctx.chl_num > 1) {
#if MSC_CHANNEL_SWITCH_USE_THREAD
			/*We don't used osal_thread_stop because this function
			  may cause sleep on linux(kthread_stop), and we this runtine
			  may be called from intr context.*/
			msc_ctx.cur_chl = msc_get_current_channel_info();
			//msc_ctx.chl_thread_running = 0;
			msc_ctx.is_stop_switch= 1;
			DBGPRINT(ELIAN_DBG_DEBUG,("[MSC]stop chl_switch thread.\n"));
			//osal_thread_stop(&msc_ctx.chl_thread);
#else
			osal_timer_stop(&msc_ctx.timer);
#endif
		}
		break;
	case EVT_ID_INFOGET:
		sccb_disable_input();
		if (msc_ctx.evt_cb)
			msc_ctx.evt_cb(eid);
		break;
	case EVT_ID_TIMEOUT:
		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC]receive pkt timeout.\n"));
#if 0
		msc_stop(msc_ctx.priv);
		msc_msleep(1000);

		para.chls = NULL;
		para.chl_num = 0;
		msc_start(&para, msc_ctx.priv);
#endif
		break;
	default:
		break;
	}
	osal_unlock(&msc_ctx.lock);
	return;
}

#if MSC_CHANNEL_SWITCH_USE_THREAD
static int chl_switch_thread(void *data)
{
	static unsigned char idx = 0;
	static int old_ch = 0;
	static int scan_counter = 0;
	struct chan_info chl_info;

	//P_ADAPTER_T pAd = (P_ADAPTER_T)(msc_ctx.priv);

	idx = 0;
	old_ch = 0;
	scan_counter = 0;
	chl_info.chan_id = 0;
	chl_info.chan_width = 0;
	chl_info.flags = 0x0;
	msc_ctx.got_mCast_packet = 1;

	//while(!osal_thread_should_stop(&msc_ctx.chl_thread)) {
	while(1) {//msc_ctx.chl_thread_running) {
		osal_sleep_ms(MSC_CHANNEL_DWELL_TIME);
		osal_lock(&msc_ctx.lock);
		if(!msc_ctx.chl_thread_running)
		{
			osal_unlock(&msc_ctx.lock);
			break;
		}

		if(msc_ctx.got_mCast_packet)
		{
			DBGPRINT(ELIAN_DBG_DEBUG,("[MSC]got Mcast, continue sleeping \n"));
			osal_unlock(&msc_ctx.lock);
			osal_sleep_ms(MSC_CHANNEL_DWELL_TIME2);
			osal_lock(&msc_ctx.lock);
			msc_ctx.got_mCast_packet = 0;
			if(!msc_ctx.chl_thread_running)
			{
				osal_unlock(&msc_ctx.lock);
				break;
			}
		}

		msc_ctx.is_thread_run_to_end = 0;
		msc_ctx.got_mCast_packet = 0;

		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] scan_counter=%d, idx=%d,is_stop_switch = %d\n",
			scan_counter,idx,msc_ctx.is_stop_switch));
		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] channel=%d, old_channel=%d,chan_width = %d, idx=%d\n",
			chl_info.chan_id, old_ch, chl_info.chan_width, idx));

		//set channel
		if(msc_ctx.find_channel)
			chl_info.chan_id = msc_ctx.find_channel;
		else if(msc_ctx.is_stop_switch == 1) {
			if(old_ch)
				chl_info.chan_id = old_ch;
			else
				chl_info.chan_id = msc_ctx.channel[0];
		}else
			chl_info.chan_id = msc_ctx.channel[idx];

		//set bandwith
		if(chl_info.chan_id > 14)
			chl_info.chan_width = 40;
		else
			chl_info.chan_width = 20;

		if( (msc_ctx.is_stop_switch==2) && (old_ch != chl_info.chan_id))
			msc_ctx.is_stop_switch = 0;

		chl_info.flags = 0x0;

		if(old_ch == chl_info.chan_id)
		{
			DBGPRINT(ELIAN_DBG_DEBUG,("sw continue\n"));
		} else {
			if(msc_ctx.is_stop_switch == 1){
				chl_info.chan_id = old_ch;
			}else{
				old_ch = chl_info.chan_id;
				sccb_set_monitor_chan(&chl_info, msc_ctx.priv);
			}
		}

		if(((!msc_ctx.find_channel)&&((msc_ctx.is_stop_switch==0))) ||(msc_ctx.is_stop_switch==2) ){
			idx++;
			if(idx >= msc_ctx.chl_num)
				idx = 0;

			// TODO why?
			if(msc_ctx.channel[idx] > 14){
				if(scan_counter%2){
					idx++;
					if(idx >= msc_ctx.chl_num)
					idx = 0;
				}
			}

			if(msc_ctx.channel[idx] < old_ch)
			{
				scan_counter ++;
			}
		}

		msc_ctx.is_thread_run_to_end = 1;
		osal_unlock(&msc_ctx.lock);
	}

	//WHY??
	if ((msc_ctx.cur_chl != chl_info.chan_id) && (chl_info.chan_id != 0) &&  (msc_ctx.cur_chl != 0))
	{
		osal_sleep_ms(100);
		chl_info.chan_id = msc_ctx.cur_chl;
		if (chl_info.chan_id >= 8  && chl_info.chan_id <= 14) {
			chl_info.width = MSC_CHAN_WIDTH_40_MINUS;
		} else {
			chl_info.width = MSC_CHAN_WIDTH_40_PLUS;
		}
		chl_info.flags = 0x0;
		if(msc_ctx.state != MSC_STATE_STOPED)
			sccb_set_monitor_chan(&chl_info, msc_ctx.priv);

		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] chl_info.chan_id = %d\n",chl_info.chan_id));
	}

	return 0;
}
#else
static void timeout_handler(unsigned long data)
{
	static int idx = 0;
	struct chan_info chl_info;

	osal_lock(&msc_ctx.lock);
	chl_info.chan_id = msc_ctx.channel[(idx++)%msc_ctx.chl_id];
	if (chl_info.chan_id >= 8  && chl_info.chan_id <= 14) {
		chl_info.width = MSC_CHAN_WIDTH_40_MINUS;
	} else {
		chl_info.width = MSC_CHAN_WIDTH_40_PLUS;
	}
	chl_info.flags = 0x0;
	sccb_set_monitor_chan(&chl_info, msc_ctx.priv);

	osal_timer_modify(&msc_ctx.timer, 1000);
	osal_unlock(&msc_ctx.lock);
	return;
}
#endif

struct efunc_table f_tbl = {
	.report_evt = msc_core_evt_cbk,
	.start_timer = sc_plt_add_timer,
	.stop_timer = sc_plt_del_timer,
	.aes128_decrypt = sc_plt_aes128_decrypt,
};

static int msc_init(struct msc_param *param, void *priv)
{
	int ret = 0;
	unsigned char i=0;

	//PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)(priv);
	P_ADAPTER_T pAd = (P_ADAPTER_T)(priv);

	/* max number of channel 38, refer to msc_listen_chls[38] */
	UINT_8 ucNumOfChannel;
	RF_CHANNEL_INFO_T aucChannelList[38];

	DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] Driver v 1.0.0\n"));

	osal_memset(&msc_ctx, 0, sizeof(msc_ctx));

	osal_lock_init(&msc_ctx.lock);
	msc_ctx.evt_cb = param->evt_cb;

	rlmDomainGetChnlList(pAd, BAND_NULL, FALSE, 38, &ucNumOfChannel, aucChannelList);

	for ( i = 0; i < ucNumOfChannel; i++) {
		msc_listen_chls[i] = aucChannelList[i].ucChannelNum;
		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] listen channel[%d]=%d \n",i,msc_listen_chls[i]));
	}

	if (param->chl_num == 0) {
		msc_ctx.channel = msc_listen_chls;
		msc_ctx.chl_num = ucNumOfChannel;//sizeof(msc_listen_chls);
		msc_ctx.elian_status.fixChannel = 0;
	//msc_ctx.elian_status.cur_channel = 0;
	} else {
		msc_ctx.channel = param->chls;
		msc_ctx.chl_num = param->chl_num;

		//msc_ctx.elian_status.fixChannel = 1;
		msc_ctx.elian_status.fixChannel = msc_ctx.channel[0];

		//DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] channel = %d\n",msc_ctx.elian_status.channel));
		//DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] fixChannel = %d\n",msc_ctx.elian_status.fixChannel));

		// DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] by ioctrl listen channel=%d \n",msc_ctx.channel[0]));
	}

	ret = elian_init(sc_plt_get_la(priv), &f_tbl, param->key);
	if (ret) {
		DBGPRINT(ELIAN_DBG_ERROR,("[MSC] Get sc fsm init failed.\n"));
		//Add error handler.
	}

	//init elian status
	msc_ctx.elian_status.stat = ELIAN_INIT;
	//msc_ctx.elian_status.fixChannel=0;
	//msc_ctx.elian_status.channel=0;
	msc_ctx.find_channel = 0;
	msc_ctx.find_centralCh = 0;
	msc_ctx.is_stop_switch = 0;

#if MSC_CHANNEL_SWITCH_USE_THREAD
	msc_ctx.chl_thread.pThreadFunc = chl_switch_thread;
	msc_ctx.chl_thread.pThreadData = NULL;
	strcpy(msc_ctx.chl_thread.threadName, "chl_switch");
	osal_thread_create(&msc_ctx.chl_thread);
#else
	msc_ctx.timer.timeoutHandler = timeout_handler;
	osal_timer_create(&msc_ctx.timer);
#endif
	return ret;
}

int msc_start(struct msc_param *param, void *priv)
{
	int ret = 0;

	if(msc_ctx.state == MSC_STATE_STARTED)
		return 0;

	msc_init(param, priv);

	sccb_init(elian_input, NULL);

	msc_ctx.m_info.filter = 0x00000000;
	msc_ctx.m_info.priv = NULL;
	msc_ctx.m_info.chl_info.chan_id = msc_ctx.channel[0];
	if(msc_ctx.m_info.chl_info.chan_id > 14)
		msc_ctx.m_info.chl_info.chan_width = 40;
	else
		msc_ctx.m_info.chl_info.chan_width = 20;
#if 0
	if (msc_ctx.m_info.chl_info.chan_id >= 8  && msc_ctx.m_info.chl_info.chan_id <= 14) {
		msc_ctx.m_info.chl_info.width = MSC_CHAN_WIDTH_40_MINUS;
	} else {
		msc_ctx.m_info.chl_info.width = MSC_CHAN_WIDTH_40_PLUS;
	}
#endif
	msc_ctx.m_info.chl_info.flags = 0x0;
	msc_ctx.chl_thread_running = 1;
	msc_ctx.is_stop_switch = 0;
	msc_ctx.is_thread_run_to_end = 1;
	msc_ctx.cur_chl = 0;
	msc_ctx.priv = priv;

	osal_memset(&msc_ctx.sa, 0, ETH_ALEN);
	msc_ctx.chl_rev = 0;

	/* Config to monitor mode. */
	sccb_enter_monitor_mode(&msc_ctx.m_info, priv);

	sccb_enable_input();

	if (msc_ctx.chl_num > 1) {
#if MSC_CHANNEL_SWITCH_USE_THREAD
		osal_thread_run(&msc_ctx.chl_thread);
#else
		osal_timer_start(&msc_ctx.timer, 1000);
#endif
	}

	msc_ctx.state = MSC_STATE_STARTED;
	return ret;
}

void msc_stop(void *priv)
{
	unsigned int count=0;
	bool timeout = 0;

	//PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)(priv);

	if (msc_ctx.state == MSC_STATE_STOPED)
		return;

	msc_ctx.find_channel=0;
	msc_ctx.find_centralCh =0;

	osal_lock(&msc_ctx.lock);
	if (msc_ctx.chl_num > 1) {
#if MSC_CHANNEL_SWITCH_USE_THREAD
		/* Avoid INTR context. */
		//osal_thread_stop(&msc_ctx.chl_thread);
		msc_ctx.chl_thread_running = 0;
		msc_ctx.is_stop_switch = 0;
		osal_memset(&msc_ctx.sa, 0, ETH_ALEN);
		msc_ctx.chl_rev=0;
#else
		osal_timer_stop(&msc_ctx.timer);
#endif
	}
	elian_stop();
	sccb_disable_input();

	/*there is a problem if chl_switch_thread haven't finish "sccb_set_monitor_chan()" function.
	so we use a flag "msc_ctx.is_thread_run_to_end" to avoid that case*/
	while(msc_ctx.is_thread_run_to_end == 0)
	{
		DBGPRINT(ELIAN_DBG_DEBUG,("chl_switch thread not finished, waiting 50ms!\n"));
		osal_sleep_ms(50);
		if(count++ >= 40)
		{
			timeout=1;
			break;
		}
	}

	if(timeout)
	{
		DBGPRINT(ELIAN_DBG_DEBUG,("msc_stop failed (timeout)!\n"));
		return;
	}

	sccb_leave_monitor_mode(priv);

	msc_ctx.state = MSC_STATE_STOPED;
	msc_ctx.elian_status.stat = ELIAN_STOPED;

	osal_unlock(&msc_ctx.lock);
	return;
}

int msc_get_result(struct msc_result *result)
{
	char buffer[MSC_RESULT_BUFFER_SIZE] = {0};
	int len;

	len = MSC_RESULT_BUFFER_SIZE;
	elian_get(TYPE_ID_AM, buffer, &len);
	/* TODO:shoule add error handling. */
	memcpy((char *)&(result->auth_mode), buffer, len);

	len = MSC_RESULT_BUFFER_SIZE;
	memset(buffer, 0, sizeof(buffer));
	elian_get(TYPE_ID_SSID, buffer, &len);
	memcpy(result->ssid, buffer, len);

	len = MSC_RESULT_BUFFER_SIZE;
	memset(buffer, 0, sizeof(buffer));
	elian_get(TYPE_ID_PWD, buffer, &len);
	memcpy(result->pwd, buffer, len);

	len = MSC_RESULT_BUFFER_SIZE;
	memset(buffer, 0, sizeof(buffer));
	elian_get(TYPE_ID_USER, buffer, &len);
	memcpy(result->user, buffer, len);

	len = MSC_RESULT_BUFFER_SIZE;
	memset(buffer, 0, sizeof(buffer));
	elian_get(TYPE_ID_CUST, buffer, &len);
	memcpy(result->cust_data, buffer, len);
	result->cust_data_len = len;

	return 0;
}

/*int msc_get_status(struct _ELIAN_STATUS *status)
{
	status->stat = msc_ctx.elian_status.stat;
	status->fixChannel = msc_ctx.elian_status.fixChannel;
	status->channel = msc_ctx.elian_status.channel;

	DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] msc_get_result 6 \n"));

	return 0;
}*/

int msc_reset()
{
	sccb_disable_input();
	elian_reset();
	sccb_enable_input();

	return 0;
}

int msc_set_chl(struct chan_info *chl, void *priv)
{
	if(msc_ctx.state == MSC_STATE_STOPED)
		return -1;

	sccb_set_monitor_chan(chl, priv);
	return 0;
}


int msc_cmd_handler(char *cmd, int len, char *result_str, void *priv)
{
	char *p = cmd;
	struct msc_param para = {0};
	struct msc_result result ={0};
	//struct chan_info chl_info;
	static int chl;

	char result_str_temp[500];

	if (cmd == NULL || result_str == NULL || priv == NULL)
		return -1;

	DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] msc_cmd_handler %s %d\n", cmd, len));

	if(osal_strncmp(p, "start", 5) == 0) {
		if(len>6) { //+1 for "enter"
			p = osal_strstr(p, "ch=");
			if(p == NULL){
				osal_strcpy(result_str, "Format: elian start ch=6");
				return -2;
			}
			chl = (int)osal_strtol(p+3, 10, NULL);
			para.chls = (char *)&chl;
			DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] msc_cmd_handler %s %d\n", p, chl));
			para.chl_num = 1;
		} else {

			DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] chl=%d\n",chl));

			if(chl){
				para.chls = (char *)&chl;
				para.chl_num = 1;
				DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] msc_cmd_handler %s %d\n", p, chl));
			} else {
				para.chls = NULL;
				para.chl_num = 0;
			}
		}
		msc_start(&para, priv);
		osal_strcpy(result_str, "ok");
	} else if(osal_strcmp(p, "stop") == 0) {
		msc_stop(priv);

		if(chl)
		{
			//msc_ctx.elian_status.channel = 0;
			msc_ctx.elian_status.fixChannel = 0;
			chl =0;
		}
		osal_strcpy(result_str, "ok");
	} else if (osal_strcmp(p, "clear") == 0) {
		msc_reset();
		osal_strcpy(result_str, "ok");
	} else if (osal_strcmp(p, "result") == 0){

		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] msc_get_result -->\n"));
		msc_get_result(&result);
		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] AM=%d, ssid=%s, pwd=%s, user=%s, cust_data_len=%d, cust_data=%s,\n",
			result.auth_mode, result.ssid, result.pwd,
			result.user, result.cust_data_len, result.cust_data));

		{
		int i;
		for(i=0;i<result.cust_data_len;i++)
		{
			DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] cust_data[%d]=%2x,\n",i,result.cust_data[i]));
		}
		}

		sprintf(result_str_temp, "AM=%d, ssid=%s, pwd=%s, user=%s, cust_data_len=%d, cust_data=%s,\n",
				result.auth_mode, result.ssid, result.pwd,
				result.user, result.cust_data_len, result.cust_data);
		if(osal_strlen(result_str_temp) >= MSC_RESULT_BUFFER_SIZE)
		{
			printk("Max result len is 128, Current len is [%d] \n", osal_strlen(result_str_temp));
		}
		else
		{
			osal_memcpy(result_str, result_str_temp, osal_strlen(result_str_temp));
		}
	} else if (osal_strncmp(p, "set_fixCh=", 7) == 0) {
		if(msc_ctx.state == MSC_STATE_STARTED)
			osal_strcpy(result_str, "Fail");
		else {
			chl = (int)osal_strtol(p+7, 10, NULL);
			msc_ctx.elian_status.fixChannel = (unsigned char)chl;

			DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] Set_ch = %d\n",chl));

			osal_strcpy(result_str, "ok");
		}
	}else if (osal_strcmp(p, "status") == 0){
		bool scanning=0;

		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] get elian status -->\n"));

		if(msc_ctx.state == MSC_STATE_STARTED)
			msc_ctx.elian_status.cur_channel = msc_get_current_channel_info();
		else
			msc_ctx.elian_status.cur_channel = (msc_ctx.elian_status.fixChannel!=0)?msc_ctx.elian_status.fixChannel:0;

		if ((msc_ctx.chl_thread_running == 0) ||
			((msc_ctx.chl_thread_running == 1) &&(msc_ctx.is_stop_switch == 1))
			|| msc_ctx.elian_status.fixChannel )
			scanning=0;
		else
			scanning=1;

		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] stat=%d, fixChannel=%d, cur_channel=%d, scanning=%d\n",
			msc_ctx.elian_status.stat, msc_ctx.elian_status.fixChannel,
			msc_ctx.elian_status.cur_channel,scanning ));

		sprintf(result_str_temp, "stat=%d, fixChannel=%d, cur_channel=%d,scanning=%d\n",
				msc_ctx.elian_status.stat, msc_ctx.elian_status.fixChannel,
				msc_ctx.elian_status.cur_channel,scanning);

		if(osal_strlen(result_str_temp) >= MSC_RESULT_BUFFER_SIZE)
		{
			printk("Max result len is 128, Current len is [%d] \n", osal_strlen(result_str_temp));
		}
		else
		{
			osal_memcpy(result_str, result_str_temp, osal_strlen(result_str_temp));
		}
	}else if (osal_strcmp(p, "continue") == 0){
		DBGPRINT(ELIAN_DBG_DEBUG,("[MSC] continue receiving data -->\n"));
		msc_ctx.is_stop_switch = 2;
		msc_reset();
		osal_strcpy(result_str, "ok");
	}else if(osal_strcmp(p, "debug=3") == 0){
		ElianDebugLevel = ELIAN_DBG_DEBUG;
		osal_strcpy(result_str, "ok");
	}else if(osal_strcmp(p, "debug=2") == 0){
		ElianDebugLevel = ELIAN_DBG_WARN;
		osal_strcpy(result_str, "ok");
	}else if(osal_strcmp(p, "debug=1") == 0){
		ElianDebugLevel = ELIAN_DBG_ERROR;
		osal_strcpy(result_str, "ok");
	}

#if 0
  {

		chl = (int)osal_strtol(p+7, 10, NULL);
		chl_info.chan_id = chl;

	 	if(osal_strstr(p, "bw=20")) {
			chl_info.width = MSC_CHAN_WIDTH_20;
		} else if (osal_strstr(p, "bw=40h")) {
			chl_info.width = MSC_CHAN_WIDTH_40_PLUS;
		} else if (osal_strstr(p, "bw=40l")) {
			chl_info.width = MSC_CHAN_WIDTH_40_MINUS;
		} else {
			if (chl_info.chan_id >= 8  && chl_info.chan_id <= 14) {
				chl_info.width = MSC_CHAN_WIDTH_40_MINUS;
			} else {
				chl_info.width = MSC_CHAN_WIDTH_40_PLUS;
			}
		}
		chl_info.flags = 0x0;
		if (msc_set_chl(&chl_info, priv)) {
			osal_strcpy(result_str, "call start CMD first.");
		} else {
			osal_strcpy(result_str, "ok");
		}
	}
#endif
	else {
		osal_strcpy(result_str, "unknown CMD.");
	}
	return 0;
}

