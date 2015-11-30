/*
 *	Device handling code
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>

#include <asm/uaccess.h>
#include "br_private.h"

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
#include "br_igmp.h"
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
#include "br_mld.h"
#endif


static struct net_device_stats *br_dev_get_stats(struct net_device *dev)
{
	//struct net_bridge *br = netdev_priv(dev);
	//return &br->statistics;
	return &dev->stats;
	
}

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
static BlogStats_t * br_dev_get_bstats(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	return &br->bstats;
}

static struct net_device_stats *br_dev_get_cstats(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	return &br->cstats;
}

static struct net_device_stats * br_dev_collect_stats(struct net_device *dev_p)
{
	BlogStats_t bStats;
	BlogStats_t * bStats_p;
	struct net_device_stats *dStats_p;
	struct net_device_stats *cStats_p;

	if ( dev_p == (struct net_device *)NULL )
		return (struct net_device_stats *)NULL;

	dStats_p = br_dev_get_stats(dev_p);
	cStats_p = br_dev_get_cstats(dev_p);
	bStats_p = br_dev_get_bstats(dev_p);

	memset(&bStats, 0, sizeof(BlogStats_t));
	blog_gstats(dev_p, &bStats, BSTATS_NOCLR);

	memcpy( cStats_p, dStats_p, sizeof(struct net_device_stats) );
	cStats_p->rx_packets += ( bStats.rx_packets + bStats_p->rx_packets );
	cStats_p->tx_packets += ( bStats.tx_packets + bStats_p->tx_packets );
	cStats_p->rx_bytes   += ( bStats.rx_bytes   + bStats_p->rx_bytes );
	cStats_p->tx_bytes   += ( bStats.tx_bytes   + bStats_p->tx_bytes );
	cStats_p->multicast  += ( bStats.multicast  + bStats_p->multicast );

	return cStats_p;
}

static void br_dev_update_stats(struct net_device * dev_p, 
                                BlogStats_t * blogStats_p)
{
	BlogStats_t * bStats_p;

	if ( dev_p == (struct net_device *)NULL )
		return;

	bStats_p = br_dev_get_bstats(dev_p);

	bStats_p->rx_packets += blogStats_p->rx_packets;
	bStats_p->tx_packets += blogStats_p->tx_packets;
	bStats_p->rx_bytes   += blogStats_p->rx_bytes;
	bStats_p->tx_bytes   += blogStats_p->tx_bytes;
	bStats_p->multicast  += blogStats_p->multicast;

	return;
}

static void br_dev_clear_stats(struct net_device * dev_p)
{
	BlogStats_t * bStats_p;
	struct net_device_stats *dStats_p;
	struct net_device_stats *cStats_p;

	if ( dev_p == (struct net_device *)NULL )
		return;

	dStats_p = br_dev_get_stats(dev_p);
	cStats_p = br_dev_get_cstats(dev_p); 
	bStats_p = br_dev_get_bstats(dev_p);

	blog_gstats(dev_p, NULL, BSTATS_CLR);
	memset(bStats_p, 0, sizeof(BlogStats_t));
	memset(dStats_p, 0, sizeof(struct net_device_stats));
	memset(cStats_p, 0, sizeof(struct net_device_stats));

	return;
}
#endif

/* net device transmit always called with no BH (preempt_disabled) */
int br_dev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);
	const unsigned char *dest = skb->data;
	struct net_bridge_fdb_entry *dst;

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
	blog_dev( skb, dev, DIR_TX, skb->len );
#endif

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
	if ((0x33 == dest[0]) && (0x33 == dest[1])) {
		if (!br_mld_mc_forward(br, skb, 0, 0)) 
			br_flood_deliver(br, skb);
	}
	else
#endif
	if (dest[0] & 1) {
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
		if (!br_igmp_mc_forward(br, skb, 0, 0))		
#endif
		br_flood_deliver(br, skb);
	}
	else if ((dst = __br_fdb_get(br, dest)) != NULL)
		br_deliver(dst->dst, skb);
	else
		br_flood_deliver(br, skb);

	return 0;
}




static int br_dev_open(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	br_features_recompute(br);
	netif_start_queue(dev);
	br_stp_enable_bridge(br);

	return 0;
}

static void br_dev_set_multicast_list(struct net_device *dev)
{
}

