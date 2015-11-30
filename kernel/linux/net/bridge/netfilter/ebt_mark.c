/*
 *  ebt_mark
 *
 *	Authors:
 *	Bart De Schuymer <bdschuym@pandora.be>
 *
 *  July, 2002
 *
 */

/* The mark target can be used in any chain,
 * I believe adding a mangle table just for marking is total overkill.
 * Marking a frame doesn't really change anything in the frame anyway.
 */

#include <linux/module.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_mark_t.h>

static unsigned int
ebt_mark_tg(struct sk_buff *skb, const struct xt_target_param *par)
{
	const struct ebt_mark_t_info *info = par->targinfo;
	int action = info->target & -16;

	if (action == MARK_SET_VALUE)
		skb->mark = info->mark;
	else if (action == MARK_OR_VALUE)
		skb->mark |= info->mark;
	else if (action == MARK_AND_VALUE)
		skb->mark &= info->mark;
	else if (action == MARK_XOR_VALUE)
		skb->mark ^= info->mark;
	else  //brcm
		skb->vtag = (unsigned short)(info->mark);

// brcm -- begin
   /* if this is an 8021Q frame and skb->vtag is not zero, we need to do 8021p priority
    * or vlan id remarking.
    */
	if ((skb->protocol == __constant_htons(ETH_P_8021Q)) && skb->vtag) {
      /* we want to save the original vlan tag before remarking it with the QoS matching
       * rule, so that it can be restored in case this frame egresses to a vlan interface
       * and we need to do double tagging.
       */
      /* bit 12 of skb->vtag_save tells whether this frame had been remarked or not. */
      if ((skb->vtag_save & 0x1000) == 0) {

   	   struct vlan_hdr *frame;
	      unsigned short TCI;

		   frame = (struct vlan_hdr *)(skb->network_header);
		   TCI = ntohs(frame->h_vlan_TCI);

         /* set bit 12 of vtag_save to indicate that this vlan frame has been remarked. */
         skb->vtag_save = (TCI | 0x1000);

         /* if the 8021p priority field (bits 0-3) of skb->vtag is not zero, remark
          * 8021p priority of the frame.  Since the 8021p priority value in vtag had
          * been incremented by 1, we need to minus 1 from it to get the exact value.
          */
         if (skb->vtag & 0xf)
		      TCI = (TCI & 0x1fff) | (((skb->vtag & 0xf) - 1) << 13);

         /* if the vlan id field (bits 4-15) of skb->vtag is not zero, remark vlan id of
          * the frame.  Since the vlan id value in vtag had been incremented by 1, we need
          * to minus 1 from it to get the exact value.
          */
         if (skb->vtag & 0xfff0)
            TCI = (TCI & 0xf000) | (((skb->vtag >> 4) & 0xfff) - 1);

		   frame->h_vlan_TCI = htons(TCI);
      }
	}
// brcm -- end

	return info->target | ~EBT_VERDICT_BITS;
}

static bool ebt_mark_tg_check(const struct xt_tgchk_param *par)
{
	const struct ebt_mark_t_info *info = par->targinfo;
	int tmp;

	tmp = info->target | ~EBT_VERDICT_BITS;
	if (BASE_CHAIN && tmp == EBT_RETURN)
		return false;
	if (tmp < -NUM_STANDARD_TARGETS || tmp >= 0)
		return false;
	tmp = info->target & ~EBT_VERDICT_BITS;
	if (tmp != MARK_SET_VALUE && tmp != MARK_OR_VALUE &&
	    tmp != MARK_AND_VALUE && tmp != MARK_XOR_VALUE &&
       tmp != VTAG_SET_VALUE)    /* brcm */
		return false;
	return true;
}

static struct xt_target ebt_mark_tg_reg __read_mostly = {
	.name		= "mark",
	.revision	= 0,
	.family		= NFPROTO_BRIDGE,
	.target		= ebt_mark_tg,
	.checkentry	= ebt_mark_tg_check,
	.targetsize	= XT_ALIGN(sizeof(struct ebt_mark_t_info)),
	.me		= THIS_MODULE,
};

static int __init ebt_mark_init(void)
{
	return xt_register_target(&ebt_mark_tg_reg);
}

static void __exit ebt_mark_fini(void)
{
	xt_unregister_target(&ebt_mark_tg_reg);
}

module_init(ebt_mark_init);
module_exit(ebt_mark_fini);
MODULE_DESCRIPTION("Ebtables: Packet mark modification");
MODULE_LICENSE("GPL");
