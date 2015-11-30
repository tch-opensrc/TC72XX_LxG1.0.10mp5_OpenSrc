#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/times.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/jhash.h>
#include <asm/atomic.h>
#include <linux/ip.h>
#include "br_private.h"
#include "br_igmp.h"
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#if defined(CONFIG_MIPS_BRCM)
#include <linux/blog.h>
#endif
#include <linux/rtnetlink.h>

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)

struct proc_dir_entry *br_igmp_entry = NULL;

extern int mcpd_process_skb(char *br_name, 
                             int if_index, 
                             int port_no, 
                             struct sk_buff *skb);

/* Define ipv6 multicast mac address to let them pass through control filtering.
 * All ipv6 multicast mac addresses start with 0x33 0x33. So control_filter
 * only need to compare the first 2 bytes of the address.
 * only the left two bytes are significant *
 */
static mac_addr ipv6_mc_addr = {{0x33, 0x33, 0x00, 0x00, 0x00, 0x00}}; 
static mac_addr upnp_addr = {{0x01, 0x00, 0x5e, 0x7f, 0xff, 0xfa}};
static mac_addr sys1_addr = {{0x01, 0x00, 0x5e, 0x00, 0x00, 0x01}};
static mac_addr sys2_addr = {{0x01, 0x00, 0x5e, 0x00, 0x00, 0x02}};
static mac_addr ospf1_addr = {{0x01, 0x00, 0x5e, 0x00, 0x00, 0x05}};
static mac_addr ospf2_addr = {{0x01, 0x00, 0x5e, 0x00, 0x00, 0x06}};
static mac_addr ripv2_addr = {{0x01, 0x00, 0x5e, 0x00, 0x00, 0x09}};

static struct in_addr ip_upnp_addr  = {0xe07ffffa};

static int br_igmp_control_filter(const unsigned char *dest)
{
   if ((is_broadcast_ether_addr(dest)) ||
       (!compare_ether_addr(dest, (const u8 *)&upnp_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&sys1_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&sys2_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&ospf1_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&ospf2_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&ripv2_addr)) ||
       ((dest[0] == ipv6_mc_addr.addr[0]) && (dest[1] == ipv6_mc_addr.addr[1])))
      return 0;
   else
      return 1;
} /* br_igmp_control_filter */

static void br_igmp_query_timeout(unsigned long ptr)
{
	struct net_bridge_mc_fdb_entry *dst;
	struct net_bridge *br;
    
	br = (struct net_bridge *) ptr;

	spin_lock_bh(&br->mcl_lock);
	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mc_list, list) {
	    if (time_after_eq(jiffies, dst->tstamp)) {
		list_del_rcu(&dst->list);
		kfree(dst);
	    }
	}
	rcu_read_unlock();
	spin_unlock_bh(&br->mcl_lock);
		
	mod_timer(&br->igmp_timer, jiffies + TIMER_CHECK_TIMEOUT*HZ);		
}

static int br_mc_fdb_update(struct net_bridge *br, 
                            struct net_bridge_port *prt, 
                            struct in_addr *grp, 
                            struct in_addr *rep, 
                            int mode, 
                            struct in_addr *src)
{
	struct net_bridge_mc_fdb_entry *dst;
	int ret = 0;
	int filt_mode;

	if(mode == SNOOP_IN_ADD)
	   filt_mode = MCAST_INCLUDE;
	else
	   filt_mode = MCAST_EXCLUDE;
    
	rcu_read_lock(); 
	list_for_each_entry_rcu(dst, &br->mc_list, list) {
	    if (dst->grp.s_addr == grp->s_addr) {
	       if((src->s_addr == dst->src_entry.src.s_addr) &&
	          (filt_mode == dst->src_entry.filt_mode) && 
	          (dst->rep.s_addr == rep->s_addr) && 
	          (dst->dst == prt)) {
	             dst->tstamp = jiffies + QUERY_TIMEOUT*HZ;
	             ret = 1;
	       }
#if defined(CONFIG_BR_IGMP_SNOOP_SWITCH_PATCH)
	/* patch for igmp report flooding by robo */
	       else if ((0 == dst->src_entry.src.s_addr) &&
	                (MCAST_EXCLUDE == dst->src_entry.filt_mode)) {
	           dst->tstamp = jiffies + QUERY_TIMEOUT*HZ;
	       }
#endif /* CONFIG_BR_IGMP_SNOOP_SWITCH_PATCH*/
           }
	}
        rcu_read_unlock();

