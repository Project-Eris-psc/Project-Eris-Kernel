#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/fd.h>
#include <linux/tty.h>
#include <linux/suspend.h>
#include <linux/root_dev.h>
#include <linux/security.h>
#include <linux/delay.h>
#include <linux/genhd.h>
#include <linux/mount.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/initrd.h>
#include <linux/async.h>
#include <linux/fs_struct.h>
#include <linux/slab.h>
#include <linux/ramfs.h>
#include <linux/shmem_fs.h>
#include <linux/syscalls.h>

#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include <linux/nfs_mount.h>
#include <linux/crypto.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>

#include "rsa.h"
#include "nfsb_key_modulus.h"
#include "../do_mounts.h"

#pragma pack(push)
#pragma pack(1)
struct ubick_header {
	char magic[4];
	uint8_t version;
	uint8_t hash_type;
	uint32_t block_size;
	uint32_t block_interval;
	uint32_t block_count;
	uint8_t hash_salt[32];
	uint8_t rsa_sig[256];
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
struct ubi_ec_hdr {
	uint32_t  magic;
	char    version;
	char    padding1[3];
	uint64_t  ec; /* Warning: the current limit is 31-bit anyway! */
	uint32_t  vid_hdr_offset;
	uint32_t  data_offset;
	uint32_t  image_seq;
	char    padding2[32];
	uint32_t  hdr_crc;
};
#pragma pack(pop)

#define RSA_LEN 256
#define RSA_GAP 0x10

extern struct mtd_info *get_mtd_device(struct mtd_info *mtd, int num);
extern int mtd_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen,
	     u_char *buf);
extern void put_mtd_device(struct mtd_info *mtd);
extern int mtd_block_isbad(struct mtd_info *mtd, loff_t ofs);

void dump_hex(const char* name,uint8_t buff[],int len)
{
	int i=0;
	printk(KERN_ERR"%s content is:",name);
	for(;i<len;i++)
	{
		if(i%16 == 0) printk(KERN_ERR"\n");
		printk(KERN_ERR"0x%x ",buff[i]);
	}
	printk(KERN_ERR"\n");
}

#define MAX_BUF_LEN 32
int get_rootfs_mtd_num(void)
{
	char* cmdline = saved_command_line;
	int idx_start = 0,idx_end = 0;
	int cmdline_len = strlen(cmdline);
	char tmp_buf[MAX_BUF_LEN]={0};
	long rootfs_mtd_num;

	for(;idx_start<cmdline_len-strlen("ubi.mtd");idx_start++)
	{
		if(memcmp(cmdline+idx_start,"ubi.mtd",strlen("ubi.mtd"))==0)
		{
			break;
		}
	}

	if(idx_start == strlen(cmdline)-strlen("ubi.mtd"))
	{
		return -1;
	}

	while(idx_start < cmdline_len && cmdline[idx_start]!='=') idx_start++;

	if(idx_start == cmdline_len) return -1;

	idx_end = ++idx_start;

	while(idx_end < cmdline_len && cmdline[idx_end]!=',' && cmdline[idx_end]!=' ') idx_end++;

	if(idx_end == cmdline_len) return -1;

	memcpy(tmp_buf,cmdline+idx_start,idx_end-idx_start);

	if(kstrtol(tmp_buf,0,&rootfs_mtd_num)!=0)
	{
		return -1;
	}

	return (int)rootfs_mtd_num;
}

int mtd_find_goodblk(struct mtd_info *mtd, loff_t *off)
{
	u32 i = 0;
	int ret;
	loff_t real_off = (loff_t)mtd->erasesize * -1;
	loff_t tmp_logic_off = (loff_t)mtd->erasesize * -1;

	while(1) {
		int isbad = mtd_block_isbad(mtd, i * mtd->erasesize);
		if (isbad == -EINVAL) {
			pr_err("[rootfs_check]can not find enough good blocks for offset %lld!\n", *off);
			return isbad;
		}
		if(!isbad)
			tmp_logic_off += mtd->erasesize;
		real_off += mtd->erasesize;
		if(tmp_logic_off == ((*off>>mtd->erasesize_shift)<<mtd->erasesize_shift))
			break;
		i++;
	}
	while (1) {
		ret = mtd_block_isbad(mtd, real_off);
		if (!ret)
			break;
		else if (ret == -EINVAL) {
			pr_err("[rootfs_check]can not find enough good blocks for offset %lld!\n", *off);
			return ret;
		}
		else
			real_off += mtd->erasesize;
	}

	*off = real_off + (*off - ((*off>>mtd->erasesize_shift)<<mtd->erasesize_shift));

	return 0;
}

