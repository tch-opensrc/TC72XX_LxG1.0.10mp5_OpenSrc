/* Set TOS field in header to any value
 *
 * (C) 2000 by Matthew G. Marsh <mgm@paktronix.com>
 *
 * This software is distributed under GNU GPL v2, 1991
 * 
 * ipt_FTOS.c borrowed heavily from ipt_TOS.c  11/09/2000
 * 
 * Updated 3/4/02 - added in Rusty's skb_clone fix 
 *                - added MODULE Political License
 *                - redid checksum routine somewhat
*/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/checksum.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_FTOS.h>

static unsigned int
target(struct sk_buff **pskb,
       unsigned int hooknum,
       const struct net_device *in,
       const struct net_device *out,
       const void *targinfo,
       void *userinfo)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	const struct ipt_FTOS_info *ftosinfo = targinfo;

	if ((iph->tos & IPTOS_TOS_MASK) != ftosinfo->ftos) {
		u_int16_t diffs[2];

		/* raw socket (tcpdump) may have clone of incoming
                   skb: don't disturb it --RR */
		if (skb_cloned(*pskb) && !(*pskb)->sk) {
			struct sk_buff *nskb = skb_copy(*pskb, GFP_ATOMIC);
			if (!nskb)
				return NF_DROP;
			kfree_skb(*pskb);
			*pskb = nskb;
			iph = (*pskb)->nh.iph;
		}

		diffs[0] = htons(iph->tos) ^ 0xFFFF;
		iph->tos = ftosinfo->ftos;
		diffs[1] = htons(iph->tos);
		iph->check = csum_fold(csum_partial((char *)diffs,
		                                    sizeof(diffs),
		                                    iph->check^0xFFFF));
		(*pskb)->nfcache |= NFC_ALTERED;
	}
	return IPT_CONTINUE;
}

static int
checkentry(const char *tablename,
	   const struct ipt_entry *e,
           void *targinfo,
           unsigned int targinfosize,
           unsigned int hook_mask)
{
	const u_int8_t ftos = ((struct ipt_FTOS_info *)targinfo)->ftos;

	if (targinfosize != IPT_ALIGN(sizeof(struct ipt_FTOS_info))) {
		printk(KERN_WARNING "FTOS: targinfosize %u != %Zu\n",
		       targinfosize,
		       IPT_ALIGN(sizeof(struct ipt_FTOS_info)));
		return 0;
	}

	if (strcmp(tablename, "mangle") != 0) {
		printk(KERN_WARNING "FTOS: can only be called from \"mangle\" table, not \"%s\"\n", tablename);
		return 0;
	}

	return 1;
}

#if defined(CONFIG_MIPS_BRCM)
static struct ipt_target ipt_ftos_reg = {
	.name		= "FTOS",
	.target		= target,
	.checkentry	= checkentry,
	.me		= THIS_MODULE,
};
#else
static struct ipt_target ipt_ftos_reg
= { { NULL, NULL }, "FTOS", target, checkentry, NULL, THIS_MODULE };
#endif

static int __init init(void)
{
	if (ipt_register_target(&ipt_ftos_reg))
		return -EINVAL;

	return 0;
}

static void __exit fini(void)
{
	ipt_unregister_target(&ipt_ftos_reg);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
