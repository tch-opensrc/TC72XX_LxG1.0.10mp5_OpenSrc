/* This is a module which is used for stopping logging. */

/* 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter/x_tables.h>
#if defined(CONFIG_MIPS_BRCM)
#include <linux/blog.h>
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("iptables stop logging module");
MODULE_ALIAS("ipt_SKIPLOG");

static unsigned int
skiplog_tg(struct sk_buff *skb, const struct xt_target_param *par)
{
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
	blog_skip(skb);
#endif

	return XT_CONTINUE;
}

static struct xt_target skiplog_tg_reg __read_mostly = {
	.name		= "SKIPLOG",
	.revision   = 0,
	.family		= NFPROTO_UNSPEC,
	.target		= skiplog_tg,
	.me		= THIS_MODULE,
};

static int __init skiplog_tg_init(void)
{
	return xt_register_target(&skiplog_tg_reg);
}

static void __exit skiplog_tg_exit(void)
{
	xt_unregister_target(&skiplog_tg_reg);
}

module_init(skiplog_tg_init);
module_exit(skiplog_tg_exit);
