/*
<:copyright-gpl
 Copyright 2012 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
:>
*/
/*
 ***************************************************************************
 * File Name  : vflash.c
 *
 * Description: The virtual flash driver which does flash io via
 * IPC calls to eCos.
 *
 * How it works:
 * DQM channels are required to be set up by the Linux client CPU
 * (hence the message box message). After that dqm io channels are
 * used to send messages back and forth. The main io happens in
 * do_rpc_io, which is reentrant. The isr handler walks a linked
 * list of outstanding requests to notify tasks.
 *
 * Still todo:
 * There are still thinks which need
 * to go in a shared header file (e.g. the message struct) and various
 * DQM reg addresses and the flash partition map needs an erase size.
 *
 * Created on :  4/4/2012  Peter Sulc
 *
 ***************************************************************************/
#include <linux/version.h>
#include <linux/param.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/semaphore.h>
#include <linux/hardirq.h>
#include <linux/types.h>
#include <linux/compile.h>
#include <linux/uts.h>
#include <linux/utsname.h>
#include <linux/utsrelease.h>

#include <bcm_map_part.h> 

#include <flash_api.h>
#include <board.h>
#include <dqm.h>
#include <hostDqmHalIf.h>
#include <bcm_intr.h>
#include <ProgramStore.h>

#include "flashmap.h"

#if defined(CONFIG_MTD_BRCMNAND)
#define DEV_FUNC(devid, op, partition, bytes) \
	( (devid << 28) | (op << 24) | ((partition-1) << 21) | bytes )
#else
#define DEV_FUNC(devid, op, partition, bytes) \
	( (devid << 28) | (op << 24) | (partition << 21) | bytes )
#endif

#define DEBUG_DQM_IO	0
#define DEBUG_VFLASH_MTD	0

#if defined(CONFIG_BCM93383)
#if defined(CONFIG_BCM_LOT1)
#define INTERRUPT_ID_VFLASH	INTERRUPT_ID_PMC_IRQ
#define ITC_OUTBOX_REG_ADDRESS 0x83fffff0
#define ITC_INBOX_REG_ADDRESS 0x83fffff4
#else
#define INTERRUPT_ID_VFLASH	INTERRUPT_ID_DQM_IRQ
#define ITC_OUTBOX_REG_ADDRESS 0xb600102c
#endif
#define VFLASH_DQM_RX_PROC			PMC
#define VFLASH_DQM_TX_PROC			PMC
#define VFLASH_DQM_RX_Q				DQM_ITC_CTL_RX_Q
#define VFLASH_DQM_TX_Q				DQM_ITC_CTL_TX_Q
#elif defined(CONFIG_BCM93380)
#define INTERRUPT_ID_VFLASH	INTERRUPT_ID_MEP_IRQ
#define ITC_OUTBOX_REG_ADDRESS 0xb4610000
#define VFLASH_DQM_RX_PROC			MPEG_PROC
#define VFLASH_DQM_TX_PROC			MSG_PROC
#define VFLASH_DQM_RX_Q				0
#define VFLASH_DQM_TX_Q				22
#else
#define INTERRUPT_ID_VFLASH vflash needs a chip specific interrupt irq number
#define ITC_OUTBOX_REG_ADDRESS vflash needs a handshake mbox address
#endif

#define RPC_IRQ_MISS_TIMEOUT		(HZ/4)
#define RPC_TIMEOUT_RESEND_SECS		3
#define RPC_TIMEOUT_RESEND_CNT		((RPC_TIMEOUT_RESEND_SECS * HZ) / RPC_IRQ_MISS_TIMEOUT)

#if defined(CONFIG_MTD_BRCMNAND)
#define NAND_CACHE_SIZE 512
/*
 * 4K page MLC with BCH-4 ECC, uses 7 ECC bytes per 512B ECC step
 */
static struct nand_ecclayout brcmnand_oob_bch4_4k = {
	.eccbytes	= 7*8,  /* 7*8 = 56 bytes */
	.eccpos		= { 
		9,10,11,12,13,14,15,
		25,26,27,28,29,30,31,
		41,42,43,44,45,46,47,
		57,58,59,60,61,62,63,
		73,74,75,76,77,78,79,
		89,90,91,92,93,94,95,
		105,106,107,108,109,110,111,
		121,122,123,124,125,126,127
		},
	.oobfree	= { /* 0  used for BBT and/or manufacturer bad block marker, 
	                    * first slice loses 1 byte for BBT */
				{.offset=1, .length=8}, 		/* 1st slice loses byte 0 */
				{.offset=16,.length=9}, 		/* 2nd slice  */
				{.offset=32, .length=9},		/* 3rd slice  */
				{.offset=48, .length=9},		/* 4th slice */
				{.offset=64, .length=9},		/* 5th slice */
				{.offset=80, .length=9},		/* 6th slice */
				{.offset=96, .length=9},		/* 7th slice */
				{.offset=112, .length=9},		/* 8th slice */
	            		//{.offset=0, .length=0}			/* End marker */
			}
};

/*
 * 2K page SLC/MLC with BCH-4 ECC, uses 7 ECC bytes per 512B ECC step
 */
static struct nand_ecclayout brcmnand_oob_bch4_2k = {
	.eccbytes	= 7*4,  /* 7*4 = 28 bytes */
	.eccpos		= { 
		9,10,11,12,13,14,15,
		25,26,27,28,29,30,31,
		41,42,43,44,45,46,47,
		57,58,59,60,61,62,63
		},
	.oobfree	= { /* 0  used for BBT and/or manufacturer bad block marker, 
	                    * first slice loses 1 byte for BBT */
				{.offset=1, .length=8}, 		/* 1st slice loses byte 0 */
				{.offset=16,.length=9}, 		/* 2nd slice  */
				{.offset=32, .length=9},		/* 3rd slice  */
				{.offset=48, .length=9},		/* 4th slice */
	            		{.offset=0, .length=0}			/* End marker */
			}
};


#endif

/***************************************************************
 * DQMChannel
 **************************************************************/
typedef struct
{
	unsigned dqmRxHandle;
	unsigned dqmTxHandle;
}	DQMChannel;

static DQMChannel dqmchannel;

/***************************************************************
 * MSG Queue
 **************************************************************/
typedef struct
{
    uint32 dev_func; // bit28-31 : device id; bit24-27 : function id; bit0-23 : user defined 
    uint32 xid;      // timestamp
    uint32 u0;       // user defined
    uint32 u1;       // user defined
} ItcRpcMsg;

typedef struct RpcMsgQueue
{
	uint32 				xid;
	ItcRpcMsg 			*response;
	struct semaphore	*sema;
	struct RpcMsgQueue	*next;
}	RpcMsgQueue;

