/*
* Copyright (C) 2011-2015 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/vmalloc.h>         /* needed by vmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <mt-plat/aee.h>
#include <mt-plat/sync_write.h>
#include <linux/sched_clock.h>
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_feature_define.h"


struct scp_aed_cfg {
	int *log;
	int log_size;
	int *phy;
	int phy_size;
	char *detail;
	MemoryDump *pMemoryDump;
	int memory_dump_size;
};

struct scp_status_reg {
	unsigned int pc;
	unsigned int lr;
	unsigned int psp;
	unsigned int sp;
	unsigned int m2h;
	unsigned int h2m;
};
static unsigned char *scp_A_detail_buffer;
static unsigned char *scp_A_dump_buffer;
static unsigned char *scp_A_dump_buffer_last;
static unsigned int scp_A_dump_length;
static unsigned char *scp_B_detail_buffer;
static unsigned char *scp_B_dump_buffer;
static unsigned char *scp_B_dump_buffer_last;
static unsigned int scp_B_dump_length;
static unsigned int scp_A_task_context_addr;
static unsigned int scp_B_task_context_addr;
static struct scp_work_struct scp_aed_work;
static struct scp_status_reg scp_A_aee_status;
static struct scp_status_reg scp_B_aee_status;
static struct mutex scp_excep_mutex;
static struct mutex scp_A_excep_dump_mutex, scp_B_excep_dump_mutex;
/* An ELF note in memory */
struct memelfnote {
	const char *name;
	int type;
	unsigned int datasz;
	void *data;
};

static int notesize(struct memelfnote *en)
{
	int sz;

	sz = sizeof(struct elf32_note);
	sz += roundup((strlen(en->name) + 1), 4);
	sz += roundup(en->datasz, 4);

	return sz;
}

static uint8_t *storenote(struct memelfnote *men, uint8_t *bufp)
{
	struct elf32_note en;

	en.n_namesz = strlen(men->name) + 1;
	en.n_descsz = men->datasz;
	en.n_type = men->type;

	memcpy(bufp, &en, sizeof(en));
	bufp += sizeof(en);

	memcpy(bufp, men->name, en.n_namesz);
	bufp += en.n_namesz;

	bufp = (uint8_t *) roundup((unsigned long)bufp, 4);
	memcpy(bufp, men->data, men->datasz);
	bufp += men->datasz;

	bufp = (uint8_t *) roundup((unsigned long)bufp, 4);
	return bufp;
}

static uint8_t *core_write_cpu_note(int cpu, struct elf32_phdr *nhdr, uint8_t *bufp, scp_core_id id)
{
	struct memelfnote notes;
	struct elf32_prstatus prstatus;
	char cpustr[16];

	memset(&prstatus, 0, sizeof(struct elf32_prstatus));
	snprintf(cpustr, sizeof(cpustr), "CPU%d", cpu);
	/* set up the process status */
	notes.name = cpustr;
	notes.type = NT_PRSTATUS;
	notes.datasz = sizeof(struct elf32_prstatus);
	notes.data = &prstatus;

	prstatus.pr_pid = cpu + 1;
	if (scp_A_task_context_addr && (id == SCP_A_ID)) {
		memcpy_from_scp((void *)&(prstatus.pr_reg),
				(void *)(SCP_TCM + scp_A_task_context_addr), sizeof(prstatus.pr_reg));
	}

	if (scp_B_task_context_addr && (id == SCP_B_ID)) {
		memcpy_from_scp((void *)&(prstatus.pr_reg),
				(void *)(SCP_B_TCM + scp_B_task_context_addr), sizeof(prstatus.pr_reg));
	}

	if (prstatus.pr_reg[15] == 0x0 && (id == SCP_A_ID))
		prstatus.pr_reg[15] = SCP_A_DEBUG_PC_REG;
	if (prstatus.pr_reg[14] == 0x0 && (id == SCP_A_ID))
		prstatus.pr_reg[14] = SCP_A_DEBUG_LR_REG;
	if (prstatus.pr_reg[13] == 0x0 && (id == SCP_A_ID))
		prstatus.pr_reg[13] = SCP_A_DEBUG_PSP_REG;

	if (prstatus.pr_reg[15] == 0x0 && (id == SCP_B_ID))
		prstatus.pr_reg[15] = SCP_B_DEBUG_PC_REG;
	if (prstatus.pr_reg[14] == 0x0 && (id == SCP_B_ID))
		prstatus.pr_reg[14] = SCP_B_DEBUG_LR_REG;
	if (prstatus.pr_reg[13] == 0x0 && (id == SCP_B_ID))
		prstatus.pr_reg[13] = SCP_B_DEBUG_PSP_REG;

	nhdr->p_filesz += notesize(&notes);
	return storenote(&notes, bufp);
}

