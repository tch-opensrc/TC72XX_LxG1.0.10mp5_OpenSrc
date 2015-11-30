/***************************************************************************
 * File Name    : if_smux.h
 * Description  : contains network driver implementation for Service MUX (SMUX) which enables to create multiple services on a physical interface like atm or ptm etc.,
 ***************************************************************************/
#ifndef _LINUX_IF_SMUX_H_
#define _LINUX_IF_SMUX_H_

#include <linux/netdevice.h>
#include <linux/skbuff.h>

/* found in socket.c */
extern void smux_ioctl_set(int (*hook)(void __user *));

/* smux device info in net_device. */
struct smux_dev_info {
  struct smux_group *smux_grp;
  struct net_device *vdev;
  struct net_device_stats stats; 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#ifdef CONFIG_BLOG
	BlogStats_t bstats; /* stats when the blog promiscuous layer has consumed packets */
	struct net_device_stats cstats; /* Cummulative Device stats (rx-bytes, tx-pkts, etc...) */
#endif
#endif
  int    proto;
  struct list_head  list;
};

/* represents a group of smux devices */
struct smux_group {
  struct net_device	*real_dev;
  struct list_head	smux_grp_devs;	
  struct list_head	virtual_devs;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#define SMUX_DEV_INFO(x) ((struct smux_dev_info *)(netdev_priv(x)))
#else
#define SMUX_DEV_INFO(x) ((struct smux_dev_info *)(x->priv))
#endif

/* inline functions */

static inline struct net_device_stats *smux_dev_get_stats(struct net_device *dev)
{
  return &(SMUX_DEV_INFO(dev)->stats);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#ifdef CONFIG_BLOG
static inline BlogStats_t * smux_dev_get_bstats(struct net_device *dev)
{
	return &(SMUX_DEV_INFO(dev)->bstats);
}
static inline struct net_device_stats *smux_dev_get_cstats(struct net_device *dev)
{
	return &(SMUX_DEV_INFO(dev)->cstats);
}
#endif
#endif

/* SMUX IOCTLs are found in sockios.h */

/* Passed in smux_ioctl_args structure to determine behaviour. Should be same as busybox/networking/smuxctl.c  */
enum smux_ioctl_cmds {
  ADD_SMUX_CMD,
  REM_SMUX_CMD,
};

enum smux_proto_types {
  SMUX_PROTO_PPPOE,
  SMUX_PROTO_IPOE,
  SMUX_PROTO_BRIDGE,
  SMUX_PROTO_HIGHEST
};

struct smux_ioctl_args {
  int cmd; /* Should be one of the smux_ioctl_cmds enum above. */
  int  proto;
  char ifname[IFNAMSIZ];
  union {
    char ifname[IFNAMSIZ]; /* smux device info */
  } u;
};
#endif /* _LINUX_IF_SMUX_H_ */