static DEFINE_SPINLOCK(rpc_msg_queue_lock);
static RpcMsgQueue	*rpc_msg_queue_head = NULL;
static RpcMsgQueue	*rpc_msg_queue_tail = NULL;
static int rpc_msg_queue_cnt = 0;

/***************************************************************
 * from the bootline
 **************************************************************/
extern unsigned int get_part_offset(void);
extern unsigned int get_part_size(void);

#if defined(CONFIG_MTD_BRCMNAND)
typedef struct
{
	uint32	data_buf;
	uint32	ecc_stat_buf;
}	BufArray;
#endif

/***************************************************************
 * block cache
 **************************************************************/
typedef struct
{
	u_char 				*buf;
	uint32				start_addr;
	uint32				end_addr;
#if defined(CONFIG_MTD_BRCMNAND)
	u_char 				*ecc_stat_buf;
	BufArray			*buf_array;
#endif
	struct semaphore	sema;
}	BlockCache;

static void	init_block_cache(void);
static BlockCache *find_block_cache(uint32 addr);
static BlockCache *new_block_cache(uint32 addr, uint32 size);
static void release_block_cache(BlockCache *bc);
static void invalidate_block_cache(BlockCache *bc);

/***************************************************************
 * other stuff
 **************************************************************/
#if defined(CONFIG_MTD_BRCMNAND)
static char bcmmtd_name[] = "bcmmtd_nand_device";
DECLARE_MUTEX(wr_buf_sem);
static unsigned char *wr_buf;
static struct mtd_partition *vflash_nand_mtd_parts = NULL;
static struct vflash_info_t *vflash_info = NULL;
static unsigned int vflash_fetch_size = 128 * 1024;
static unsigned int ecc_stat_buf_len;
#else
static char bcmmtd_name[] = "bcmmtd_device";
#endif

static long rpc_io_timeout = RPC_IRQ_MISS_TIMEOUT;

static DEFINE_SPINLOCK(itc_msg_lock);

/* 
 * Put UTS_RELEASE immediately after a NULL string terminator. On eCos side we use the intermediate
 * NULL terminator to find the UTS_RELEASE and copy it to the ItcVersionString buffer and thus 
 * have it only show up in the banner in one place. 
 */
static const char dist_banner[] __initdata =
    LINUX_IMAGE "\n"
	UTS_VERSION "\n"
	LINUX_COMPILE_BY "@" LINUX_COMPILE_HOST "." LINUX_COMPILE_DOMAIN "\n"
    LINUX_COMPILER "\0"
    UTS_RELEASE;

static irqreturn_t vflash_isr(int irq, void *unused);

/***************************************************************
 * mtd partition stuff
 **************************************************************/
static flash_partition_t	*flash_partition_ptr;
static struct mtd_info      *vflash_mtd = NULL;
static struct mtd_partition *vflash_mtd_parts = NULL;

static int get_flash_partition(loff_t address);

void bcm_cache_wback_inv(uint32 start, uint32 size)
{
#if !(defined(CONFIG_BCM93383) && defined(CONFIG_BCM_LOT1))
    dma_cache_wback_inv(start, size);
#endif
}

void bcm_cache_inv(uint32 start, uint32 size)
{
#if !(defined(CONFIG_BCM93383) && defined(CONFIG_BCM_LOT1))
    dma_cache_inv(start, size);
#endif
}

/***************************************************************
 * msg queue code
 **************************************************************/
static void add_to_msg_queue(RpcMsgQueue *msg)
{
	unsigned long flags;
	
	spin_lock_irqsave(&rpc_msg_queue_lock, flags);

	if (rpc_msg_queue_tail)
		rpc_msg_queue_tail->next = msg;
	else
		rpc_msg_queue_head = msg;
	rpc_msg_queue_tail = msg;

	rpc_msg_queue_cnt++;
		
	spin_unlock_irqrestore(&rpc_msg_queue_lock, flags);
}

static RpcMsgQueue *extract_from_msg_queue(uint32 xid)
{
	unsigned long	flags;
	RpcMsgQueue		*prev, *msg;
	
	spin_lock_irqsave(&rpc_msg_queue_lock, flags);
	
	msg = rpc_msg_queue_head;
	prev = NULL;
	while (msg) {
		if (msg->xid == xid) {
			if (prev) {
				prev->next = msg->next;
				if (rpc_msg_queue_tail == msg)
					rpc_msg_queue_tail = prev;
			}
			else {
				rpc_msg_queue_head = msg->next;
				if (rpc_msg_queue_tail == msg)
					rpc_msg_queue_tail = msg->next; /* NULL */
			}
			rpc_msg_queue_cnt--;
			break;
		}
		prev = msg;
		msg = msg->next;
	}
	spin_unlock_irqrestore(&rpc_msg_queue_lock, flags);
	return msg;
}

static void dump_msg(RpcMsgQueue *msg)
{
	if (msg)
		printk("msg=%p xid=%08lx next=%p\n", msg, msg->xid, msg->next);
	else
		printk("msg=NULL\n");
}

static void dump_msg_queue(void)
{
	unsigned long	flags;
	RpcMsgQueue		*msg;
	spin_lock_irqsave(&rpc_msg_queue_lock, flags);
	printk("MSG QUEUE count: %d\n", rpc_msg_queue_cnt);
	printk("head: ");
	dump_msg(rpc_msg_queue_head);
	printk("tail: ");
	dump_msg(rpc_msg_queue_tail);

	msg = rpc_msg_queue_head;
	while (msg) {
		dump_msg(msg);
		msg = msg->next;
	}
	spin_unlock_irqrestore(&rpc_msg_queue_lock, flags);
}

#if defined(CONFIG_MTD_BRCMNAND)
static int check_ecc_stat(struct mtd_info *mtd, uint8 *ecc_stat_buf, int offset, int size)
{
	int ofs, ret;

	ret = 0;
	while(size > 0) {
		ofs = offset / NAND_CACHE_SIZE;
		if(ecc_stat_buf[ofs] > 0) {
			mtd->ecc_stats.corrected += ecc_stat_buf[ofs];
			/*printk("ECC error count = %d\n", (int)ecc_stat_buf[ofs]);*/
			ecc_stat_buf[ofs] = 0;
			ret = -EUCLEAN; 
		}
		size -= NAND_CACHE_SIZE;
		offset += NAND_CACHE_SIZE;
	}
	return ret;
}
#endif

#if defined(CONFIG_BCM93383) && defined(CONFIG_BCM_LOT1)
void disable_rpc_io_timeout(void)
{
	rpc_io_timeout = MAX_SCHEDULE_TIMEOUT;
	sysctl_hung_task_timeout_secs = 0;
	sysctl_hung_task_warnings = 0;
	printk("RPC IO timeout is disabled!\n");
}
#endif

/*******************************************************************
 * do_rpc_io:
 * Put a message on the oustanding request queue.
 * Send the request through the dqm fifo.
 * Wait until the response with the correct transaction id comes in,
 * or timeout after 10 seconds.
 ******************************************************************/