void exception_header_init(void *oldbufp, scp_core_id id)
{
	struct elf32_phdr *nhdr, *phdr;
	struct elf32_hdr *elf;
	off_t offset = 0;

	uint8_t *bufp = oldbufp;
	uint32_t cpu;

	/* NT_PRPSINFO */
	struct elf32_prpsinfo prpsinfo;
	struct memelfnote notes;

	elf = (struct elf32_hdr *) bufp;
	bufp += sizeof(struct elf32_hdr);
	offset += sizeof(struct elf32_hdr);
	elf_setup_eident(elf->e_ident, ELFCLASS32);

	/*setup elf header*/
	elf->e_type = ET_CORE;
	elf->e_machine = EM_ARM;
	elf->e_version = EV_CURRENT;
	elf->e_entry = 0;
	elf->e_phoff = sizeof(struct elf32_hdr);
	elf->e_shoff = 0;
	elf->e_flags = ELF_CORE_EFLAGS;
	elf->e_ehsize = sizeof(struct elf32_hdr);
	elf->e_phentsize = sizeof(struct elf32_phdr);
	elf->e_phnum = 2;
	elf->e_shentsize = 0;
	elf->e_shnum = 0;
	elf->e_shstrndx = 0;

	nhdr = (struct elf32_phdr *) bufp;
	bufp += sizeof(struct elf32_phdr);
	offset += sizeof(struct elf32_phdr);
	memset(nhdr, 0, sizeof(struct elf32_phdr));
	nhdr->p_type = PT_NOTE;

	phdr = (struct elf32_phdr *) bufp;
	bufp += sizeof(struct elf32_phdr);
	offset += sizeof(struct elf32_phdr);
	phdr->p_flags = PF_R|PF_W|PF_X;
	phdr->p_offset = CRASH_MEMORY_HEADER_SIZE;
	phdr->p_vaddr = CRASH_MEMORY_OFFSET;
	phdr->p_paddr = CRASH_MEMORY_OFFSET;

	if (id == SCP_A_ID) {
		phdr->p_filesz = (SCP_A_TCM_SIZE - CRASH_MEMORY_OFFSET);
		phdr->p_memsz = (SCP_A_TCM_SIZE - CRASH_MEMORY_OFFSET);
	} else {
		phdr->p_filesz = (SCP_B_TCM_SIZE - CRASH_MEMORY_OFFSET);
		phdr->p_memsz = (SCP_B_TCM_SIZE - CRASH_MEMORY_OFFSET);
	}

	phdr->p_align = 0;
	phdr->p_type = PT_LOAD;

	nhdr->p_offset = offset;

	/* set up the process info */
	notes.name = CORE_STR;
	notes.type = NT_PRPSINFO;
	notes.datasz = sizeof(struct elf32_prpsinfo);
	notes.data = &prpsinfo;

	memset(&prpsinfo, 0, sizeof(struct elf32_prpsinfo));
	prpsinfo.pr_state = 0;
	prpsinfo.pr_sname = 'R';
	prpsinfo.pr_zomb = 0;
	prpsinfo.pr_gid = prpsinfo.pr_uid = 0x0;
	strlcpy(prpsinfo.pr_fname, "freertos8", sizeof(prpsinfo.pr_fname));
	strlcpy(prpsinfo.pr_psargs, "freertos8", ELF_PRARGSZ);

	nhdr->p_filesz += notesize(&notes);
	bufp = storenote(&notes, bufp);

	/* Store pre-cpu backtrace */
	for (cpu = 0; cpu < 1; cpu++)
		bufp = core_write_cpu_note(cpu, nhdr, bufp, id);
}
/*
static void scp_dump_buffer_set(unsigned char *buf, unsigned int length)
{
	mutex_lock(&scp_A_excep_dump_mutex);
	scp_A_dump_buffer = buf;
	scp_A_dump_length = length;
	mutex_unlock(&scp_A_excep_dump_mutex);
}
*/
/*
 * return last lr for debugging
 */