static int br_dev_stop(struct net_device *dev)
{
	br_stp_disable_bridge(netdev_priv(dev));

	netif_stop_queue(dev);

	return 0;
}

static int br_change_mtu(struct net_device *dev, int new_mtu)
{
	struct net_bridge *br = netdev_priv(dev);
	if (new_mtu < 68 || new_mtu > br_min_mtu(br))
		return -EINVAL;

	dev->mtu = new_mtu;

#ifdef CONFIG_BRIDGE_NETFILTER
	/* remember the MTU in the rtable for PMTU */
	br->fake_rtable.u.dst.metrics[RTAX_MTU - 1] = new_mtu;
#endif

	return 0;
}

/* Allow setting mac address to any valid ethernet address. */
static int br_set_mac_address(struct net_device *dev, void *p)
{
	struct net_bridge *br = netdev_priv(dev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EINVAL;

	spin_lock_bh(&br->lock);
	memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);
	br_stp_change_bridge_id(br, addr->sa_data);
	br->flags |= BR_SET_MAC_ADDR;
	spin_unlock_bh(&br->lock);

	return 0;
}

static void br_getinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strcpy(info->driver, "bridge");
	strcpy(info->version, BR_VERSION);
	strcpy(info->fw_version, "N/A");
	strcpy(info->bus_info, "N/A");
}

static int br_set_sg(struct net_device *dev, u32 data)
{
	struct net_bridge *br = netdev_priv(dev);

	if (data)
		br->feature_mask |= NETIF_F_SG;
	else
		br->feature_mask &= ~NETIF_F_SG;

	br_features_recompute(br);
	return 0;
}

static int br_set_tso(struct net_device *dev, u32 data)
{
	struct net_bridge *br = netdev_priv(dev);

	if (data)
		br->feature_mask |= NETIF_F_TSO;
	else
		br->feature_mask &= ~NETIF_F_TSO;

	br_features_recompute(br);
	return 0;
}

static int br_set_tx_csum(struct net_device *dev, u32 data)
{
	struct net_bridge *br = netdev_priv(dev);

	if (data)
		br->feature_mask |= NETIF_F_NO_CSUM;
	else
		br->feature_mask &= ~NETIF_F_ALL_CSUM;

	br_features_recompute(br);
	return 0;
}

static const struct ethtool_ops br_ethtool_ops = {
	.get_drvinfo    = br_getinfo,
	.get_link	= ethtool_op_get_link,
	.get_tx_csum	= ethtool_op_get_tx_csum,
	.set_tx_csum 	= br_set_tx_csum,
	.get_sg		= ethtool_op_get_sg,
	.set_sg		= br_set_sg,
	.get_tso	= ethtool_op_get_tso,
	.set_tso	= br_set_tso,
	.get_ufo	= ethtool_op_get_ufo,
	.get_flags	= ethtool_op_get_flags,
};

static const struct net_device_ops br_netdev_ops = {
	.ndo_open		 = br_dev_open,
	.ndo_stop		 = br_dev_stop,
	.ndo_start_xmit		 = br_dev_xmit,
	.ndo_set_mac_address	 = br_set_mac_address,
	.ndo_set_multicast_list	 = br_dev_set_multicast_list,
	.ndo_change_mtu		 = br_change_mtu,
	.ndo_do_ioctl		 = br_dev_ioctl,
#ifdef CONFIG_BLOG
        .ndo_get_stats           = br_dev_collect_stats,
#else
        .ndo_get_stats           = br_dev_get_stats
#endif
};

void br_dev_setup(struct net_device *dev)
{
	random_ether_addr(dev->dev_addr);
	ether_setup(dev);

#ifdef CONFIG_BLOG
	dev->put_stats = br_dev_update_stats;
	dev->clr_stats = br_dev_clear_stats;
#endif


	dev->netdev_ops = &br_netdev_ops;
	dev->destructor = free_netdev;
	SET_ETHTOOL_OPS(dev, &br_ethtool_ops);
	dev->tx_queue_len = 0;
	dev->priv_flags = IFF_EBRIDGE;

	dev->features = NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_HIGHDMA |
			NETIF_F_GSO_MASK | NETIF_F_NO_CSUM | NETIF_F_LLTX |
			NETIF_F_NETNS_LOCAL | NETIF_F_GSO;
}