int do_rpc_io(ItcRpcMsg *req)
{
	static DEFINE_SPINLOCK(io_lock);
	unsigned long flags;

    MsgStruct           msgBuf;
    ItcRpcMsg         	response;
	RpcMsgQueue			msgQueueItem;
	int					timeout_cnt = 0;
	DECLARE_MUTEX(sema);

    // Construct a message
    memset((void *)&msgBuf, 0, sizeof(MsgStruct));
    msgBuf.msgLen = sizeof(ItcRpcMsg)/4;
    memcpy((void *)&msgBuf.msgData[0], (void *)req, sizeof(ItcRpcMsg));
#if DEBUG_DQM_IO
	printk(">>>> %s %08lx %08lx %08lx %08lx\n",
		   __func__, msgBuf.msgData[0], msgBuf.msgData[1], msgBuf.msgData[2], msgBuf.msgData[3]);
#endif
    memset(&response, 0, sizeof(response));
	msgQueueItem.xid 		= req->xid;
	msgQueueItem.response 	= &response;
	msgQueueItem.sema 		= &sema;
	msgQueueItem.next 		= NULL;

	add_to_msg_queue(&msgQueueItem);

	down(&sema);
	
	spin_lock_irqsave(&io_lock, flags);
    if(hostDqmSendMessage(&msgBuf, dqmchannel.dqmTxHandle))
    {
        printk("%s:%d - Can not send rpc message!\n", __func__, __LINE__);
		spin_unlock_irqrestore(&io_lock, flags);
        return -1;
    }
	hostDqmEnableDqmQInt(dqmchannel.dqmRxHandle);
	spin_unlock_irqrestore(&io_lock, flags);
	enable_brcm_irq(INTERRUPT_ID_VFLASH);
	while (down_timeout(&sema, rpc_io_timeout))
	{
		timeout_cnt++;
		vflash_isr(-1, NULL);
		if (timeout_cnt >= 60) {
			dump_msg_queue();
			extract_from_msg_queue(msgQueueItem.xid);
			return -ETIMEDOUT;
		}
		else if ((timeout_cnt >= RPC_TIMEOUT_RESEND_CNT) &&
				 (timeout_cnt % RPC_TIMEOUT_RESEND_CNT == 0)) {
			printk("%s: TIMEOUT resending %08lx\n", __func__, msgQueueItem.xid);
			spin_lock_irqsave(&io_lock, flags);
			if(hostDqmSendMessage(&msgBuf, dqmchannel.dqmTxHandle))
			{
				printk("%s:%d - Can not send rpc message!\n", __func__, __LINE__);
				spin_unlock_irqrestore(&io_lock, flags);
				return -1;
			}
			spin_unlock_irqrestore(&io_lock, flags);
		}
	}
#if DEBUG_DQM_IO
	if (timeout_cnt)
	{
		printk("%s: %d timeouts but I got a reply for msg %08lx\n",
			   __func__, timeout_cnt, msgQueueItem.xid);
	}
#endif
	return response.u0;
}

/*******************************************************************
 * vflash_isr: the interrupt handler for dqm messages
 ******************************************************************/
static irqreturn_t vflash_isr(int irq, void *unused)
{
	MsgStruct       rxMsg;

	while (hostDqmGetMessage(&rxMsg, dqmchannel.dqmRxHandle)) {
		
        ItcRpcMsg *rpcMsg =  (ItcRpcMsg *)&rxMsg.msgData[0];
		RpcMsgQueue *msg = extract_from_msg_queue(rpcMsg->xid);

		if (msg) {
#if DEBUG_DQM_IO
			if (irq == -1)
				printk("%s: extracted %08x\n", __func__, rpcMsg->xid);
#endif
			memcpy(msg->response, rpcMsg, sizeof(ItcRpcMsg));
			up(msg->sema);
		}
		else {
			printk("%s: unable to find message id %08x\n", __func__, (unsigned int)rpcMsg->xid);
			dump_msg_queue();
		}
	}
	hostDqmEnableDqmQInt(dqmchannel.dqmRxHandle);
	enable_brcm_irq(INTERRUPT_ID_VFLASH);
    return IRQ_HANDLED;
}

/*******************************************************************
 * vflash_read_buf:
 * create a request message and send it via do_rpc_io
 * do_rpc_io waits for response or timeout
 ******************************************************************/
static int vflash_read_buf(int partition, int offset,
						   u_char *buffer, int numbytes)
{
    ItcRpcMsg req;
    u_char *vmallocated_buf = NULL;
    int ret, is_buf_vmallocated;
#if defined(CONFIG_MTD_BRCMNAND)
    uint8 * ecc_buf;
    uint8 * data_buf;
#endif

    /* VMallocated (mmu translated) memory can't be used by the eCos CPU */
    is_buf_vmallocated = KSEGX(buffer) == KSEG2;
    if (is_buf_vmallocated)
    {
		vmallocated_buf = buffer;
		buffer = kmalloc(numbytes, GFP_KERNEL);
		if (!buffer)
			return -EINVAL;
    }

    // Construct a request message
    memset((void *)&req, 0, sizeof(req));
    req.dev_func = DEV_FUNC(REMOTE_FLASH_DEVICE_ID, REMOTE_READ, partition, numbytes);
    req.xid = read_c0_count();
    req.u0 = (uint32)buffer;
    req.u1 = offset;

#if !defined(CONFIG_MTD_BRCMNAND)
    bcm_cache_inv((uint32)buffer, (uint32)numbytes);
#else
    data_buf = (uint8 *)(((BufArray *)buffer)->data_buf);
    if(data_buf) {
        bcm_cache_inv((uint32)data_buf, (uint32)numbytes);
    }
    ecc_buf = (uint8 *)(((BufArray *)buffer)->ecc_stat_buf);
    if(ecc_buf) {
	bcm_cache_inv((uint32)ecc_buf, (uint32)ecc_stat_buf_len);
    }
    bcm_cache_wback_inv((uint32)buffer, (uint32)sizeof(BufArray));
#endif

#if DEBUG_DQM_IO
	printk("%s partition %d offset %08x buffer %p size %d\n",
		   __func__, partition, offset, buffer, numbytes);
#endif
	
    ret =  do_rpc_io(&req);

    if (is_buf_vmallocated)
    {
		memcpy(vmallocated_buf, buffer, numbytes);
		kfree(buffer);
    }

	return ret;
}

/*******************************************************************
 * vflash_write_buf:
 * create a request message and send it via do_rpc_io
 * do_rpc_io waits for response or timeout
 ******************************************************************/
