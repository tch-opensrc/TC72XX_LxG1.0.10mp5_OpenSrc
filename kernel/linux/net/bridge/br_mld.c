#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/times.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/jhash.h>
#include <asm/atomic.h>
#include <linux/ip.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>

#include "br_private.h"
#include "br_mld.h"

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)

struct proc_dir_entry *br_mld_entry = NULL;

extern int mcpd_process_skb(char *br_name, 
                             int if_index, 
                             int port_no, 
                             struct sk_buff *skb);

/* ipv6 address mac address */
static mac_addr upnp_addr = {{0x33, 0x33, 0x00, 0x00, 0x00, 0x0f}};
static mac_addr sys1_addr = {{0x33, 0x33, 0x00, 0x00, 0x00, 0x01}};
static mac_addr sys2_addr = {{0x33, 0x33, 0x00, 0x00, 0x00, 0x02}};
static mac_addr ospf1_addr = {{0x33, 0x33, 0x00, 0x00, 0x00, 0x05}};
static mac_addr ospf2_addr = {{0x33, 0x33, 0x00, 0x00, 0x00, 0x06}};
static mac_addr ripv2_addr = {{0x33, 0x33, 0x5e, 0x00, 0x00, 0x09}};
static mac_addr sys_addr = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

static int mld_control_filter(const unsigned char *dest)
{
   if ((!compare_ether_addr(dest, (const u8 *) &upnp_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&sys1_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&sys2_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&ospf1_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&ospf2_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&sys_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&ripv2_addr)) ||
       (!compare_ether_addr(dest, (const u8 *)&sys_addr)))
      return 0;
   else
      return 1;
}

static void mld_query_timeout(unsigned long ptr)
{
	struct net_br_mld_mc_fdb_entry *dst;
	struct net_bridge *br;
    
	br = (struct net_bridge *) ptr;

	spin_lock_bh(&br->mld_mcl_lock);
	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mld_mc_list, list) {
	    if (time_after_eq(jiffies, dst->tstamp)) {
		list_del_rcu(&dst->list);
		kfree(dst);
	    }
	}
	rcu_read_unlock();
	spin_unlock_bh(&br->mcl_lock);
		
	mod_timer(&br->mld_timer, jiffies + TIMER_CHECK_TIMEOUT*HZ);		
}

static int br_mld_mc_fdb_update(struct net_bridge *br, 
                                struct net_bridge_port *prt, 
                                struct in6_addr *grp, 
                                struct in6_addr *rep, 
                                int mode, 
                                struct in6_addr *src)
{
	struct net_br_mld_mc_fdb_entry *dst;
	int ret = 0;
	int filt_mode;

	if(mode == SNOOP_IN_ADD)
	   filt_mode = MCAST_INCLUDE;
	else
	   filt_mode = MCAST_EXCLUDE;
    
	rcu_read_lock(); 
	list_for_each_entry_rcu(dst, &br->mld_mc_list, list) {
	    if ((IN6_ARE_ADDR_EQUAL(&dst->grp, grp)) &&
		(filt_mode == dst->src_entry.filt_mode) && 
		(dst->dst == prt) &&
		(IN6_ARE_ADDR_EQUAL(&dst->rep, rep)) && 
	        (IN6_ARE_ADDR_EQUAL(src, &dst->src_entry.src))) {
	             dst->tstamp = jiffies + QUERY_TIMEOUT*HZ;
	             ret = 1;
           }
	}
        rcu_read_unlock();

	return ret;
}

static struct net_br_mld_mc_fdb_entry *br_mld_mc_fdb_get(struct net_bridge *br, 
                                                         struct net_bridge_port *prt, 
                                                         struct in6_addr *grp, 
                                                         struct in6_addr *rep, 
                                                         int mode, 
                                                         struct in6_addr *src)
{
	struct net_br_mld_mc_fdb_entry *dst;
	int filt_mode;
    
	if(mode == SNOOP_IN_CLEAR)
	   filt_mode = MCAST_INCLUDE;
	else
	   filt_mode = MCAST_EXCLUDE;
          
	rcu_read_lock(); 
	list_for_each_entry_rcu(dst, &br->mld_mc_list, list) {
	    if (IN6_ARE_ADDR_EQUAL(&dst->grp, grp) && 
		(dst->dst == prt) &&
                (filt_mode == dst->src_entry.filt_mode) && 
                (IN6_ARE_ADDR_EQUAL(&dst->rep, rep)) &&
                (IN6_ARE_ADDR_EQUAL(&dst->src_entry.src, src))) {
		    return dst;
	    }
	}
	rcu_read_unlock(); 
	
