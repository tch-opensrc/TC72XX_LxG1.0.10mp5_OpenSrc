/*
<:copyright-gpl

 Copyright 2009 Broadcom Corp. All Rights Reserved.

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
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <net/ip.h>
#include <net/dsfield.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include "skb_defines.h"

#if 0

#define DEBUG_TOS(args) printk args
#define DEBUG_TOS1(args) printk args

#else

#define DEBUG_TOS(args)   
#define DEBUG_TOS1(args)  

#endif

#define DUMP_TUPLE_IPV4(tp)						\
	 DEBUG_TOS(("tuple %p: %u %u.%u.%u.%u:%hu -> %u.%u.%u.%u:%hu\n",	\
				 (tp), (tp)->dst.protonum,				\
				 NIPQUAD((tp)->src.u3.ip), ntohs((tp)->src.u.all),		\
				 NIPQUAD((tp)->dst.u3.ip), ntohs((tp)->dst.u.all)))

#define DYNTOS_DSCP_MASK 0xfc  /* 11111100 */
#define DYNTOS_DSCP_SHIFT   2
#define DYNTOS_DSCP_MAX  0x3f  /* 00111111 */

#define DYNTOS_INITIALIZING   0
#define DYNTOS_INHERITED 		1
#define DYNTOS_SKIP 				2

#define DYNTOS_LAN2WAN_DEFAULT_DSCP 0
#define DYNTOS_WAN2LAN_DEFAULT_DSCP 0

#define DYNTOS_PROC_TRANSTBL_FILENAME "net/netfilter/nf_dtos_w2ldscp_transtbl"
#define TOS_MAPPINGTABLE_MAX_SIZE 64
#define DYNTOS_MAX_PROC_WRITE_BUFLEN 64

static char dyntos_proc_buffer[DYNTOS_MAX_PROC_WRITE_BUFLEN];
static struct proc_dir_entry * dyntos_proc_file = NULL;

/* structure used to maintain dscp transmarking table entries*/

struct dscpMapping {
	 uint8_t orig;
	 uint8_t new;
};

/* dscp transmarking table entries*/
struct transMarkTable {
	 uint16_t size;
	 uint16_t used;
	 struct dscpMapping *dscp;
};

static struct transMarkTable transMarkTbl;

/*finds the dscp mapping and returns new dscp value
 * returns DYNTOS_WAN2LAN_DEFAULT_DSCP if no match */ 

uint8_t getDscpfromTransTbl(uint8_t orig)
{
	 int i;
	 for(i=0; i < transMarkTbl.size; i++)
	 {
			if(transMarkTbl.dscp[i].orig == orig)
			{
				 return transMarkTbl.dscp[i].new;
			}

	 }

	 return DYNTOS_WAN2LAN_DEFAULT_DSCP;
}

/* Adds a new DSCP mapping,(over writes the existing mapping for 
 * origDscp value if present)
 * an entry is free if both orig and new are 0 */
int  addDscpinTransTbl(uint8_t origDscp, uint8_t newDscp)
{
	 int i;

	 /*replace entry */
	 for(i=0; i < transMarkTbl.size; i++)
	 {
			if(transMarkTbl.dscp[i].orig == origDscp)
			{

				 if((transMarkTbl.dscp[i].orig == 0) && (transMarkTbl.dscp[i].new == 0 ) &&(newDscp != 0 )) 
						transMarkTbl.used++;/* new entry special case as intially entries are set to 0*/

				 if((transMarkTbl.dscp[i].orig == 0) && (transMarkTbl.dscp[i].new != 0 ) &&(newDscp == 0 )) 
						transMarkTbl.used--;/*  remove entry special case as intially entries are set to 0*/

				 transMarkTbl.dscp[i].new = newDscp;


				 return 0; 
			}
	 }

	 /*new entry */
	 for(i=0; i < transMarkTbl.size; i++)
	 {
			if((transMarkTbl.dscp[i].orig == 0) && (transMarkTbl.dscp[i].new == 0 ))
			{
				 transMarkTbl.dscp[i].orig = origDscp;
				 transMarkTbl.dscp[i].new = newDscp;
				 transMarkTbl.used++;
				 return 0; 
			}
	 }

	 /*table full */
	 printk(KERN_ERR "%s:Transmark Table is Full\n",__FUNCTION__);
	 return -1; 
}