static int vflash_write_buf(int partition, int offset,
							u_char *buffer, int numbytes)
{
    ItcRpcMsg req;
    u_char *vmallocated_buf = NULL;
    int ret, is_buf_vmallocated;

    /* VMallocated (mmu translated) memory can't be used by the eCos CPU */
    is_buf_vmallocated = KSEGX(buffer) == KSEG2;
    if (is_buf_vmallocated)
    {
		vmallocated_buf = buffer;
		buffer = kmalloc(numbytes, GFP_KERNEL);
		if (!buffer)
			return -EINVAL;
		memcpy(buffer, vmallocated_buf, numbytes);
    }

    // Construct a request message
    memset((void *)&req, 0, sizeof(req));
    req.dev_func = DEV_FUNC(REMOTE_FLASH_DEVICE_ID, REMOTE_WRITE, partition, numbytes);
    req.xid = read_c0_count();
    req.u0 = (uint32)buffer;
    req.u1 = offset;

	bcm_cache_wback_inv((uint32)buffer, (uint32)numbytes);
	
#if DEBUG_DQM_IO 
	printk("%s partition %d offset %08x buffer %p size %d\n",
		   __func__, partition, offset, buffer, numbytes);
#endif
    ret = do_rpc_io(&req);

    if (is_buf_vmallocated)
		kfree(buffer);

	return ret;
}
/*******************************************************************
 * vflash_erase_block:
 * create a request message and send it via do_rpc_io
 * do_rpc_io waits for response or timeout
 ******************************************************************/
static int vflash_erase_block(int partition, int offset, int blocksize)
{
    ItcRpcMsg req;

    // Construct a request message
    memset((void *)&req, 0, sizeof(req));
    req.dev_func = DEV_FUNC(REMOTE_FLASH_DEVICE_ID, REMOTE_ERASE, partition, blocksize);
    req.xid = read_c0_count();
    req.u0 = 0;
    req.u1 = offset;

#if DEBUG_DQM_IO
	printk("%s partition %d offset %08x\n", __func__, partition, offset);
#endif
	return do_rpc_io(&req);
}

#if defined(CONFIG_MTD_BRCMNAND)
static int vflash_block_isbad(int partition, int offset)
{
    ItcRpcMsg req;
    int ret;

    // Construct a request message
    memset((void *)&req, 0, sizeof(req));
    req.dev_func = DEV_FUNC(REMOTE_FLASH_DEVICE_ID, REMOTE_BLOCK_ISBAD, partition, 0);
    req.xid = read_c0_count();
    req.u0 = 0;
    req.u1 = offset;

#if DEBUG_DQM_IO
	printk("%s partition %d offset %08x\n", __func__, partition, offset);
#endif
    ret = do_rpc_io(&req);
    #if 0
    if(ret == 1)
    {
    	printk("Bad Block at %08x\n", (unsigned int)offset);
    }
    #endif

    return ret; 
}

static int vflash_block_markbad(int partition, int offset)
{
    ItcRpcMsg req;

    // Construct a request message
    memset((void *)&req, 0, sizeof(req));
    req.dev_func = DEV_FUNC(REMOTE_FLASH_DEVICE_ID, REMOTE_BLOCK_MARKBAD, partition, 0);
    req.xid = read_c0_count();
    req.u0 = 0;
    req.u1 = offset;

#if DEBUG_DQM_IO
	printk("%s partition %d offset %08x\n", __func__, partition, offset);
#endif
	return do_rpc_io(&req);
}

static int vflash_get_device_info(vflash_info_t *info)
{
    ItcRpcMsg req;

#if DEBUG_DQM_IO
    printk("-->%s info=%p\n", __func__, (void*)info);
#endif

    // Construct a request message
    memset((void *)&req, 0, sizeof(req));
    req.dev_func = DEV_FUNC(REMOTE_FLASH_DEVICE_ID, REMOTE_GET_DEV_INFO, LINUX_KERNEL_ROOTFS_PARTITION, sizeof(struct vflash_info_t));
    req.xid = read_c0_count();
    req.u0 = (uint32)info;
    req.u1 = sizeof(struct vflash_info_t);

    bcm_cache_inv((uint32)info, sizeof(struct vflash_info_t));

    return do_rpc_io(&req);
}
#endif

/*******************************************************************
 * mtd stuff
 ******************************************************************/
static int bcmmtd_erase(struct mtd_info *mtd, struct erase_info *instr)
{
    uint32_t rem;
    u32 addr,len;
    int partition, offset;
	int ret;
	BlockCache *bc;

    /* sanity czech */
    if (instr->addr + instr->len > mtd->size)
		return -EINVAL;

    div_u64_rem(instr->len, mtd->erasesize, &rem);

    if (rem)
		return -EINVAL;

    addr = instr->addr;
    len = instr->len;

#if DEBUG_VFLASH_MTD
    printk("-->%s addr = %08x len = %08x\n", __func__, (unsigned int)addr, (unsigned int)len);
#endif
    /* whole-chip erase is not supported */
    if (len == mtd->size) 
    {
        instr->state = MTD_ERASE_FAILED;
        return -EIO;
    }
    else 
    { 
        while (len) 
        {
			bc = find_block_cache(addr);
			if (bc)
			{
				invalidate_block_cache(bc);
				release_block_cache(bc);
			}
			partition = get_flash_partition(addr);
			offset = addr - flash_partition_ptr[partition].offset;
			ret = vflash_erase_block(partition, offset, mtd->erasesize);
			if (ret < 0)
				return ret;
    	    addr += mtd->erasesize;
			len -= mtd->erasesize;
		}
    }

    instr->state = MTD_ERASE_DONE;
    mtd_erase_callback(instr);

    return 0;
}