#define HASH_LEN 32
#define CHECK_FAIL -1
#define CHECK_PASS 0
int mtd_verity(int mtd_num)
{
	struct mtd_info* root_mtd = get_mtd_device(NULL,mtd_num);
	size_t retlen;
	size_t ubi_image_len;
	struct ubick_header *header;
	uint32_t block_size,block_interval,block_count;
	u_char* buf;
	uint32_t block_size_order;
	struct page* pages;
	uint8_t* calulate_buffer;
	uint8_t* decrypted_content;
	uint8_t excepted_e[4] = {0x00, 0x01, 0x00, 0x01};
	uint8_t calculate_hash[HASH_LEN];
	int i=0;
	struct scatterlist sg;
	struct crypto_hash *tfm;
	struct hash_desc desc;
	int ret = CHECK_PASS;
	loff_t real_offset, log_offset;
	size_t readlen;

	if(root_mtd == NULL)
		return CHECK_FAIL;

	buf = kmalloc(PAGE_SIZE,GFP_KERNEL);
	if(buf == NULL)
	{
		ret = CHECK_FAIL;
		goto fail1;
	}

	real_offset = 0;
	if (mtd_find_goodblk(root_mtd, &real_offset))
		return CHECK_FAIL;
	mtd_read(root_mtd, real_offset, PAGE_SIZE, &retlen,buf);

	ubi_image_len = be32_to_cpu(*(uint32_t*)&buf[sizeof(struct ubi_ec_hdr)-2*sizeof(uint32_t)]);

	real_offset = ubi_image_len;
	if (mtd_find_goodblk(root_mtd, &real_offset))
		return CHECK_FAIL;

	readlen = root_mtd->erasesize
		-(real_offset -((real_offset>>root_mtd->erasesize_shift)<<root_mtd->erasesize_shift));
	if (readlen < PAGE_SIZE) {
		mtd_read(root_mtd, real_offset, readlen, &retlen,buf);
		real_offset = ubi_image_len + readlen;
		if (mtd_find_goodblk(root_mtd, &real_offset))
			return CHECK_FAIL;
		mtd_read(root_mtd, real_offset, PAGE_SIZE - readlen, &retlen,buf + readlen);
	} else {
		mtd_read(root_mtd, real_offset, PAGE_SIZE, &retlen,buf);
	}

	header = (struct ubick_header*)buf;
	if(memcmp(header->magic,"UBIC",sizeof("UBIC")!=0))
	{
		pr_err("[rootfs_check]mtd_verity header magic is not matched!\n");
		ret = CHECK_FAIL;
		goto fail2;
	}
	block_size = be32_to_cpu(header->block_size);
	block_interval = be32_to_cpu(header->block_interval);
	block_count = be32_to_cpu(header->block_count);

	block_size_order = get_order(block_size);
	pages = alloc_pages(GFP_KERNEL,block_size_order);
	if(pages == NULL)
	{
		pr_err("[rootfs_check]block_size order %d is too large,please reduce block_size\n",block_size_order);
		ret = CHECK_FAIL;
		goto fail2;
	}
	calulate_buffer = page_address(pages);

	tfm = crypto_alloc_hash("sha256",0,0);
	desc.tfm = tfm;
	desc.flags = 0;
	crypto_hash_init(&desc);

	if(block_count*(block_size+block_interval)-block_interval>ubi_image_len)
	{
		pr_err("[rootfs_check]error block parameters\n");
		ret = CHECK_FAIL;
		goto fail3;
	}

	for(i=0;i<block_count;i++)
	{
		readlen = block_size;
		log_offset = i*(block_size+block_interval);
		real_offset = log_offset;
		while (1) {
			size_t bytes = min((size_t)(root_mtd->erasesize
				- (log_offset -((log_offset>>root_mtd->erasesize_shift)<<root_mtd->erasesize_shift))),
				readlen);
			if (mtd_find_goodblk(root_mtd, &real_offset))
				return CHECK_FAIL;
			mtd_read(root_mtd,real_offset,bytes,&retlen,calulate_buffer+(block_size-readlen));
			readlen -= bytes;
			if (!readlen)
				break;
			log_offset += bytes;
			real_offset = log_offset;
		}
		sg_init_one(&sg,calulate_buffer,block_size);
		crypto_hash_update(&desc,&sg,block_size);
	}

	sg_init_one(&sg,header,sizeof(struct ubick_header)-sizeof(header->rsa_sig));

	crypto_hash_update(&desc,&sg,sizeof(struct ubick_header)-sizeof(header->rsa_sig));

	crypto_hash_final(&desc,calculate_hash);

	decrypted_content=rsa_encryptdecrypt(header->rsa_sig,excepted_e,rsa_modulus);

	if(memcmp(calculate_hash,decrypted_content+RSA_GAP,HASH_LEN)!=0)
		ret = CHECK_FAIL;
	else
		ret = CHECK_PASS;

	kfree(decrypted_content);
fail3:
	crypto_free_hash(tfm);
	__free_pages(pages,block_size_order);
fail2:
	kfree(buf);
fail1:
	put_mtd_device(root_mtd);

	return ret;
}