/* delete a DSCP mapping from trans table */
int  delDscpinTransTbl(uint8_t origDscp)
{
	 int i;

	 for(i=0; i < transMarkTbl.size; i++)
	 {
			if((transMarkTbl.dscp[i].orig == origDscp) )
			{
				 transMarkTbl.dscp[i].orig = 0;
				 transMarkTbl.dscp[i].new = 0;
				 transMarkTbl.used--;
				 return 0; 
			}
	 }

	 printk(KERN_ERR "%s: Entry not found in Transmark Table\n",__FUNCTION__);
	 return -1; 
}

/*for setting interface's IFF_WANDEV flag ;LAB testing purpose */
int setWanIfFlag(char *name)
{
	 struct net_device *dev = NULL;         

	 dev = dev_get_by_name(name);  
	 if(dev){
			printk(KERN_INFO "setting %s IFF_WANDEV flag\n",name);
			dev->priv_flags  |= IFF_WANDEV;     
			return 0;
	 } else {
			printk(KERN_ERR "interface %s not found\n",name);
			return -1;
	 }

}

/* Entry point into dyntos module at pre-routing
 * this  function is the core engine of this module
 * */
static unsigned int nf_dyntos_in(unsigned int hooknum,
			struct sk_buff **pskb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn)(struct sk_buff *))
{
	 struct nf_conn *ct;
	 enum ip_conntrack_info ctinfo;
	 u_int8_t pktDscp; 

	 ct = nf_ct_get(*pskb, &ctinfo);

	 DEBUG_TOS1((" %s: seen packet \n",__FUNCTION__));

	 if(!ct) {
			DEBUG_TOS1((KERN_INFO " %s: seen packet with out flow\n",__FUNCTION__));
			return NF_ACCEPT;
	 }