static int bcmmtd_read(struct mtd_info *mtd, loff_t addr, size_t len,
					   size_t *retlen, u_char *buf)
{
	BlockCache *bc;
    int partition, offset, ret, i;
#if defined(CONFIG_MTD_BRCMNAND)
    size_t rd_len;
    int left;
    u_char *ptr;
#endif
#if DEBUG_VFLASH_MTD
    printk("-->%s, offset=%0llx, len=%08x, buf=%08x \n", __func__, addr, len, (unsigned int)buf);
#endif

    if (retlen)
		*retlen = 0;

    if (addr + len > mtd->size) {
		printk("%s ERROR: trying to read beyond flash size %ld\n", __func__, addr+len);
		return -EINVAL;
	}

    partition = get_flash_partition(addr);
    if (partition < 0)
		return -ENODEV;
	
	bc = find_block_cache(addr);
	if (likely(bc && ((addr + len) <= bc->end_addr))) {
		/* most likely: we have a cached buffer */
		offset = bc->start_addr - flash_partition_ptr[partition].offset;
		i = addr - bc->start_addr;
		memcpy(buf, bc->buf + i, len);
		release_block_cache(bc);

		/* set return length */
		if (retlen)
			*retlen = len;
#if defined(CONFIG_MTD_BRCMNAND)
		ret = check_ecc_stat(mtd, bc->ecc_stat_buf, i, len);
		return ret;
#else
		return 0;
#endif
	}

	if (!bc) {
#if defined(CONFIG_MTD_BRCMNAND)
		bc = new_block_cache(addr, vflash_fetch_size);
#else
		bc = new_block_cache(addr, FLASH_ERASE_SIZE);
#endif
	}
	if (likely(bc && ((addr + len) <= bc->end_addr))) {
		/* we need to read a new cached buffer */
		offset = bc->start_addr - flash_partition_ptr[partition].offset;
#if defined(CONFIG_MTD_BRCMNAND)
		/* pass the data and ecc buffer pointers */
		bc->buf_array->data_buf = (uint32)bc->buf;
		bc->buf_array->ecc_stat_buf = (uint32)bc->ecc_stat_buf;

		ret = vflash_read_buf(partition, offset, (u_char *)bc->buf_array, bc->end_addr - bc->start_addr);
#else
		ret = vflash_read_buf(partition, offset, bc->buf, bc->end_addr - bc->start_addr);
#endif
		if (ret < 0) {
			release_block_cache(bc);
			return ret;
		}
		i = addr - bc->start_addr;
		memcpy(buf, bc->buf + i, len);
		release_block_cache(bc);
#if defined(CONFIG_MTD_BRCMNAND)
		ret = check_ecc_stat(mtd, bc->ecc_stat_buf, i, len);
#endif
	}
	else {
#if !defined(CONFIG_MTD_BRCMNAND)
		/* this unlikely: the buffer size spans multiple erase blocks */
			release_block_cache(bc);
			offset = addr - flash_partition_ptr[partition].offset;
			ret = vflash_read_buf(partition, offset, buf, len);

			if (ret < 0)
				return ret;
#else
printk("cache miss (m eb) -->\n");
		if(bc) {
			left =len;
			offset = addr - flash_partition_ptr[partition].offset;
			ptr = buf;
			memset(buf, 0, len);
			    do {
			    	if((offset%(vflash_fetch_size)) != 0) {
				    rd_len = vflash_fetch_size - (offset%(vflash_fetch_size));
				}
				else if(left > vflash_fetch_size) {
				    rd_len = vflash_fetch_size;
				}
				else {
				    rd_len = left; 
				}

#if defined(CONFIG_MTD_BRCMNAND)
				/* pass the data and ecc buffer pointers */
				bc->buf_array->data_buf = (uint32)bc->buf;
				bc->buf_array->ecc_stat_buf = (uint32)bc->ecc_stat_buf;
				ret = vflash_read_buf(partition, offset, (u_char *)bc->buf_array, rd_len);
#else
				ret = vflash_read_buf(partition, offset, bc->buf, rd_len);
#endif
				if (ret < 0)
				{
				    return ret;
				}
#if DEBUG_VFLASH_MTD
				printk("left = %08x rd_len = %08x ptr=%p\n", (unsigned int)left, (unsigned int)rd_len, ptr);
#endif
				memcpy(ptr, bc->buf, rd_len);
#if defined(CONFIG_MTD_BRCMNAND)
				if(check_ecc_stat(mtd, bc->ecc_stat_buf, 0, rd_len))
					ret = -EUCLEAN;
#endif
				left -= rd_len;
				offset += rd_len;
				ptr += rd_len;

			        } while(left > 0);
			
			release_block_cache(bc);
			if (ret < 0) {
				printk("vflash reading error!\n");
				return ret;
			}
		}
		else {
			printk("No bc buffer available!\n");
			return -1;
		}
#endif
	}
    /* set return length */
    if (retlen)
		*retlen = len;
		
#if defined(CONFIG_MTD_BRCMNAND)
    return (ret);
#else
    return 0;
#endif
}

static int bcmmtd_write(struct mtd_info *mtd, loff_t addr, size_t len,
						size_t *retlen, const u_char *buf)
{
    BlockCache *bc;
    int partition, offset, ret;
#if defined(CONFIG_MTD_BRCMNAND)
    int left = len; 
    size_t wr_len;
#endif

    if (retlen)
		*retlen = 0;

    /* sanity czech */
    if (addr + len > mtd->size) {
		printk("%s ERROR: trying to write a beyond flash size %ld\n", __func__, addr+len);
		return -EINVAL;
	}

	bc = find_block_cache(addr);
	if (bc)
	{
		invalidate_block_cache(bc);
		release_block_cache(bc);
	}
    partition = get_flash_partition(addr);
    offset = addr - flash_partition_ptr[partition].offset;
    
#if !defined(CONFIG_MTD_BRCMNAND)
        ret = vflash_write_buf(partition, offset, buf, len);
#else
    if(down_interruptible(&wr_buf_sem))
    	return -ERESTARTSYS;
#if DEBUG_VFLASH_MTD
    printk("-->%s, offset=%0llx, len=%08x\n", __func__, addr, len);
#endif
    do {
	if(left > vflash_info->erase_size) {
            memcpy((void *)wr_buf, (void *)buf, vflash_info->erase_size);
	    left -= vflash_info->erase_size;
	    wr_len = vflash_info->erase_size;
	}
	else {
            memcpy((void *)wr_buf, (void *)buf, left);
	    wr_len = left; 
	    left = 0;
	}

        ret = vflash_write_buf(partition, offset, wr_buf, wr_len);
	offset += wr_len;

	if (ret < 0)
	{
            up(&wr_buf_sem);
	    return ret;
	}
    } while(left > 0);

    up(&wr_buf_sem);
#endif
	
    /* set return length */
    if (retlen)
		*retlen = len;

    return 0;
    
    return 0;
}

static int get_flash_partition(loff_t address)
{
    int i;

    for (i = 0; i < NUM_FLASH_PARTITIONS; i++)
		if ((address >= vflash_mtd_parts[i].offset) &&
			(address < vflash_mtd_parts[i].offset + vflash_mtd_parts[i].size))
			return i;

    return -1;
}

#if defined(CONFIG_MTD_BRCMNAND)
static int bcmmtd_reboot_cb(struct notifier_block *nb, unsigned long val, void *v)
{
	struct mtd_info *mtd;

	mtd = container_of(nb, struct mtd_info, reboot_notifier);
	return NOTIFY_DONE;
}

static int bcmmtd_suspend(struct mtd_info *mtd)
{
	printk("-->%s \n", __func__);
	return 0;
}

static void bcmmtd_resume(struct mtd_info *mtd)
{
	printk("-->%s \n", __func__);
}

static int bcmmtd_block_isbad(struct mtd_info *mtd, loff_t ofs)
{

    	int partition, offset;
	int ret;
//	printk("-->%s ofs=%0llx\n", __func__,  ofs);
	/* Check for invalid offset */
	if (ofs > mtd->size) {
		return -EINVAL;
	}	
        partition = get_flash_partition(ofs);
	offset = ofs;
	ret = vflash_block_isbad(partition, offset);
	return ret;
}

