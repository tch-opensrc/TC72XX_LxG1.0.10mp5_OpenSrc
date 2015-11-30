#insert dyntos module and modules it depends on
insmod /lib/modules/2.6.21.5/kernel/net/netfilter/nf_conntrack.ko
insmod /lib/modules/2.6.21.5/kernel/net/netfilter/nf_dyntos.ko

#configure WAN2LAN DSCP Transmark table  
echo add 0x38 0x20 >/proc/net/netfilter/nf_dtos_w2ldscp_transtbl
echo add 0x30 0x20 >/proc/net/netfilter/nf_dtos_w2ldscp_transtbl
echo add 0x2E 0x30 >/proc/net/netfilter/nf_dtos_w2ldscp_transtbl
echo add 0x28 0x30 >/proc/net/netfilter/nf_dtos_w2ldscp_transtbl