uint32_t scp_dump_lr(void)
{
	if (is_scp_ready(SCP_A_ID))
		return SCP_A_DEBUG_LR_REG;
	else
		return 0xFFFFFFFF;
}
/*
 * return last pc for debugging
 */
uint32_t scp_dump_pc(void)
{
	if (is_scp_ready(SCP_A_ID))
		return SCP_A_DEBUG_PC_REG;
	else
		return 0xFFFFFFFF;
}
/*
 * dump scp register for debugging
 */
void scp_A_dump_regs(void)
{
	if (is_scp_ready(SCP_A_ID)) {
		pr_err("[SCP} SCP A is alive\n");
		pr_err("[SCP] SCP_A_DEBUG_PC_REG:0x%x\n", SCP_A_DEBUG_PC_REG);
		pr_err("[SCP] SCP_A_DEBUG_LR_REG:0x%x\n", SCP_A_DEBUG_LR_REG);
		pr_err("[SCP] SCP_A_DEBUG_PSP_REG:0x%x\n", SCP_A_DEBUG_PSP_REG);
		pr_err("[SCP] SCP_A_DEBUG_SP_REG:0x%x\n", SCP_A_DEBUG_SP_REG);
	} else {
		pr_err("[SCP} SCP A is dead\n");
	}
}

/*
 * dump scp  B register for debugging
 */
void scp_B_dump_regs(void)
{
	if (is_scp_ready(SCP_B_ID)) {
		pr_err("[SCP} SCP B is alive\n");
		pr_err("[SCP] SCP_B_DEBUG_PC_REG:0x%x\n", SCP_B_DEBUG_PC_REG);
		pr_err("[SCP] SCP_B_DEBUG_LR_REG:0x%x\n", SCP_B_DEBUG_LR_REG);
		pr_err("[SCP] SCP_B_DEBUG_PSP_REG:0x%x\n", SCP_B_DEBUG_PSP_REG);
		pr_err("[SCP] SCP_B_DEBUG_SP_REG:0x%x\n", SCP_B_DEBUG_SP_REG);
	} else {
		pr_err("[SCP} SCP B is dead\n");
	}
}

/*
 * save scp register when scp crash
 * these data will be used to generate EE
 */
void scp_aee_last_reg(void)
{
	pr_debug("scp_aee_last_reg\n");

	scp_A_aee_status.pc = SCP_A_DEBUG_PC_REG;
	scp_A_aee_status.lr = SCP_A_DEBUG_LR_REG;
	scp_A_aee_status.psp = SCP_A_DEBUG_PSP_REG;
	scp_A_aee_status.sp = SCP_A_DEBUG_SP_REG;
	scp_A_aee_status.m2h = SCP_A_TO_HOST_REG;
	scp_A_aee_status.h2m = GIPC_TO_SCP_REG;

	scp_B_aee_status.pc = SCP_B_DEBUG_PC_REG;
	scp_B_aee_status.lr = SCP_B_DEBUG_LR_REG;
	scp_B_aee_status.psp = SCP_B_DEBUG_PSP_REG;
	scp_B_aee_status.sp = SCP_B_DEBUG_SP_REG;
	scp_B_aee_status.m2h = SCP_B_TO_HOST_REG;
	scp_B_aee_status.h2m = GIPC_TO_SCP_REG;

	pr_debug("scp_aee_last_reg end\n");
}

/*
 * this function need SCP to keeping awaken
 * scp_crash_dump: dump scp tcm info.
 * @param MemoryDump:   scp dump struct
 * @param scp_core_id:  core id
 * @return:             scp dump size
 */