	return NULL;
}

int br_mld_mc_fdb_add(struct net_bridge *br, 
                      struct net_bridge_port *prt, 
                      struct in6_addr *grp, 
                      struct in6_addr *rep, 
                      int mode, 
                      struct in6_addr *src)
{
	struct net_br_mld_mc_fdb_entry *mc_fdb;

        if(!br || !prt || !grp || !rep)
            return 0;

        if((SNOOP_IN_ADD != mode) && (SNOOP_EX_ADD != mode))             
            return 0;

	if (br_mld_mc_fdb_update(br, prt, grp, rep, mode, src))
	    return 0;

	mc_fdb = kzalloc(sizeof(struct net_br_mld_mc_fdb_entry), GFP_KERNEL);
	if (!mc_fdb)
	    return ENOMEM;
	memcpy(&mc_fdb->grp, grp, sizeof(struct in6_addr));
	memcpy(&mc_fdb->rep, rep, sizeof(struct in6_addr));
	memcpy(&mc_fdb->src_entry, src, sizeof(struct in6_addr));
	mc_fdb->src_entry.filt_mode = 
                  (mode == SNOOP_IN_ADD) ? MCAST_INCLUDE : MCAST_EXCLUDE;
	mc_fdb->dst = prt;
	mc_fdb->tstamp = jiffies + QUERY_TIMEOUT*HZ;

	spin_lock_bh(&br->mld_mcl_lock);
	list_add_tail_rcu(&mc_fdb->list, &br->mld_mc_list);
	spin_unlock_bh(&br->mld_mcl_lock);

	if (!br->start_timer) {
    	    init_timer(&br->mld_timer);
	    br->mld_timer.expires = jiffies + TIMER_CHECK_TIMEOUT*HZ;
	    br->mld_timer.function = mld_query_timeout;
	    br->mld_timer.data = (unsigned long) br;
	    add_timer(&br->mld_timer);
	    br->start_timer = 1;
	}

	return 1;
}

void br_mld_mc_fdb_cleanup(struct net_bridge *br)
{
	struct net_br_mld_mc_fdb_entry *dst;
    
	spin_lock_bh(&br->mld_mcl_lock);
	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mld_mc_list, list) {
	    list_del_rcu(&dst->list);
	    kfree(dst);
	}
	rcu_read_unlock();
	spin_unlock_bh(&br->mld_mcl_lock);
}

void br_mld_mc_fdb_remove_grp(struct net_bridge *br, 
                              struct net_bridge_port *prt, 
                              struct in6_addr *grp)
{
	struct net_br_mld_mc_fdb_entry *dst;

	spin_lock_bh(&br->mld_mcl_lock);
	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mld_mc_list, list) {
	    if ((IN6_ARE_ADDR_EQUAL(&dst->grp, grp)) && 
	        (dst->dst == prt)) {
		list_del_rcu(&dst->list);
		kfree(dst);
	    }
	}
	rcu_read_unlock();
	spin_unlock_bh(&br->mld_mcl_lock);
}

int br_mld_mc_fdb_remove(struct net_bridge *br, 
                         struct net_bridge_port *prt, 
                         struct in6_addr *grp, 
                         struct in6_addr *rep, 
                         int mode, 
                         struct in6_addr *src)
{
	struct net_br_mld_mc_fdb_entry *mc_fdb;

        if((SNOOP_IN_CLEAR != mode) && (SNOOP_EX_CLEAR != mode))             
            return 0;
	
	if ((mc_fdb = br_mld_mc_fdb_get(br, prt, grp, rep, mode, src))) {
	    spin_lock_bh(&br->mld_mcl_lock);
	    list_del_rcu(&mc_fdb->list);
	    kfree(mc_fdb);
	    spin_unlock_bh(&br->mld_mcl_lock);

	    return 1;
	}
	
	return 0;
}