	 if(ct->dyntos.status == DYNTOS_INHERITED) {
			DEBUG_TOS1((KERN_INFO "%s: changing tos in pkt to %x \n",__FUNCTION__,
                ct->dyntos.dscp[CTINFO2DIR(ctinfo)]));

			if (!skb_make_writable(pskb, sizeof(struct iphdr)))
				 return NF_DROP;

			ipv4_change_dsfield((*pskb)->nh.iph, (__u8)(~DYNTOS_DSCP_MASK),
						ct->dyntos.dscp[CTINFO2DIR(ctinfo)] << DYNTOS_DSCP_SHIFT);

	 } else if(ct->dyntos.status == DYNTOS_INITIALIZING) {

			if (ct == &nf_conntrack_untracked) {

				 ct->dyntos.status = DYNTOS_SKIP;
				 DEBUG_TOS((KERN_INFO "skipping tos mangling for untracked flow\n"));
				 return NF_ACCEPT;
			}

			/*for now we change TOS only for TCP/UDP */
			/*if(((*pskb)->nh.iph->protocol != IPPROTO_UDP) || ((*pskb)->nh.iph->protocol != IPPROTO_TCP)){
				 ct->dyntos.status = DYNTOS_SKIP;
				 return NF_ACCEPT;
			}*/
         /*TODO: should we skip broadcast packets ?? */


			pktDscp = ipv4_get_dsfield((*pskb)->nh.iph) >> DYNTOS_DSCP_SHIFT;

			if(!SKBMARK_GET_IFFWAN_MARK((*pskb)->mark)) {
				 /* LAN -> WAN packet */

				 DEBUG_TOS1((" %s: initializing case lan->wan packet \n",__FUNCTION__));

				 if(pktDscp != DYNTOS_LAN2WAN_DEFAULT_DSCP) {

						if (!skb_make_writable(pskb, sizeof(struct iphdr)))
							 return NF_DROP;

						ipv4_change_dsfield((*pskb)->nh.iph, (__u8)(~DYNTOS_DSCP_MASK),
									DYNTOS_LAN2WAN_DEFAULT_DSCP << DYNTOS_DSCP_SHIFT);
				 }

			} else {
				 /* WAN -> LAN packet */

				 DEBUG_TOS1(("%s: initializing case wan->lan packet \n",__FUNCTION__));
				 if (!skb_make_writable(pskb, sizeof(struct iphdr)))
						return NF_DROP;

				 /* inherit tos from packet */
				 if (CTINFO2DIR(ctinfo) == IP_CT_DIR_ORIGINAL) {
						/*connection intiated from WAN */
						ct->dyntos.dscp[IP_CT_DIR_REPLY] = pktDscp;
						ct->dyntos.dscp[IP_CT_DIR_ORIGINAL] = getDscpfromTransTbl(pktDscp);

						ipv4_change_dsfield((*pskb)->nh.iph, (__u8)(~DYNTOS_DSCP_MASK),
									ct->dyntos.dscp[IP_CT_DIR_ORIGINAL] << DYNTOS_DSCP_SHIFT);
				 } else {
						/*connection intiated from LAN or LOCAL*/
						ct->dyntos.dscp[IP_CT_DIR_ORIGINAL] = pktDscp;
						ct->dyntos.dscp[IP_CT_DIR_REPLY] = getDscpfromTransTbl(pktDscp);

						ipv4_change_dsfield((*pskb)->nh.iph, (__u8)(~DYNTOS_DSCP_MASK),
									ct->dyntos.dscp[IP_CT_DIR_REPLY] << DYNTOS_DSCP_SHIFT);
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
						/* Notify associated flows in flow cache, so they will relearn with
						 *  new tos values this is needed only for UDP, as TCP flows 
						 *  are created only when packet are seen from both directions 	 
						 */

						if((*pskb)->nh.iph->protocol == IPPROTO_UDP){
							 blog_notify(NULL, ct, BLOG_EVENT_DTOS);  
							 DEBUG_TOS(("%s:blog_notify:EVENT_DTOS for\n",__FUNCTION__));
							 DUMP_TUPLE_IPV4(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
						}
#endif
				 }

				 /*update tos status in nf_conn */
				 ct->dyntos.status = DYNTOS_INHERITED;

				 DEBUG_TOS((KERN_INFO "dynamic tos values(%X, %X) inherited forflow\n",
									ct->dyntos.dscp[IP_CT_DIR_ORIGINAL],
									ct->dyntos.dscp[IP_CT_DIR_REPLY]));
				 DUMP_TUPLE_IPV4(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
			}

	 } else if(ct->dyntos.status == DYNTOS_SKIP){
			/*handle untracked connections */

	 } else {

			printk(KERN_WARNING " %s :dyntos unknown status(%d) for flow\n",
						__FUNCTION__, ct->dyntos.status);
			DUMP_TUPLE_IPV4(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	 }

	 return NF_ACCEPT;
}

static unsigned int nf_dyntos_local(unsigned int hooknum,
			struct sk_buff **pskb,
			const struct net_device *in,
			const struct net_device *out,
			int (*okfn)(struct sk_buff *))
{
	 struct nf_conn *ct;
	 enum ip_conntrack_info ctinfo;

	 /* root is playing with raw sockets. */
	 if ((*pskb)->len < sizeof(struct iphdr)
				 || (*pskb)->nh.iph->ihl * 4 < sizeof(struct iphdr)) {
			if (net_ratelimit())
				 DEBUG_TOS((KERN_INFO "nf_dyntos_local: happy cracking.\n"));
			return NF_ACCEPT;
	 }

	 ct = nf_ct_get(*pskb, &ctinfo);

	 if(!ct){
			DEBUG_TOS((KERN_INFO "%s: seen packet with out flow\n",__FUNCTION__));
			return NF_ACCEPT;
	 }

	 if(ct->dyntos.status == DYNTOS_INHERITED) {

			if (!skb_make_writable(pskb, sizeof(struct iphdr)))
				 return NF_DROP;

			/* LOCAL -> WAN packet */
			ipv4_change_dsfield((*pskb)->nh.iph, (__u8)(~DYNTOS_DSCP_MASK),
						ct->dyntos.dscp[CTINFO2DIR(ctinfo)] << DYNTOS_DSCP_SHIFT);

	 } else if(ct->dyntos.status == DYNTOS_INITIALIZING) {

			if (test_bit(IPS_SEEN_REPLY_BIT, &ct->status)) {
				 /*this happens only with LAN <-> LOCAL so just skip */ 
				 ct->dyntos.status = DYNTOS_SKIP;
			}

	 }

	 return NF_ACCEPT;
}

static struct nf_hook_ops nf_dyntos_ops[] = {
	 {
			.hook		= nf_dyntos_in,
			.owner		= THIS_MODULE,
			.pf		= PF_INET,
			.hooknum	= NF_IP_PRE_ROUTING,
			.priority	= NF_IP_PRI_MANGLE - 10,/*pre routing do it before mangle table */
	 },
	 {
			.hook		= nf_dyntos_local,
			.owner		= THIS_MODULE,
			.pf		= PF_INET,
			.hooknum	= NF_IP_LOCAL_OUT,
			.priority	= NF_IP_PRI_MANGLE + 10,/*local out do it after mangle table */

	 },
};

/* proc interface functions for configuring/reading transmark table 
 * from userspace
 * */

static void *dyntos_seq_start(struct seq_file *seq, loff_t *pos)
{
	 if(*pos > transMarkTbl.size)
			return NULL;

	 return *pos ? pos : SEQ_START_TOKEN;
}

static void *dyntos_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	 ++(*pos);
	 if(*pos > transMarkTbl.size)
			return NULL;

	 return &transMarkTbl.dscp[(*pos)-1];
}

static void dyntos_seq_stop(struct seq_file *seq, void *v)
{
	 return;
}

static int dyntos_seq_show(struct seq_file *seq, void *v)
{
	 if (v == SEQ_START_TOKEN){
			seq_printf(seq,"WANTOS\t-->\tLANTOS Max num entries:%d,"
						"Current num Entries:%d\n",
						transMarkTbl.size, transMarkTbl.used);
	 } else {
			struct dscpMapping *tos = (struct dscpMapping *)v;
			if((tos->orig !=0) && (tos->new !=0))/*show only used entries*/
				 seq_printf(seq, "%02x\t   \t%02x\n",tos->orig,tos->new);
	 }
	 return 0;
}

static struct seq_operations dyntos_seq_ops = {
	 .start   =  dyntos_seq_start,
	 .next =  dyntos_seq_next,
	 .stop =  dyntos_seq_stop,
	 .show =  dyntos_seq_show,
};

int nf_dyntos_proc_open(struct inode *inode, struct file *file)
{
	 return seq_open(file, &dyntos_seq_ops);
}


static ssize_t nf_dyntos_proc_write(struct file *file, const char *buffer,
			size_t len, loff_t *offset)
{
	 uint8_t origDscp, newDscp;
	 char wanIfname[32];

	 if(len > DYNTOS_MAX_PROC_WRITE_BUFLEN)
	 {
			printk(KERN_ALERT "%s: User datalen > max kernel buffer len=%d\n",
						__FUNCTION__, len);
			return -EFAULT;
	 }

	 if ( copy_from_user(dyntos_proc_buffer, buffer, len) )
	 {
			printk(KERN_ALERT "%s copy_from_user failure.\n", __FUNCTION__ );
			//kfree( kbuffer );
			return -EFAULT;
	 }

	 DEBUG_TOS((KERN_INFO "Applying %u bytes configuration\n", len));

	 if(sscanf(dyntos_proc_buffer,"add %hhi %hhi",&origDscp, &newDscp)) {
			if(addDscpinTransTbl(origDscp,newDscp) < 0)
				 return -EFAULT;
	 } else if(sscanf(dyntos_proc_buffer,"delete %hhi",&origDscp)) {
			if(delDscpinTransTbl(origDscp) < 0)
				 return -EFAULT;
	 } else if(sscanf(dyntos_proc_buffer,"setwanif %s", wanIfname)) {
			if(setWanIfFlag(wanIfname) < 0)
				 return -EFAULT;
	 } else {
			printk(KERN_ALERT " unknown command/syntax in %s .\n", __FUNCTION__ );
			printk(KERN_ALERT "use 'add' or 'delete' commands Ex: \n");
			printk(KERN_ALERT "add origDscp newDscp >/proc/.../.. \n");
			printk(KERN_ALERT "delete origDscp >/proc/.../.. \n");
			return -EFAULT;
	 }	

	 return len;
}

static struct file_operations dyntos_proc_fops = {
	 .open    = nf_dyntos_proc_open,
	 .read    = seq_read,
	 .write   = nf_dyntos_proc_write,
	 .llseek  = seq_lseek,
	 .release = seq_release,
};

int nf_dyntos_proc_init(void)
{
	 dyntos_proc_file = create_proc_entry(DYNTOS_PROC_TRANSTBL_FILENAME, 0644, NULL);
	 if ( dyntos_proc_file == (struct proc_dir_entry *)NULL )
	 {
			printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
						DYNTOS_PROC_TRANSTBL_FILENAME);
			return -ENOMEM;
	 }

	 dyntos_proc_file->owner = THIS_MODULE;
	 dyntos_proc_file->proc_fops = &dyntos_proc_fops;
	 dyntos_proc_file->mode = S_IFREG | S_IRUGO | S_IWUSR;
	 dyntos_proc_file->uid = dyntos_proc_file->gid = 0;
	 dyntos_proc_file->size = 80;

	 printk(KERN_INFO "/proc/%s created\n", DYNTOS_PROC_TRANSTBL_FILENAME);

	 return 0; /* success */
}

void nf_dyntos_proc_fini(void)
{
	 remove_proc_entry(DYNTOS_PROC_TRANSTBL_FILENAME, NULL);
	 printk(KERN_INFO "/proc/%s removed\n", DYNTOS_PROC_TRANSTBL_FILENAME);
}

static int __init nf_dyntos_init(void)
{
	 int ret = 0;

	 need_conntrack();

	 transMarkTbl.size = TOS_MAPPINGTABLE_MAX_SIZE;
	 transMarkTbl.dscp = kmalloc((transMarkTbl.size * sizeof(struct dscpMapping)),
																		 GFP_KERNEL);
	 memset(transMarkTbl.dscp, 0, (transMarkTbl.size * sizeof(struct dscpMapping)));

	 ret = nf_register_hooks(nf_dyntos_ops,
				 ARRAY_SIZE(nf_dyntos_ops));
	 if (ret < 0) {
			printk("nf_dyntos: can't register hooks.\n");
			goto cleanup_tbl;
	 }
#if defined(CONFIG_PROC_FS) 
	 ret = nf_dyntos_proc_init();
	 if (ret < 0)
			goto cleanup_hooks;

	 return ret;

cleanup_hooks:
	 nf_unregister_hooks(nf_dyntos_ops, ARRAY_SIZE(nf_dyntos_ops));
#endif
cleanup_tbl:
	 return ret;
}

static void __exit nf_dyntos_fini(void)
{
#if defined(CONFIG_PROC_FS) 
	 nf_dyntos_proc_fini();
#endif
	 nf_unregister_hooks(nf_dyntos_ops, ARRAY_SIZE(nf_dyntos_ops));
}

MODULE_AUTHOR("broadcom.com");
MODULE_DESCRIPTION("DSCP Inheritance from WAN");
MODULE_LICENSE("GPL");
MODULE_ALIAS("nf_dyntos-" __stringify(AF_INET));
MODULE_ALIAS("nf_dyntos");

module_init(nf_dyntos_init);
module_exit(nf_dyntos_fini);