static unsigned int scp_crash_dump(MemoryDump *pMemoryDump, scp_core_id id)
{
	unsigned int lock;
	unsigned int *reg;
	unsigned int scp_dump_size;

	/*check SRAM lock ,awake scp*/
	if (scp_awake_lock(id) == -1)
		pr_err("scp_crash_dump: awake scp fail, scp id=%u\n", id);

	reg = (unsigned int *) (scpreg.cfg + SCP_LOCK_OFS);
	lock = *reg;
	*reg &= ~SCP_TCM_LOCK_BIT;
	dsb(SY);
	if ((*reg & SCP_TCM_LOCK_BIT)) {
		pr_debug("[SCP]unlock failed, skip dump\n");
		return 0;
	}

	exception_header_init(pMemoryDump, id);
	if (id == SCP_A_ID) {
		memcpy_from_scp((void *)&(pMemoryDump->memory),
			(void *)(SCP_TCM + CRASH_MEMORY_OFFSET), (SCP_A_TCM_SIZE - CRASH_MEMORY_OFFSET));
		scp_dump_size = CRASH_MEMORY_HEADER_SIZE + SCP_A_TCM_SIZE;
	} else {
		memcpy_from_scp((void *)&(pMemoryDump->memory),
			(void *)(SCP_B_TCM + CRASH_MEMORY_OFFSET), (SCP_B_TCM_SIZE - CRASH_MEMORY_OFFSET));
		scp_dump_size = CRASH_MEMORY_HEADER_SIZE + SCP_B_TCM_SIZE;
	}
	*reg = lock;
	dsb(SY);
	/*check SRAM unlock*/
	scp_awake_unlock(id);
	return scp_dump_size;
}
/*
 * generate aee argument without dump scp register
 * @param aed_str:  exception description
 * @param aed:      struct to store argument for aee api
 */
static void scp_prepare_aed(char *aed_str, struct scp_aed_cfg *aed)
{
	char *detail, *log;
	u8 *phy, *ptr;
	u32 log_size, phy_size;

	pr_debug("scp_prepare_aed\n");

	detail = vmalloc(SCP_AED_STR_LEN);
	ptr = detail;
	memset(detail, 0, SCP_AED_STR_LEN);
	snprintf(detail, SCP_AED_STR_LEN, "%s\n", aed_str);
	detail[SCP_AED_STR_LEN - 1] = '\0';

	log_size = 0;
	log = NULL;

	phy_size = 0;
	phy = NULL;

	aed->log = (int *)log;
	aed->log_size = log_size;
	aed->phy = (int *)phy;
	aed->phy_size = phy_size;
	aed->detail = detail;

	pr_debug("scp_prepare_aed end\n");
}

/*
 * generate aee argument with scp register dump
 * @param aed_str:  exception description
 * @param aed:      struct to store argument for aee api
 * @param id:       identify scp core id
 */