int br_mld_mc_forward(struct net_bridge *br, struct sk_buff *skb, int forward, int clone)
{
	struct net_br_mld_mc_fdb_entry *dst;
	int status = 0;
	struct sk_buff *skb2;
	struct net_bridge_port *p;
	unsigned char *mld_type = NULL;
	unsigned char *ipv6_addr = NULL;
	struct ipv6hdr *pipv6 = ipv6_hdr(skb);
	const unsigned char *dest = eth_hdr(skb)->h_dest;

	if (skb->data[6] == IPPROTO_ICMPV6) {
              mld_type = &skb->data[40];
	      ipv6_addr = &skb->data[48];
	} 
	else if ((skb->data[6] == IPPROTO_HOPOPTS) &&
                   (skb->data[40] == IPPROTO_ICMPV6)) {
	      mld_type = &skb->data[48];
	      ipv6_addr = &skb->data[56];
	}

	if((mld_type != NULL) && ((*mld_type == ICMPV6_MGM_REPORT) ||
		(*mld_type == ICMPV6_MGM_REDUCTION) || 
		(*mld_type == ICMPV6_MLD2_REPORT))) 
	{
	   if(skb->dev && (skb->dev->br_port) &&
	     (br->mld_snooping || br->mld_proxy)) 
	   {
	      mcpd_process_skb(br->dev->name, 
                               br->dev->ifindex, 
                               skb->dev->br_port->port_no, 
                               skb);
           }
	   return 0;
	}

	if (!br->mld_snooping)
		return 0;

	if ((br->mld_snooping == SNOOPING_BLOCKING_MODE) && 
             mld_control_filter(dest))
	    status = 1;

	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mld_mc_list, list) {
	    if (IN6_ARE_ADDR_EQUAL(&dst->grp, &pipv6->daddr)) {
              if((dst->src_entry.filt_mode == MCAST_INCLUDE) && 
                 (IN6_ARE_ADDR_EQUAL(&pipv6->saddr, &dst->src_entry.src))) {

		if (!dst->dst->dirty) {
		    skb2 = skb_clone(skb, GFP_ATOMIC);
		    if (forward)
			br_forward(dst->dst, skb2);
		    else
			br_deliver(dst->dst, skb2);
		}
		dst->dst->dirty = 1;
		status = 1;
              }
              else if(dst->src_entry.filt_mode == MCAST_EXCLUDE) {
                if((0 == dst->src_entry.src.s6_addr[0]) ||
                   (IN6_ARE_ADDR_EQUAL(&pipv6->saddr, &dst->src_entry.src))) {

		  if (!dst->dst->dirty) {
		    skb2 = skb_clone(skb, GFP_ATOMIC);
		    if (forward)
			br_forward(dst->dst, skb2);
		    else
			br_deliver(dst->dst, skb2);
		  }
		  dst->dst->dirty = 1;
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
	struct net_br_mld_mc_fdb_entry *dst;
	struct net_bridge *br = netdev_priv(dev);
	

	seq_printf(seq, "mld snooping %d\n", br->mld_snooping);

	seq_printf(seq, "bridge	device	group		   reporter          mode  source timeout\n");

	rcu_read_lock();
	list_for_each_entry_rcu(dst, &br->mld_mc_list, list) {
		seq_printf(seq, "%s %6s    ", br->dev->name, dst->dst->dev->name);
                seq_printf(seq, "%04x:%04x:%04x:%04x ",
                                     dst->grp.s6_addr32[0],
                                     dst->grp.s6_addr32[1],
                                     dst->grp.s6_addr32[2],
                                     dst->grp.s6_addr32[3]);
                seq_printf(seq, "%04x:%04x:%04x:%04x ",
                                     dst->rep.s6_addr32[0],
                                     dst->rep.s6_addr32[1],
                                     dst->rep.s6_addr32[2],
                                     dst->rep.s6_addr32[3]);
                                     

		seq_printf(seq, "%2s  %04x:%04x:%04x:%04x   %d\n", 
			(dst->src_entry.filt_mode == MCAST_EXCLUDE) ? 
			"EX" : "IN",
			dst->src_entry.src.s6_addr32[0], 
			dst->src_entry.src.s6_addr32[1], 
			dst->src_entry.src.s6_addr32[2], 
			dst->src_entry.src.s6_addr32[3], 
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

static struct file_operations br_mld_snoop_proc_fops = {
	.owner = THIS_MODULE,
	.open  = snoop_seq_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

void br_mld_snooping_init(void)
{
	br_mld_entry = create_proc_entry("net/mld_snooping", 0, NULL);

	if(br_mld_entry) {
		br_mld_entry->proc_fops = &br_mld_snoop_proc_fops;
	}
}
#endif