#define DEV_VERITY "/dev/rootfs_verity"
int bdev_verity(dev_t rootfs_dev)
{
	int root_fd = -1;
	size_t ubi_image_len;
	struct ubick_header *header;
	uint32_t block_size,block_interval,block_count;
	u_char* buf;
	uint32_t block_size_order;
	struct page* pages;
	uint8_t* calulate_buffer;
	uint8_t* decrypted_content;
	uint8_t excepted_e[4] = {0x00, 0x01, 0x00, 0x01};
	uint8_t calculate_hash[HASH_LEN];
	int i=0;
	struct scatterlist sg;
	struct crypto_hash *tfm;
	struct hash_desc desc;
	int ret = CHECK_PASS;
	int err;
	loff_t real_offset, log_offset;
	size_t readlen;

	err = create_dev(DEV_VERITY, rootfs_dev);
	if (err < 0) {
		pr_err("rootfs_check: create_dev failed\n");
		return -1;
	}

	root_fd = sys_open(DEV_VERITY, O_RDONLY, 0);
	if (root_fd < 0) {
		pr_err("rootfs_check: open %s failed\n", DEV_VERITY);
		ret = CHECK_FAIL;
		goto fail0;
	}
	
	buf = kmalloc(PAGE_SIZE,GFP_KERNEL);
	if(buf == NULL)
	{
		ret = CHECK_FAIL;
		goto fail1;
	}

	real_offset = 0;
	sys_lseek(root_fd,real_offset,0);
	sys_read(root_fd, buf,PAGE_SIZE);
	ubi_image_len = (*(unsigned int*)(buf+0x400+0x4))*(1<<(10+*(unsigned int*)(buf+0x400+0x18)));
	
	real_offset = ubi_image_len;
	sys_lseek(root_fd,real_offset,0);
	sys_read(root_fd, buf, PAGE_SIZE);

	header = (struct ubick_header*)buf;
	if(memcmp(header->magic,"EXTC",sizeof("EXTC")!=0))
	{
		pr_err("[rootfs_check]bdev_verity header magic is not matched!\n");
		ret = CHECK_FAIL;
		goto fail2;
	}
	block_size = be32_to_cpu(header->block_size);
	block_interval = be32_to_cpu(header->block_interval);
	block_count = be32_to_cpu(header->block_count);

	block_size_order = get_order(block_size);
	pages = alloc_pages(GFP_KERNEL,block_size_order);
	if(pages == NULL)
	{
		pr_err("[rootfs_check]block_size order %d is too large,please reduce block_size\n",block_size_order);
		ret = CHECK_FAIL;
		goto fail2;
	}
	calulate_buffer = page_address(pages);

	tfm = crypto_alloc_hash("sha256",0,0);
	desc.tfm = tfm;
	desc.flags = 0;
	crypto_hash_init(&desc);

	if(block_count*(block_size+block_interval)-block_interval>ubi_image_len)
	{
		pr_err("[rootfs_check]error block parameters\n");
		ret = CHECK_FAIL;
		goto fail3;
	}

	for(i=0;i<block_count;i++)
	{
		readlen = block_size;
		log_offset = i*(block_size+block_interval);
		real_offset = log_offset;
		sys_lseek(root_fd,real_offset,0);
		sys_read(root_fd,calulate_buffer,readlen);
		sg_init_one(&sg,calulate_buffer,block_size);
		crypto_hash_update(&desc,&sg,block_size);
	}

	sg_init_one(&sg,header,sizeof(struct ubick_header)-sizeof(header->rsa_sig));

	crypto_hash_update(&desc,&sg,sizeof(struct ubick_header)-sizeof(header->rsa_sig));

	crypto_hash_final(&desc,calculate_hash);

	decrypted_content=rsa_encryptdecrypt(header->rsa_sig,excepted_e,rsa_modulus);

	if(memcmp(calculate_hash,decrypted_content+RSA_GAP,HASH_LEN)!=0)
		ret = CHECK_FAIL;
	else
		ret = CHECK_PASS;

	kfree(decrypted_content);
fail3:
	crypto_free_hash(tfm);
	__free_pages(pages,block_size_order);
fail2:
	kfree(buf);
fail1:
	sys_close(root_fd);
fail0:
	sys_unlink(DEV_VERITY);

	return ret;
}