static void scp_prepare_aed_dump(char *aed_str, struct scp_aed_cfg *aed, scp_core_id id)
{
	u8 *scp_detail;
	u8 *scp_dump_ptr;

	u32 memory_dump_size;
	MemoryDump *pMemoryDump = NULL;

	char *scp_A_log = NULL;
	char *scp_B_log = NULL;

	pr_debug("scp_prepare_aed_dump: %s\n", aed_str);

	scp_aee_last_reg();

	scp_A_log = scp_get_last_log(SCP_A_ID);
	scp_B_log = scp_get_last_log(SCP_B_ID);


	/* prepare scp aee detail information */
	if (id == SCP_A_ID)
		scp_detail = scp_A_detail_buffer;
	else
		scp_detail = scp_B_detail_buffer;

	if (scp_detail == NULL) {
		pr_err("[SCP AEE]detail buf is null\n");
	} else {
		/* prepare scp aee detail information*/
		memset(scp_detail, 0, SCP_AED_STR_LEN);
		if (id == SCP_A_ID) {
			snprintf(scp_detail, SCP_AED_STR_LEN, "%s\nscp_A pc=0x%08x, lr=0x%08x, psp=0x%08x, sp=0x%08x\n"
			"scp_B pc=0x%08x, lr=0x%08x, psp=0x%08x, sp=0x%08x\nlast log:\n%s",
			aed_str, scp_A_aee_status.pc, scp_A_aee_status.lr, scp_A_aee_status.psp, scp_A_aee_status.sp,
			scp_B_aee_status.pc, scp_B_aee_status.lr, scp_B_aee_status.psp, scp_B_aee_status.sp, scp_A_log);
		} else {
			snprintf(scp_detail, SCP_AED_STR_LEN, "%s\nscp_A pc=0x%08x, lr=0x%08x, psp=0x%08x, sp=0x%08x\n"
			"scp_B pc=0x%08x, lr=0x%08x, psp=0x%08x, sp=0x%08x\nlast log:\n%s",
			aed_str, scp_A_aee_status.pc, scp_A_aee_status.lr, scp_A_aee_status.psp, scp_A_aee_status.sp,
			scp_B_aee_status.pc, scp_B_aee_status.lr, scp_B_aee_status.psp, scp_B_aee_status.sp, scp_B_log);
		}
		scp_detail[SCP_AED_STR_LEN - 1] = '\0';
	}

	/*prepare scp A db file*/
	memory_dump_size = 0;
	scp_dump_ptr = scp_A_dump_buffer_last;
	if (!scp_dump_ptr) {
		pr_err("[SCP AEE]MemoryDump buf is null, size=0x%x\n", memory_dump_size);
		memory_dump_size = 0;
	} else {
		pr_debug("[SCP AEE]scp A dump ptr:0x%llx\n", (unsigned long long)scp_dump_ptr);
		pMemoryDump = (MemoryDump *) scp_dump_ptr;
		memset(pMemoryDump, 0x0, sizeof(*pMemoryDump));
		memory_dump_size = scp_crash_dump(pMemoryDump, SCP_A_ID);
	}
	/* scp_dump_buffer_set */
	mutex_lock(&scp_A_excep_dump_mutex);
	scp_A_dump_buffer_last = scp_A_dump_buffer;
	scp_A_dump_buffer = scp_dump_ptr;
	scp_A_dump_length = memory_dump_size;
	mutex_unlock(&scp_A_excep_dump_mutex);

	/*prepare scp B db file*/
	memory_dump_size = 0;
	scp_dump_ptr = scp_B_dump_buffer_last;
	if (!scp_dump_ptr) {
		pr_err("[SCP AEE]SCP B MemoryDump buf is null, size=0x%x\n", memory_dump_size);
		memory_dump_size = 0;
	} else {
		pr_debug("[SCP AEE]scp B dump ptr:0x%llx\n", (unsigned long long)scp_dump_ptr);
		pMemoryDump = (MemoryDump *) scp_dump_ptr;
		memset(pMemoryDump, 0x0, sizeof(*pMemoryDump));
		memory_dump_size = scp_crash_dump(pMemoryDump, SCP_B_ID);
	}
	/* scp_dump_buffer_set */
	mutex_lock(&scp_A_excep_dump_mutex);
	scp_B_dump_buffer_last = scp_B_dump_buffer;
	scp_B_dump_buffer = scp_dump_ptr;
	scp_B_dump_length = memory_dump_size;
	mutex_unlock(&scp_A_excep_dump_mutex);

	aed->log = NULL;
	aed->log_size = 0;
	aed->phy = NULL;
	aed->phy_size = 0;
	aed->detail = scp_detail;
	aed->pMemoryDump = NULL;
	aed->memory_dump_size = 0;

	pr_debug("scp_prepare_aed_dump end\n");
}

/*
 * generate an exception according to exception type
 * @param type: exception type
 */