static int bcmmtd_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
    	int partition, offset;
#if DEBUG_VFLASH_MTD
//	printk("-->%s ofs=%0llx\n", __func__,  ofs);
#endif
        partition = get_flash_partition(ofs);
	offset = ofs;
	return vflash_block_markbad(partition, offset);
}


static int bcmmtd_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops)
{
#if DEBUG_VFLASH_MTD
	printk("-->%s, offset=%0llx, len=%08x\n", __func__,  to, (int) ops->len);
#endif
	
	if (ops->datbuf && ((to + ops->len) > mtd->size)) 
	{
		printk("Attempt to write beyond end of device\n");		
		return -EINVAL;
	}

	if(!ops->datbuf) {
#if DEBUG_VFLASH_MTD
		printk("oob only write, mode = %d\n", ops->mode);
#endif
	}
	else {
#if DEBUG_VFLASH_MTD
		printk("oob + page write not supported yet!\n");
#endif
	}

	return 0;
}

static int bcmmtd_read_oob(struct mtd_info *mtd, loff_t from,
			 struct mtd_oob_ops *ops)
{
#if DEBUG_VFLASH_MTD
	printk("-->%s, offset=%lx len=%x, databuf=%p\n", __func__, 
		(unsigned long)from, (unsigned)ops->len, ops->datbuf);
#endif

	if (ops->datbuf) {
#if DEBUG_VFLASH_MTD
		printk("oob only read!\n");
#endif
	}
	else {
#if DEBUG_VFLASH_MTD
		printk("oob + page read not supported yet!\n");
#endif
	}

	return 0;
}
#endif

static int init_mtd(void)
{
    int	nr_parts = 0;
    uint32 flash_size = 0;
#if defined(CONFIG_MTD_BRCMNAND)
    uint32 nand_start;
#endif

    /* 
     * Look for flash map signature stored by bootloader in RAM
     * If it exists, then we can parse the flash map passed in
     * by bootloader to figure out the partitions.
     */
    if (*((unsigned int *)(FLASH_MAP_RAM_OFFSET)) == FLASH_MAP_SIGNATURE)
    {
		int i;
		flash_partition_ptr = (struct flash_partition_t *)
			kzalloc(sizeof(struct flash_partition_t) * NUM_FLASH_PARTITIONS, GFP_KERNEL);
		
		if (!flash_partition_ptr)
			return -ENOMEM;
		memcpy((void *)flash_partition_ptr,
			   (void *)(FLASH_MAP_RAM_OFFSET+4),
			   sizeof(struct flash_partition_t) * NUM_FLASH_PARTITIONS);

		nr_parts = NUM_FLASH_PARTITIONS;
        vflash_mtd_parts = kzalloc(sizeof(struct mtd_partition) * nr_parts, GFP_KERNEL);
        if (!vflash_mtd_parts)
			return -ENOMEM;

#if defined(CONFIG_MTD_BRCMNAND)
        vflash_nand_mtd_parts = kzalloc(sizeof(struct mtd_partition) * 2, GFP_KERNEL);
        if (!vflash_nand_mtd_parts)
			return -ENOMEM;


	/* Assuming app partition is always from device offset 0 */
	nand_start = flash_partition_ptr[LINUX_APPS_PARTITION].offset;
#endif

		for (i = 0; i < NUM_FLASH_PARTITIONS; i++)
		{
			if (i == LINUX_KERNEL_ROOTFS_PARTITION)
			{
            #if !defined(CONFIG_MTD_BRCMNAND)
				/* Get rootfs offset */
				int extra = get_part_offset() - flash_partition_ptr[i].offset;
				vflash_mtd_parts[i].offset = get_part_offset();
				vflash_mtd_parts[i].size = flash_partition_ptr[i].size - extra;
			#else
				vflash_mtd_parts[i].offset = flash_partition_ptr[i].offset;
				vflash_mtd_parts[i].size = flash_partition_ptr[i].size;
			#endif
			}
#if defined(CONFIG_BCM93383)
#if defined(CONFIG_MTD_BRCMNAND)
            else if ((i == LINUX_APPS_PARTITION) || (i == LINUX_ROOTFS_PARTITION))
#else
            else if (i == LINUX_APPS_PARTITION)
#endif
			{
#if defined(CONFIG_MTD_BRCMNAND)
				vflash_mtd_parts[i].offset = flash_partition_ptr[i].offset;
				vflash_mtd_parts[i].size = flash_partition_ptr[i].size;
#else
				vflash_mtd_parts[i].offset =
					flash_partition_ptr[i].offset + FLASH_ERASE_SIZE;
				vflash_mtd_parts[i].size =
					flash_partition_ptr[i].size - FLASH_ERASE_SIZE;
#endif
            }
#endif
            else
			{
				vflash_mtd_parts[i].offset = flash_partition_ptr[i].offset;
				vflash_mtd_parts[i].size = flash_partition_ptr[i].size;
			}
			vflash_mtd_parts[i].name = kzalloc(11, GFP_KERNEL);
			strncpy(vflash_mtd_parts[i].name, flash_partition_ptr[i].name, 10);
			vflash_mtd_parts[i].name[10] = 0;
			printk("MTD%d %-10s offset %08lx size %08lx\n", i,
				   vflash_mtd_parts[i].name,
				   (uint32)vflash_mtd_parts[i].offset,
				   (uint32)vflash_mtd_parts[i].size);
#if defined(CONFIG_MTD_BRCMNAND)
			vflash_mtd_parts[i].offset -= nand_start; 
			flash_partition_ptr[i].offset -= nand_start;
#endif
#if defined(CONFIG_BCM93383)
#if !defined(CONFIG_MTD_BRCMNAND)
			/* Everything but the apps partition is non-writeable */
			if ((i != LINUX_APPS_PARTITION) &&
				(i != LINUX_DHTML_PARTITION) )
				vflash_mtd_parts[i].mask_flags = MTD_WRITEABLE;
#endif
#endif
			flash_size += flash_partition_ptr[i].size;
		}
    }
    else
    {
		printk("NO FLASH MAP FOUND!!!\n");
		return 0;
    }

#if defined(CONFIG_MTD_BRCMNAND)
    vflash_info = (vflash_info_t *)kzalloc(sizeof(struct vflash_info_t), GFP_KERNEL);
    if (!vflash_info) {
    	printk("%s : memory allocation failure!\n",__func__);
	return -ENOMEM;
    }

    /* Get device information */
    vflash_get_device_info(vflash_info);

#if 0
    printk("type = %08x\n", vflash_info->type);
    printk("size_l = %08x\n", vflash_info->size_l);
    printk("size_h = %08x\n", vflash_info->size_h);
    printk("erase_size = %08x\n", vflash_info->erase_size);
    printk("page_size = %08x\n", vflash_info->page_size);
#endif

    /* For write buffer */
    wr_buf = (unsigned char *)kmalloc(vflash_info->erase_size, GFP_KERNEL);
    if (wr_buf == NULL)
    {
	printk("%s: out of memory\n", __func__);
    }

    /* Calculate fetch size */
    if(vflash_info->erase_size > vflash_fetch_size) {
    	vflash_fetch_size = vflash_info->erase_size;
    }
    ecc_stat_buf_len = vflash_fetch_size/NAND_CACHE_SIZE + 4;
    printk("vflash_fetch_size = %08x\n", vflash_fetch_size);
    printk("ecc_stat_buf_len = %08x\n", ecc_stat_buf_len);
#endif

//#if defined(CONFIG_BCM_RAMDISK)
#if 0
    return 0;
#else
    /* Set up MTD INFO struct */
    vflash_mtd = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
    if (!vflash_mtd)
    {
		return -ENODEV;
    }
#if defined(CONFIG_MTD_BRCMNAND)
    vflash_mtd->name = bcmmtd_name;
    vflash_mtd->type = MTD_NANDFLASH;
    vflash_mtd->size = vflash_info->size_l; 
    vflash_mtd->erasesize = vflash_info->erase_size; 
    vflash_mtd->writesize = vflash_info->page_size; 
    vflash_mtd->flags = MTD_CAP_NANDFLASH;

    vflash_mtd->read = bcmmtd_read; 
    vflash_mtd->write = bcmmtd_write;
    vflash_mtd->erase = bcmmtd_erase;
    vflash_mtd->read_oob = bcmmtd_read_oob;
    vflash_mtd->write_oob = bcmmtd_write_oob;
    vflash_mtd->block_isbad = bcmmtd_block_isbad;
    vflash_mtd->block_markbad = bcmmtd_block_markbad;

    vflash_mtd->oobsize = vflash_mtd->writesize >> 5; 
    vflash_mtd->ecclayout = &brcmnand_oob_bch4_2k;
    vflash_mtd->point = NULL;
    vflash_mtd->unpoint = NULL;
    vflash_mtd->writev = NULL;
    vflash_mtd->sync = NULL;
    vflash_mtd->lock = NULL; 
    vflash_mtd->unlock = NULL;
    vflash_mtd->suspend = bcmmtd_suspend;
    vflash_mtd->resume = bcmmtd_resume;
    /*
    vflash_mtd->reboot_notifier.notifier_call = bcmmtd_reboot_cb;
    register_reboot_notifier(&vflash_mtd->reboot_notifier);
    */
#else
    vflash_mtd->name = bcmmtd_name;
    vflash_mtd->type = MTD_NORFLASH;
    vflash_mtd->size = flash_size;
    vflash_mtd->read = bcmmtd_read; 
    vflash_mtd->write = bcmmtd_write;
    vflash_mtd->erase = bcmmtd_erase;
    vflash_mtd->erasesize = FLASH_ERASE_SIZE;
    vflash_mtd->writesize = 1;
    vflash_mtd->flags = MTD_CAP_NORFLASH;
#endif

#if defined(CONFIG_MTD_BRCMNAND)
	/* Only create partitions for root file system and apps */
	nr_parts = 2;
	//return add_mtd_partitions(vflash_mtd, &vflash_mtd_parts[LINUX_KERNEL_ROOTFS_PARTITION], nr_parts);
	vflash_nand_mtd_parts[0].name = kzalloc(11, GFP_KERNEL);
	strncpy(vflash_nand_mtd_parts[0].name, vflash_mtd_parts[LINUX_ROOTFS_PARTITION].name, 10);
	vflash_nand_mtd_parts[0].name[10] = 0;
	vflash_nand_mtd_parts[0].offset = vflash_mtd_parts[LINUX_ROOTFS_PARTITION].offset + vflash_info->erase_size;
	vflash_nand_mtd_parts[0].size = vflash_mtd_parts[LINUX_ROOTFS_PARTITION].size - vflash_info->erase_size - vflash_fetch_size;

	vflash_nand_mtd_parts[1].name = kzalloc(11, GFP_KERNEL);
	strncpy(vflash_nand_mtd_parts[1].name, vflash_mtd_parts[LINUX_APPS_PARTITION].name, 10);
	vflash_nand_mtd_parts[1].name[10] = 0;
	vflash_nand_mtd_parts[1].offset = vflash_mtd_parts[LINUX_APPS_PARTITION].offset + vflash_info->erase_size;
	vflash_nand_mtd_parts[1].size = vflash_mtd_parts[LINUX_APPS_PARTITION].size - vflash_info->erase_size - vflash_fetch_size;

	return add_mtd_partitions(vflash_mtd, vflash_nand_mtd_parts, nr_parts);
#else

	return add_mtd_partitions(vflash_mtd, vflash_mtd_parts, nr_parts);
#endif
#endif
}