	return ret;
}

static struct net_bridge_mc_fdb_entry *br_mc_fdb_get(struct net_bridge *br, 
                                                     struct net_bridge_port *prt, 
                                                     struct in_addr *grp, 
                                                     struct in_addr *rep, 
                                                     int mode, 
                                                     struct in_addr *src)
{
	struct net_bridge_mc_fdb_entry *dst;
	int filt_mode;
    
	if(mode == SNOOP_IN_CLEAR)
	  filt_mode = MCAST_INCLUDE;
	else
	  filt_mode = MCAST_EXCLUDE;
          
	rcu_read_lock(); 
	list_for_each_entry_rcu(dst, &br->mc_list, list) {
	    if ((dst->grp.s_addr == grp->s_addr) && 
                (dst->rep.s_addr == rep->s_addr) &&
                (filt_mode == dst->src_entry.filt_mode) && 
                (dst->src_entry.src.s_addr == src->s_addr)) {
		if (dst->dst == prt)
		    return dst;
	    }
	}
	rcu_read_unlock(); 	
	
	return NULL;
}

int br_igmp_mc_fdb_add(struct net_bridge *br, 
                       struct net_bridge_port *prt, 
                       struct in_addr *grp, 
                       struct in_addr *rep, 
                       int mode, 
                       struct in_addr *src)
{
	struct net_bridge_mc_fdb_entry *mc_fdb;
#if defined(CONFIG_BR_IGMP_SNOOP_SWITCH_PATCH)
	struct net_bridge_mc_fdb_entry *mc_fdb_robo;
#endif /* CONFIG_BR_IGMP_SNOOP_SWITCH_PATCH */

	//printk("--- add mc entry ---\n");

        if(!br || !prt || !grp|| !rep)
            return 0;

        if((SNOOP_IN_ADD != mode) && (SNOOP_EX_ADD != mode))             
            return 0;

        if(grp->s_addr == ip_upnp_addr.s_addr)
           return 0;
	    
	if (br_mc_fdb_update(br, prt, grp, rep, mode, src))
	    return 0;

#if defined(CONFIG_BR_IGMP_SNOOP_SWITCH_PATCH)
	/* patch for snooping entry when LAN client access port is moved & 
           igmp report flooding by robo */
	spin_lock_bh(&br->mcl_lock);
	rcu_read_lock();
	list_for_each_entry_rcu(mc_fdb_robo, &br->mc_list, list) {
	   if ((mc_fdb_robo->grp.s_addr == grp->s_addr) &&
                (0 == mc_fdb_robo->src_entry.src.s_addr) &&
                (MCAST_EXCLUDE == mc_fdb_robo->src_entry.filt_mode) && 
		(mc_fdb_robo->rep.s_addr == rep->s_addr) &&
                (mc_fdb_robo->dst != prt)) {
		   list_del_rcu(&mc_fdb_robo->list);
		   kfree(mc_fdb_robo);
	   }
	}
        rcu_read_unlock();
	spin_unlock_bh(&br->mcl_lock);
#endif /* CONFIG_BR_IGMP_SNOOP_SWITCH_PATCH */
	    
	mc_fdb = kmalloc(sizeof(struct net_bridge_mc_fdb_entry), GFP_KERNEL);
	if (!mc_fdb)
	    return ENOMEM;
	mc_fdb->grp.s_addr = grp->s_addr;
	mc_fdb->rep.s_addr = rep->s_addr;
	memcpy(&mc_fdb->src_entry, src, sizeof(struct in_addr));
	mc_fdb->src_entry.filt_mode = 
                  (mode == SNOOP_IN_ADD) ? MCAST_INCLUDE : MCAST_EXCLUDE;
	mc_fdb->dst = prt;
	mc_fdb->tstamp = jiffies + QUERY_TIMEOUT*HZ;