void scp_aed(scp_excep_id type, scp_core_id id)
{
	struct scp_aed_cfg aed;
	char *scp_aed_title;
	mutex_lock(&scp_excep_mutex);

	/* get scp title and exception type*/
	if ((type == EXCEP_RUNTIME) && (id == SCP_B_ID))
		type = (is_scp_ready(id)) ? EXCEP_RUNTIME : EXCEP_BOOTUP;

	switch (type) {
		case EXCEP_LOAD_FIRMWARE:
			scp_prepare_aed("scp firmware load exception", &aed);
			if (id == SCP_A_ID)
				scp_aed_title = "SCP_A load firmware exception";
			else if (id == SCP_B_ID)
				scp_aed_title = "SCP_B load firmware exception";
			else
				scp_aed_title = "SCP load firmware exception";
			break;
		case EXCEP_RESET:
			if (id == SCP_A_ID)
				scp_aed_title = "SCP_A reset exception";
			else if (id == SCP_B_ID)
				scp_aed_title = "SCP_B reset exception";
			else
				scp_aed_title = "SCP reset exception";
			break;
		case EXCEP_BOOTUP:
			if (id == SCP_A_ID)
				scp_aed_title = "SCP_A boot exception";
			else if (id == SCP_B_ID)
				scp_aed_title = "SCP_B boot exception";
			else
				scp_aed_title = "SCP boot exception";
			scp_get_log(id);
			break;
		case EXCEP_RUNTIME:
			if (id == SCP_A_ID)
				scp_aed_title = "SCP_A runtime exception";
			else if (id == SCP_B_ID)
				scp_aed_title = "SCP_B runtime exception";
			else
				scp_aed_title = "SCP runtime exception";
			scp_get_log(id);
			break;
		default:
			scp_prepare_aed("scp unknown exception", &aed);
			if (id == SCP_A_ID)
				scp_aed_title = "SCP_A unknown exception";
			else if (id == SCP_B_ID)
				scp_aed_title = "SCP_B unknown exception";
			else
				scp_aed_title = "SCP unknown exception";
			break;
	}
	/*print scp message*/
	pr_debug("scp_aed_title=%s", scp_aed_title);

	if (type != EXCEP_LOAD_FIRMWARE)
		scp_prepare_aed_dump(scp_aed_title, &aed, id);
	/*print detail info. in kernel*/
	pr_debug("%s", aed.detail);

	/* scp aed api, only detail information available*/
	aed_scp_exception_api(NULL, 0, NULL, 0, aed.detail, DB_OPT_DEFAULT);

	pr_debug("[SCP] scp exception dump is done\n");

	mutex_unlock(&scp_excep_mutex);
}

/*
 * generate an exception and reset scp right now
 * NOTE: this function may be blocked and should not be called in interrupt context
 * @param type: exception type
 */
void scp_aed_reset_inplace(scp_excep_id type, scp_core_id id)
{
	pr_debug("[SCP]scp_aed_reset_inplace\n");
	scp_aed(type, id);
#ifndef CFG_RECOVERY_SUPPORT
	/* workaround for QA, not reset SCP in WDT */
	if (type == EXCEP_RUNTIME) {
		return;
	}
#endif
	if (id == SCP_A_ID) {
		pr_debug("[SCP] SCP_A_REBOOT\n");
		reset_scp(SCP_A_REBOOT);
	} else {
		pr_debug("[SCP] SCP_B_REBOOT\n");
		reset_scp(SCP_B_REBOOT);
	}
}

/*
 * callback function for work struct
 * generate an exception and reset scp
 * NOTE: this function may be blocked and should not be called in interrupt context
 * @param ws:   work struct
 */
static void scp_aed_reset_ws(struct work_struct *ws)
{
	struct scp_work_struct *sws = container_of(ws, struct scp_work_struct, work);
	scp_excep_id type = (scp_excep_id) sws->flags;
	scp_core_id id = (scp_core_id) sws->id;

	pr_debug("[SCP]scp_aed_reset_ws: scp_excep_id=%u scp_core_id=%u\n", type, id);
	scp_aed_reset_inplace(type, id);
}

/* IPI for ramdump config
 * @param id:   IPI id
 * @param data: IPI data
 * @param len:  IPI data length
 */
static void scp_A_ram_dump_ipi_handler(int id, void *data, unsigned int len)
{
	scp_A_task_context_addr = *(unsigned int *)data;
	pr_debug("[SCP]get scp_A_task_context_addr: 0x%x\n", scp_A_task_context_addr);
}

/* IPI for ramdump config
 * @param id:   IPI id
 * @param data: IPI data
 * @param len:  IPI data length
 */