#if defined(CONFIG_BCM93383) && defined(CONFIG_BCM_LOT1)
static uint32 rcvItcMsg(void)
{
    return *((volatile uint32 *)ITC_INBOX_REG_ADDRESS);
}	

EXPORT_SYMBOL (rcvItcMsg);
#endif

void sendItcMsg(uint32 msg)
{
	unsigned long flags;
	uint32 c0reg;
	spin_lock_irqsave(&itc_msg_lock, flags);

#if defined(CONFIG_BCM93383)
        *((volatile uint32 *)ITC_OUTBOX_REG_ADDRESS) = msg;
#if defined(CONFIG_BCM_LOT1)
	c0reg = __read_32bit_c0_register($13, 0);
	__write_32bit_c0_register($13, 0, c0reg | (1 << 8));
#endif
#else
	__write_32bit_c0_register($31, 0, msg);
	c0reg = __read_32bit_c0_register($13, 0);
	__write_32bit_c0_register($13, 0, c0reg | (1 << 8));
#endif
	spin_unlock_irqrestore(&itc_msg_lock, flags);
}	

#if defined(CONFIG_BCM93383) && defined(CONFIG_BCM_LOT1)
EXPORT_SYMBOL (sendItcMsg);
#endif

/*******************************************************************
 * init_dqm
 ******************************************************************/