	spin_lock_bh(&br->mcl_lock);
	list_add_tail_rcu(&mc_fdb->list, &br->mc_list);
	spin_unlock_bh(&br->mcl_lock);

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
	blog_notify(NULL, NULL, BLOG_EVENT_STOP);
#endif

	if (!br->start_timer) {
    	    init_timer(&br->igmp_timer);
	    br->igmp_timer.expires = jiffies + TIMER_CHECK_TIMEOUT*HZ;
	    br->igmp_timer.function = br_igmp_query_timeout;
	    br->igmp_timer.data = (unsigned long) br;
	    add_timer(&br->igmp_timer);
	    br->start_timer = 1;
	}

	return 1;
}

void br_igmp_mc_fdb_cleanup(struct net_bridge *br)
{
	struct net_bridge_mc_fdb_entry *dst;
    
	spin_lock_bh(&br->mcl_lock);
	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mc_list, list) {
	    list_del_rcu(&dst->list);
	    kfree(dst);
	}
	rcu_read_unlock();
	spin_unlock_bh(&br->mcl_lock);
}

void br_igmp_mc_fdb_remove_grp(struct net_bridge *br, struct net_bridge_port *prt, struct in_addr *grp)
{
	struct net_bridge_mc_fdb_entry *dst;

	spin_lock_bh(&br->mcl_lock);
	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mc_list, list) {
	    if ((dst->grp.s_addr == grp->s_addr) && (dst->dst == prt)) {
		list_del_rcu(&dst->list);
		kfree(dst);
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
		blog_notify(NULL, NULL, BLOG_EVENT_STOP);
#endif
	    }
	}
	rcu_read_unlock();
	spin_unlock_bh(&br->mcl_lock);
}

int br_igmp_mc_fdb_remove(struct net_bridge *br, 
                          struct net_bridge_port *prt, 
                          struct in_addr *grp, 
                          struct in_addr *rep, 
                          int mode, 
                          struct in_addr *src)
{
	struct net_bridge_mc_fdb_entry *mc_fdb;

	//printk("--- remove mc entry ---\n");

	if((SNOOP_IN_CLEAR != mode) && (SNOOP_EX_CLEAR != mode))
	   return 0;
	
	if ((mc_fdb = br_mc_fdb_get(br, prt, grp, rep, mode, src))) {
	    spin_lock_bh(&br->mcl_lock);
	    list_del_rcu(&mc_fdb->list);
	    kfree(mc_fdb);
	    spin_unlock_bh(&br->mcl_lock);
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
		blog_notify(NULL, NULL, BLOG_EVENT_STOP);
#endif

	    return 1;
	}
	
	return 0;
}