static void scp_B_ram_dump_ipi_handler(int id, void *data, unsigned int len)
{
	scp_B_task_context_addr = *(unsigned int *)data;
	pr_debug("[SCP]get scp_B_task_context_addr: 0x%x\n", scp_B_task_context_addr);
}
/*
 * schedule a work to generate an exception and reset scp
 * @param type: exception type
 */
void scp_aed_reset(scp_excep_id type, scp_core_id id)
{
	scp_aed_work.flags = (unsigned int) type;
	scp_aed_work.id = (unsigned int) id;
	scp_schedule_work(&scp_aed_work);
}

static ssize_t scp_A_dump_show(struct file *filep, struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;

	mutex_lock(&scp_A_excep_dump_mutex);

	if (offset >= 0 && offset < scp_A_dump_length) {
		if ((offset + size) > scp_A_dump_length)
			size = scp_A_dump_length - offset;

		memcpy(buf, scp_A_dump_buffer + offset, size);
		length = size;
	}

	mutex_unlock(&scp_A_excep_dump_mutex);

	return length;
}

static ssize_t scp_B_dump_show(struct file *filep, struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;

	mutex_lock(&scp_B_excep_dump_mutex);

	if (offset >= 0 && offset < scp_B_dump_length) {
		if ((offset + size) > scp_B_dump_length)
			size = scp_B_dump_length - offset;

		memcpy(buf, scp_B_dump_buffer + offset, size);
		length = size;
	}

	mutex_unlock(&scp_B_excep_dump_mutex);

	return length;
}

struct bin_attribute bin_attr_scp_dump = {
	.attr = {
		.name = "scp_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = scp_A_dump_show,
};

struct bin_attribute bin_attr_scp_B_dump = {
	.attr = {
		.name = "scp_B_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = scp_B_dump_show,
};


/*
 * init a work struct
 */
int scp_excep_init(void)
{
	mutex_init(&scp_excep_mutex);
	mutex_init(&scp_A_excep_dump_mutex);
	mutex_init(&scp_B_excep_dump_mutex);
	INIT_WORK(&scp_aed_work.work, scp_aed_reset_ws);

	/* alloc dump memory*/
	scp_A_detail_buffer = vmalloc(SCP_AED_STR_LEN);
	if (!scp_A_detail_buffer)
		return -1;
	scp_B_detail_buffer = vmalloc(SCP_AED_STR_LEN);
	if (!scp_B_detail_buffer)
		return -1;
	scp_A_dump_buffer = vmalloc(sizeof(MemoryDump));
	if (!scp_A_dump_buffer)
		return -1;
	scp_B_dump_buffer = vmalloc(sizeof(MemoryDump));
	if (!scp_B_dump_buffer)
		return -1;
	scp_A_dump_buffer_last = vmalloc(sizeof(MemoryDump));
	if (!scp_A_dump_buffer_last)
		return -1;
	scp_B_dump_buffer_last = vmalloc(sizeof(MemoryDump));
	if (!scp_B_dump_buffer_last)
		return -1;

	/* init global values */
	scp_A_dump_length = 0;
	scp_B_dump_length = 0;

	return 0;
}
/*
 * ram dump init
 */
void scp_ram_dump_init(void)
{
	/* init global values */
	scp_A_task_context_addr = 0;
	scp_B_task_context_addr = 0;
	/* ipi handler registration */
	scp_ipi_registration(IPI_SCP_A_RAM_DUMP, scp_A_ram_dump_ipi_handler, "A_ramdp");
	scp_ipi_registration(IPI_SCP_B_RAM_DUMP, scp_B_ram_dump_ipi_handler, "B_ramdp");
	pr_debug("[SCP] ram_dump_init() done\n");
}

/*
 * cleanup scp exception
 */
void scp_excep_cleanup(void)
{
	vfree(scp_A_detail_buffer);
	vfree(scp_B_detail_buffer);
	vfree(scp_A_dump_buffer_last);
	vfree(scp_B_dump_buffer_last);
	vfree(scp_A_dump_buffer);
	vfree(scp_B_dump_buffer);
	scp_A_task_context_addr = 0;
	scp_B_task_context_addr = 0;
	pr_debug("[SCP] scp_excep_cleanup() done\n");
}