static int init_dqm(void)
{
#if defined(CONFIG_BCM93383)
	extern void bcmiop_init(void);
#endif
	BcmHostDqmCallBack cb;

#if defined(CONFIG_BCM93383) && !defined(CONFIG_BCM_LOT1)
	bcmiop_init();
#endif
	cb.fn = (_isr_callback_)vflash_isr;
	cb.context = (void *)&dqmchannel;

	dqmchannel.dqmRxHandle =
		getHostDqmHalIfHandle(DQM_NO_Q_ASSIGNED, VFLASH_DQM_RX_Q, VFLASH_DQM_RX_PROC, &cb);
	dqmchannel.dqmTxHandle =
		getHostDqmHalIfHandle(VFLASH_DQM_TX_Q, DQM_NO_Q_ASSIGNED, VFLASH_DQM_TX_PROC, NULL);

	hostDqmEnableDqmQInt(dqmchannel.dqmRxHandle);

	/* magic handshake */
	
	bcm_cache_wback_inv((uint32)dist_banner, (uint32)sizeof(dist_banner));
	sendItcMsg((uint32)dist_banner);
	printk("Sent banner %08lx\n",(uint32)dist_banner);

	return dqmchannel.dqmRxHandle && dqmchannel.dqmTxHandle ? 0 : -1;
}


/***************************************************************
 * block cache
 **************************************************************/

#define MAX_BLOCK_CACHES	5
static BlockCache blockcache[MAX_BLOCK_CACHES];
static unsigned block_cache_lru_index = 0;	/* last recently used */
static DEFINE_SPINLOCK(block_cache_lock);

static void	init_block_cache(void)
{
	int i;
	memset(blockcache, 0, sizeof(blockcache));
	for (i = 0; i < MAX_BLOCK_CACHES; i++)
		sema_init(&blockcache[i].sema, 1);
}

static BlockCache *find_block_cache(unsigned long addr)
{
	int i, locked;
	unsigned long flags;
	BlockCache *bc;

	spin_lock_irqsave(&block_cache_lock, flags);
	
	bc = &blockcache[block_cache_lru_index];
	if (likely((bc->start_addr <= addr && addr < bc->end_addr)))
	{
		locked = (down_trylock(&bc->sema) == 0);
		spin_unlock_irqrestore(&block_cache_lock, flags);
		if (likely(locked))
		{
#if DEBUG_VFLASH_MTD
				printk("%s - found block cache for %08x!\n", __func__, (unsigned int)bc->start_addr);
#endif
			return bc;
			}
		else
		{	/* if its in flight wait for it */
			down(&bc->sema);
			/* reverify */
			if (bc->start_addr <= addr && addr < bc->end_addr)
			{
#if DEBUG_VFLASH_MTD
				printk("%s - found block cache for %08x!\n", __func__, (unsigned int)bc->start_addr);
#endif
				return bc;
				}
		}
		/* failure due to contention so try again */
		spin_lock_irqsave(&block_cache_lock, flags);
	}
	for (i = 0; i < MAX_BLOCK_CACHES; i++)
	{
		bc = &blockcache[i];
		if (bc->start_addr <= addr && addr < bc->end_addr)
		{
			locked = (down_trylock(&bc->sema) == 0);
			if (locked) {
				block_cache_lru_index = i;
				spin_unlock_irqrestore(&block_cache_lock, flags);
#if DEBUG_VFLASH_MTD
				printk("%s - found block cache for %08x!\n", __func__, (unsigned int)bc->start_addr);
#endif
				return bc;
			}
		}
	}
	spin_unlock_irqrestore(&block_cache_lock, flags);
	return NULL;	
}

static BlockCache *new_block_cache(unsigned long addr, unsigned long size)
{
	int i, locked;
	unsigned long flags;
	BlockCache *bc;
	
	spin_lock_irqsave(&block_cache_lock, flags);

	i = block_cache_lru_index + 1;
	if (i == MAX_BLOCK_CACHES)
		i = 0;
	bc = &blockcache[i];
	locked = (down_trylock(&bc->sema) == 0);
	if (unlikely(!locked))
	{
		for (i = 0; i < MAX_BLOCK_CACHES; i++)
		{
			bc = &blockcache[i];
			locked = (down_trylock(&bc->sema) == 0);
			if (locked)
				break;
		}
		if (i == MAX_BLOCK_CACHES)
		{	/* everything is in use */
			i = block_cache_lru_index + 1;
			if (i == MAX_BLOCK_CACHES)
				i = 0;
			bc = &blockcache[i];
			spin_unlock_irqrestore(&block_cache_lock, flags);
			down(&bc->sema);
			spin_lock_irqsave(&block_cache_lock, flags);
		}
	}	
	if (bc->buf)
	{
		if ((bc->end_addr - bc->start_addr) != size)
		{
			char *buf = krealloc(bc->buf, size, GFP_KERNEL);
			if (buf == NULL)
			{
				printk("%s: out of memory\n", __func__);
				spin_unlock_irqrestore(&block_cache_lock, flags);
				return NULL;
			}
			bc->buf = buf;
		}
	}
	else
	{
		bc->buf = kmalloc(size, GFP_KERNEL);
		if (bc->buf == NULL)
		{
			printk("%s: out of memory\n", __func__);
			spin_unlock_irqrestore(&block_cache_lock, flags);
			return NULL;
		}

#if defined(CONFIG_MTD_BRCMNAND)
		bc->buf_array = (BufArray *)kmalloc((ecc_stat_buf_len+sizeof(BufArray)), GFP_KERNEL);
		if (bc->buf_array == NULL)
		{
			kfree(bc->buf);
			printk("%s: out of memory\n", __func__);
			spin_unlock_irqrestore(&block_cache_lock, flags);
			return NULL;
		}
		bc->ecc_stat_buf = (u_char *)((uint32)bc->buf_array + sizeof(BufArray));
#endif
	}
	bc->start_addr = addr & ~(size - 1);
	bc->end_addr = bc->start_addr + size;
	spin_unlock_irqrestore(&block_cache_lock, flags);
	return bc;
}

static void release_block_cache(BlockCache *bc)
{
	up(&bc->sema);
}

static void invalidate_block_cache(BlockCache *bc)
{
	kfree(bc->buf);
	bc->buf = 0;
	bc->start_addr = bc->end_addr = 0;
}

/***************************************************************
 * init all this stuff
 **************************************************************/

#if defined(CONFIG_MTD_BRCMNAND)
int __init bcmVFlash_init(void)
#else
static int __init bcmVFlash_init(void)
#endif
{
	int ret = init_dqm();
	if (ret < 0) {
		printk("bcmVFlash_init catastrophic ERROR in init_dqm\n");
		return ret;
	}
	init_block_cache();
	ret = init_mtd();
	if (ret < 0) {
		printk("bcmVFlash_init catastrophic ERROR in init_mtd\n");
		return ret;
	}
	return 0;
}

#if !defined(CONFIG_MTD_BRCMNAND) && !defined(CONFIG_BCM_RAMDISK)
module_init(bcmVFlash_init);
#endif


EXPORT_SYMBOL (do_rpc_io);
#if defined(CONFIG_BCM93383) && defined(CONFIG_BCM_LOT1)
EXPORT_SYMBOL (disable_rpc_io_timeout);
#endif