int br_igmp_mc_forward(struct net_bridge *br, 
                       struct sk_buff *skb, 
                       int forward, 
                       int clone)
{
	struct net_bridge_mc_fdb_entry *dst;
	int status = 0;
	struct sk_buff *skb2;
	struct net_bridge_port *p;
	struct iphdr *pip = ip_hdr(skb);
	const unsigned char *dest = eth_hdr(skb)->h_dest;

	if ((skb->data[9] == IPPROTO_IGMP)  &&
	    (br->igmp_proxy || br->igmp_snooping))
	{
	   if(skb->dev && (skb->dev->br_port)) 
	   { 
	      mcpd_process_skb(br->dev->name, 
                                      br->dev->ifindex, 
                                      skb->dev->br_port->port_no, 
                                      skb);
	   }
	   return 0;
	}

	if (!br->igmp_snooping)
		return 0;

	if ((br->igmp_snooping == SNOOPING_BLOCKING_MODE) && 
	    br_igmp_control_filter(dest))
	    status = 1;

	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mc_list, list) {
	    if (dst->grp.s_addr == pip->daddr) {
              if((dst->src_entry.filt_mode == MCAST_INCLUDE) && 
                 (pip->saddr == dst->src_entry.src.s_addr)) {
		if (!dst->dst->dirty) {
		    if((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL)
	               return 0;
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
			blog_clone(skb, skb2->blog_p);
#endif
		    if (forward)
			br_forward(dst->dst, skb2);
		    else
			br_deliver(dst->dst, skb2);
		}
		dst->dst->dirty = 1;
		status = 1;
              }
              else if(dst->src_entry.filt_mode == MCAST_EXCLUDE) {
                if((0 == dst->src_entry.src.s_addr) ||
                   (pip->saddr != dst->src_entry.src.s_addr)) {

		  if (!dst->dst->dirty) {
		    if((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL)
	               return 0;
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
			blog_clone(skb, skb2->blog_p);
#endif
		    if (forward)
			br_forward(dst->dst, skb2);
		    else
			br_deliver(dst->dst, skb2);
		  }
		  dst->dst->dirty = 1;
		  status = 1;
                }
                else if(pip->saddr == dst->src_entry.src.s_addr) {
                  status = 1;
                }
              }
	    }
	}

	if (status) {
	    list_for_each_entry_rcu(p, &br->port_list, list) {
		p->dirty = 0;
	  }
	}
	rcu_read_unlock();

	if((status) && (!clone))
	   kfree_skb(skb);

	return status;
}

static void *snoop_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct net_device *dev;
	loff_t offs = 0;

	rtnl_lock();
	ASSERT_RTNL();
	for_each_netdev(&init_net, dev)
        {
		if ((dev->priv_flags & IFF_EBRIDGE) &&
                    (*pos == offs)) { 
			return dev;
		}
	}
	++offs;
	return NULL;
}

static void *snoop_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct net_device *dev = v;

	++*pos;
	
	for(dev = next_net_device(dev); dev; dev = next_net_device(dev)) {
		if(dev->priv_flags & IFF_EBRIDGE)
			return dev;
	}
	return NULL;
}

static int snoop_seq_show(struct seq_file *seq, void *v)
{
	struct net_device *dev = v;
	struct net_bridge_mc_fdb_entry *dst;
	struct net_bridge *br = netdev_priv(dev);

	seq_printf(seq, "igmp snooping %d proxy %d\n", br->igmp_snooping, br->igmp_proxy);

	seq_printf(seq, "bridge	device	group\t reporter\tmode\tsource\ttimeout\n");

	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mc_list, list) {
		seq_printf(seq, "%s %6s    ", br->dev->name, dst->dst->dev->name);
                seq_printf(seq, "%04x\t%04x\t", dst->grp.s_addr, dst->rep.s_addr);

		seq_printf(seq, "%2s\t%04x\t%d\n", 
			(dst->src_entry.filt_mode == MCAST_EXCLUDE) ? 
			"EX" : "IN", dst->src_entry.src.s_addr, 
			(int) (dst->tstamp - jiffies)/HZ);
	}
	rcu_read_unlock();

	return 0;
}

static void snoop_seq_stop(struct seq_file *seq, void *v)
{
	rtnl_unlock();
}

static struct seq_operations snoop_seq_ops = {
	.start = snoop_seq_start,
	.next  = snoop_seq_next,
	.stop  = snoop_seq_stop,
	.show  = snoop_seq_show,
};

static int snoop_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &snoop_seq_ops);
}

static struct file_operations br_igmp_snoop_proc_fops = {
	.owner = THIS_MODULE,
	.open  = snoop_seq_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

void br_igmp_snooping_init(void)
{
	br_igmp_entry = proc_create("igmp_snooping", 0, init_net.proc_net,
			   &br_igmp_snoop_proc_fops);

	if(br_igmp_entry) {
           printk("error while creating igmp_snooping proc\n");
	}
}
#endif
