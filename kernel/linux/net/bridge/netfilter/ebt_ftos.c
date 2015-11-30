/*
 *  ebt_ftos
 *
 *	Authors:
 *	 Song Wang <songw@broadcom.com>
 *
 *  Feb, 2004
 *
 */

// The ftos target can be used in any chain
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/checksum.h>
#include <linux/if_vlan.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_ftos_t.h>

static unsigned int ebt_ftos_tg(struct sk_buff *skb, const struct xt_target_param *par)      
{
	//struct ebt_ftos_t_info *ftosinfo = (struct ebt_ftos_t_info *)data;
	const  struct ebt_ftos_t_info *ftosinfo = par->targinfo;
	struct iphdr *iph;
        struct vlan_hdr *frame;	
	unsigned char prio = 0;
	unsigned short TCI;
        /* Need to recalculate IP header checksum after altering TOS byte */
	u_int16_t diffs[2];

	/* if VLAN frame, we need to point to correct network header */
	if (skb->protocol == __constant_htons(ETH_P_8021Q))
        	iph = (struct iphdr *)(skb->network_header+ VLAN_HLEN);
        else
			//iph = skb->nh.iph;
			iph = (struct iphdr *)(skb->network_header);


	if ((ftosinfo->ftos_set & FTOS_SETFTOS) && (iph->tos != ftosinfo->ftos)) {
                //printk("ebt_target_ftos:FTOS_SETFTOS .....\n");
		/* raw socket (tcpdump) may have clone of incoming
                   skb: don't disturb it --RR */
		if (skb_cloned(skb) && !skb->sk) {
			struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);
			if (!nskb)
				return NF_DROP;
			kfree_skb(skb);
			skb = nskb;
			if ((skb)->protocol == __constant_htons(ETH_P_8021Q))
                		iph = (struct iphdr *)(skb->network_header + VLAN_HLEN);
        		else
					//iph = skb->nh.iph;
					iph = (struct iphdr *)(skb->network_header);
		}


		diffs[0] = htons(iph->tos) ^ 0xFFFF;
		iph->tos = ftosinfo->ftos;
		diffs[1] = htons(iph->tos);
		iph->check = csum_fold(csum_partial((char *)diffs,
		                                    sizeof(diffs),
		                                    iph->check^0xFFFF));		
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// member below is removed
//		(*pskb)->nfcache |= NFC_ALTERED;
	} else if (ftosinfo->ftos_set & FTOS_WMMFTOS) {
	    //printk("ebt_target_ftos:FTOS_WMMFTOS .....0x%08x\n", (*pskb)->mark & 0xf);
		/* raw socket (tcpdump) may have clone of incoming
                   skb: don't disturb it --RR */
		if (skb_cloned(skb) && !skb->sk) {
			struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);
			if (!nskb)
				return NF_DROP;
			kfree_skb(skb);
			skb = nskb;
			if (skb->protocol == __constant_htons(ETH_P_8021Q))
                		iph = (struct iphdr *)(skb->network_header + VLAN_HLEN);
        		else
					//iph = skb->nh.iph;
					iph = (struct iphdr *)(skb->network_header);
	}

        diffs[0] = htons(iph->tos) ^ 0xFFFF;
	    iph->tos |= ((skb->mark >> PRIO_LOC_NFMARK) & PRIO_LOC_NFMASK) << DSCP_MASK_SHIFT;
		diffs[1] = htons(iph->tos);
		iph->check = csum_fold(csum_partial((char *)diffs,
		                                    sizeof(diffs),
		                                    iph->check^0xFFFF));
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// member below is removed
//        (*pskb)->nfcache |= NFC_ALTERED;
	} else if ((ftosinfo->ftos_set & FTOS_8021QFTOS) && skb->protocol == __constant_htons(ETH_P_8021Q)) {
	    
	    /* raw socket (tcpdump) may have clone of incoming
           skb: don't disturb it --RR */
		if (skb_cloned(skb) && !skb->sk) {
			struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);
			if (!nskb)
				return NF_DROP;
			kfree_skb(skb);
			skb = nskb;

            iph = (struct iphdr *)(skb->network_header + VLAN_HLEN);
            frame = (struct vlan_hdr *)(skb->network_header);
			TCI = ntohs(frame->h_vlan_TCI);
			prio = (unsigned char)((TCI >> 13) & 0x7);
		}
        //printk("ebt_target_ftos:FTOS_8021QFTOS ..... 0x%08x\n", prio);
        diffs[0] = htons(iph->tos) ^ 0xFFFF;
	    iph->tos |= (prio & 0xf) << DSCP_MASK_SHIFT;
		diffs[1] = htons(iph->tos);
		iph->check = csum_fold(csum_partial((char *)diffs,
		                                    sizeof(diffs),
		                                    iph->check^0xFFFF)); 
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// member below is removed
//        (*pskb)->nfcache |= NFC_ALTERED;
	}

	return ftosinfo->target;
}

static bool ebt_ftos_tg_check(const struct xt_tgchk_param *par)
{
	const struct ebt_ftos_t_info *info = par->targinfo;
/*
	if (datalen != sizeof(struct ebt_ftos_t_info))
		return -EINVAL;
*/
	if (BASE_CHAIN && info->target == EBT_RETURN)
		return false;
	
	//CLEAR_BASE_CHAIN_BIT;
	
	if (INVALID_TARGET)
		return false;
	
	return true;
}

static struct xt_target ebt_ftos_tg_reg = {
	.name		= EBT_FTOS_TARGET,
	.revision	= 0,
	.family		= NFPROTO_BRIDGE,
	.target		= ebt_ftos_tg,
	.checkentry	= ebt_ftos_tg_check,
	.targetsize	= XT_ALIGN(sizeof(struct ebt_ftos_t_info)),
	.me		= THIS_MODULE,
};

static int __init ebt_ftos_init(void)
{
	int ret;
	ret = xt_register_target(&ebt_ftos_tg_reg);

	if(ret == 0)
		printk(KERN_INFO "ebt_ftos registered\n");

	return ret;
}

static void __exit ebt_ftos_fini(void)
{
	xt_unregister_target(&ebt_ftos_tg_reg);
}

module_init(ebt_ftos_init);
module_exit(ebt_ftos_fini);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Song Wang, songw@broadcom.com");
MODULE_DESCRIPTION("Target to overwrite the full TOS byte in IP header");
