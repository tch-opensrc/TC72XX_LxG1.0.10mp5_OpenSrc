/*
<:copyright-gpl
 Copyright 2008 Broadcom Corp. All Rights Reserved.

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
/******************************************************************************
 Filename:       nf_conntrack_proto_esp.c
 Author:         Pavan Kumar
 Creation Date:  05/27/04

 Description:
  Implements the ESP ALG connectiontracking.
  Migrated to kernel 2.6.21.5 on April 16, 2008 by Dan-Han Tsai.
*****************************************************************************/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/ip.h>

#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/netfilter/nf_conntrack_proto_esp.h>

#if 0
#define DEBUGP(format, args...)	printk(KERN_DEBUG "%s: " format, __FUNCTION__, ## args)
#else
#define DEBUGP(x, args...)
#endif

#define ESP_REF_TMOUT   (30 * HZ)
#define ESP_CONN_TMOUT  (60 * HZ * 6)
#define ESP_TMOUT_COUNT (ESP_CONN_TMOUT/ESP_REF_TMOUT)
#define ESP_STREAM_TIMEOUT (6000 * HZ)
#define ESP_TIMEOUT (3000 * HZ)

#define IPSEC_INUSE    1
#define MAX_PORTS      64
#define TEMP_SPI_START 1500

struct _esp_table 
{
   u_int32_t l_spi;
   u_int32_t r_spi;
   u_int32_t l_ip;
   u_int32_t r_ip;
   u_int32_t timeout;
   u_int16_t tspi;
   struct nf_conn *ct;
   struct timer_list refresh_timer;
   int    pkt_rcvd;
   int    ntimeouts;
   int    inuse;
};

static struct _esp_table esp_table[MAX_PORTS];
struct sk_buff nfskb_p;

static void esp_free_entry( int index )
{
   if( esp_table[index].inuse ) 
   {
      DEBUGP( "   %s Deleting timer index %d\n", 
               __FUNCTION__, index);
      del_timer(&esp_table[index].refresh_timer);
      memset(&esp_table[index], 0, sizeof(struct _esp_table));
   }
}

static void esp_refresh_ct( unsigned long data )
{
   struct _esp_table *esp_entry = NULL;

   if( data > MAX_PORTS ) 
      return;

   if( (esp_entry = &esp_table[data]) == NULL )
      return;

   DEBUGP( "   ntimeouts %d pkt_rcvd %d entry %p data %lu ct %p spi [0x%x - 0x%x] %u.%u.%u.%u <-> %u.%u.%u.%u\n", 
         esp_entry->ntimeouts, esp_entry->pkt_rcvd, esp_entry, data, esp_entry->ct, esp_entry->l_spi, esp_entry->r_spi, NIPQUAD(esp_entry->l_ip), NIPQUAD(esp_entry->r_ip));

   if( esp_entry->pkt_rcvd )
   {
      esp_entry->pkt_rcvd  = 0;
      esp_entry->ntimeouts = 0;
   } 
   else 
   {
      esp_entry->ntimeouts++;
      if( esp_entry->ntimeouts >= ESP_TMOUT_COUNT )
      {
         esp_free_entry(data);
         return;
      }
   }
   if( esp_entry->ct )
   {
      esp_entry->refresh_timer.expires = jiffies + ESP_REF_TMOUT;
      esp_entry->refresh_timer.function = esp_refresh_ct;
      esp_entry->refresh_timer.data = data;
      nfskb_p.nfct = (struct nf_conntrack *)esp_entry->ct;
      nf_ct_refresh_acct(esp_entry->ct, 0, &nfskb_p, ESP_CONN_TMOUT);
      add_timer(&esp_entry->refresh_timer);
   }
}

/*
 * Allocate a free IPSEC table entry.
 */
struct _esp_table *alloc_esp_entry( void )
{
   int idx = 0;

   for( ; idx < MAX_PORTS; idx++ )
   {
      if( esp_table[idx].inuse == IPSEC_INUSE )
         continue;

      memset(&esp_table[idx], 0, sizeof(struct _esp_table));
      esp_table[idx].tspi  = idx + TEMP_SPI_START;
      esp_table[idx].inuse = IPSEC_INUSE;

      DEBUGP( "   %s New esp_entry at idx %d tspi %u \n", 
         __FUNCTION__, idx, esp_table[idx].tspi );

      init_timer(&esp_table[idx].refresh_timer);
      esp_table[idx].refresh_timer.data = idx;
      esp_table[idx].refresh_timer.expires = jiffies + ESP_REF_TMOUT;
      esp_table[idx].refresh_timer.function = esp_refresh_ct;
      add_timer(&esp_table[idx].refresh_timer);
      return (&esp_table[idx]);
   }
   return NULL;
}

/*
 * Search an ESP table entry by source IP.
 * If found one, update the spi value
 */
struct _esp_table *search_esp_entry_by_ip( const struct nf_conntrack_tuple *tuple, const __u32 spi )
{
   int idx = 0;
   __u32 srcIP = tuple->src.u3.ip;
   __u32 dstIP = tuple->dst.u3.ip;
   struct _esp_table *esp_entry = esp_table;

   for( ; idx < MAX_PORTS; idx++, esp_entry++ )
   {
      DEBUGP("   Searching IP %u.%u.%u.%u <-> %u.%u.%u.%u,  %u.%u.%u.%u\n",
          NIPQUAD(srcIP), NIPQUAD(esp_entry->l_ip), 
          NIPQUAD(esp_entry->r_ip));
      
      /* make sure l_ip is LAN IP */
      if( (srcIP == esp_entry->l_ip) && (((unsigned char *)&(srcIP))[0] == 192) )
      {
         DEBUGP("   found entry with l_ip\n");
         esp_entry->l_spi = spi;

         /* This is a new connection of the same LAN host */
         if( dstIP != esp_entry->r_ip )
         {
            esp_entry->r_ip = dstIP;
            esp_entry->r_spi = 0;
         }
         return esp_entry;
      }
      else if( srcIP == esp_entry->r_ip )
      {
         DEBUGP("   found entry with r_ip\n");
         /* FIXME */
         if( esp_entry->r_spi == 0 )
         {
            DEBUGP("   found entry with r_ip and r_spi == 0\n");
            esp_entry->r_spi = spi;
            return esp_entry;
         }
	 /* We cannot handle spi changed at WAN side */
         DEBUGP("   found entry with r_ip but r_spi != 0\n");
      }
   }
   DEBUGP( "   %s No Entry\n", __FUNCTION__);
   return NULL;
}

/*
 * Search an ESP table entry by spi
 */
struct _esp_table *search_esp_entry_by_spi( const __u32 spi, const __u32 srcIP )
{
   int idx = 0;
   struct _esp_table *esp_entry = esp_table;

   for( ; idx < MAX_PORTS; idx++, esp_entry++ )
   {
      DEBUGP("   Searching spi %x <-> %x, %x\n",
          spi, esp_entry->l_spi, esp_entry->r_spi);
      
      if( (spi == esp_entry->l_spi) || (spi == esp_entry->r_spi) )
      {
         DEBUGP("   In %s, found entry %d with tspi %u\n",
                  __FUNCTION__, idx, esp_entry->tspi);

         /* l_spi and r_spi may be the same */
         if( (spi == esp_entry->l_spi) && (srcIP == esp_entry->r_ip) )
	 {
            DEBUGP("  In %s, l_spi==r_spi\n",  __FUNCTION__);
            esp_entry->r_spi = spi;
	 }

         return esp_entry;
      }
   }
   DEBUGP( "   %s No Entry\n", __FUNCTION__);
   return NULL;

}

/* invert esp part of tuple */
static int esp_invert_tuple(struct nf_conntrack_tuple *tuple,
			    const struct nf_conntrack_tuple *orig)
{
   DEBUGP("%s with spi = 0x%u\n",  __FUNCTION__, orig->src.u.esp.spi);
   tuple->dst.u.esp.spi = orig->dst.u.esp.spi;
   tuple->src.u.esp.spi = orig->src.u.esp.spi;
   return 1;
}

/* esp hdr info to tuple */
static int esp_pkt_to_tuple(const struct sk_buff *skb, unsigned int dataoff,
                            struct nf_conntrack_tuple *tuple)
{
   struct esphdr _esphdr, *esphdr;
   struct _esp_table *esp_entry = NULL;

   esphdr = skb_header_pointer(skb, dataoff, sizeof(_esphdr), &_esphdr);
   if( !esphdr ) 
   {
      /* try to behave like "nf_conntrack_proto_generic" */
      tuple->src.u.all = 0;
      tuple->dst.u.all = 0;
      return 1;
   }

   DEBUGP("Enter pkt_to_tuple() with spi %x\n", esphdr->spi);
   /* check if esphdr has a new SPI:
    *   if no, update tuple with correct tspi and increment pkt count;
    *   if yes, check if we have seen the source IP:
    *             if yes, do the tspi and pkt count update
    *             if no, create a new entry
    */
   if( (esp_entry = search_esp_entry_by_spi(esphdr->spi, tuple->src.u3.ip)) == NULL ) 
   {
      if( (esp_entry = 
           search_esp_entry_by_ip(tuple, esphdr->spi)) == NULL )
      {
#if 0
         /* Because SA is simplex, it's possible that WAN starts connection first. 
	  * We need to make sure that the connection starts from LAN.
	  */
         if( ((unsigned char *)&(tuple->src.u3.ip))[0] != 192 )
	 {
 	      DEBUGP("srcIP %u.%u.%u.%u is WAN IP, DROP packet\n", NIPQUAD(tuple->src.u3.ip));
	      return 0;
	 }
#endif
         esp_entry = alloc_esp_entry();
         if( esp_entry == NULL ) 
         {
            DEBUGP( "%s All entries in use\n", __FUNCTION__);
            return 0;
         }
         esp_entry->l_spi = esphdr->spi;
         esp_entry->l_ip = tuple->src.u3.ip;
         esp_entry->r_ip = tuple->dst.u3.ip;
      }
   }

   DEBUGP( "%s: entry_info: tspi %u l_spi 0x%x r_spi 0x%x l_ip %u.%u.%u.%u r_ip %u.%u.%u.%u srcIP %u.%u.%u.%u dstIP %u.%u.%u.%u\n",
         __FUNCTION__, esp_entry->tspi, esp_entry->l_spi, esp_entry->r_spi, NIPQUAD(esp_entry->l_ip), NIPQUAD(esp_entry->r_ip), NIPQUAD(tuple->src.u3.ip), NIPQUAD(tuple->dst.u3.ip) );

   tuple->dst.u.esp.spi = tuple->src.u.esp.spi = esp_entry->tspi;
   esp_entry->pkt_rcvd++;

   return 1;
}

/* print esp part of tuple */
static int esp_print_tuple(struct seq_file *s,
                           const struct nf_conntrack_tuple *tuple)
{
   return seq_printf(s, "srcspi=0x%x dstspi=0x%x ",
          tuple->src.u.esp.spi, tuple->dst.u.esp.spi);
}

/* print private data for conntrack */
static int esp_print_conntrack(struct seq_file *s, const struct nf_conn *ct)
{
   return seq_printf(s, "timeout=%u, stream_timeout=%u ",
                    (ct->proto.esp.timeout / HZ),
                    (ct->proto.esp.stream_timeout / HZ));
}

/* Returns verdict for packet, and may modify conntrack */
static int esp_packet(struct nf_conn *ct, const struct sk_buff *skb,
                      unsigned int dataoff, enum ip_conntrack_info ctinfo,
                      int pf, unsigned int hooknum)
{
   const struct iphdr *iph = skb->nh.iph;
   struct esphdr _esphdr, *esphdr;

   esphdr = skb_header_pointer(skb, dataoff, sizeof(_esphdr), &_esphdr);

   DEBUGP("%s (0x%x) %u.%u.%u.%u <-> %u.%u.%u.%u status %s info %d %s\n", __FUNCTION__, esphdr->spi, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr), (ct->status & IPS_SEEN_REPLY) ? "SEEN" : "NOT_SEEN", ctinfo, (ctinfo == IP_CT_NEW ) ? "CT_NEW" : "SEEN_REPLY" );

   /* If we've seen traffic both ways, this is some kind of ESP
      stream.  Extend timeout. */
   if( test_bit(IPS_SEEN_REPLY_BIT, &ct->status) ) 
   {
      nf_ct_refresh_acct(ct, ctinfo, skb, ESP_STREAM_TIMEOUT);
      /* Also, more likely to be important, and not a probe */
      if( !test_and_set_bit(IPS_ASSURED_BIT, &ct->status) )
         nf_conntrack_event_cache(IPCT_STATUS, skb);
   } 
   else
      nf_ct_refresh_acct(ct, ctinfo, skb, ESP_TIMEOUT);

   return NF_ACCEPT;
}

/* Called when a new connection for this protocol found. */
static int esp_new(struct nf_conn *ct, const struct sk_buff *skb,
                   unsigned int dataoff)
{
   const struct iphdr *iph = skb->nh.iph;
   struct _esp_table *esp_entry;
   struct esphdr _esphdr, *esphdr;

   ct->proto.esp.stream_timeout = ESP_STREAM_TIMEOUT;
   ct->proto.esp.timeout = ESP_TIMEOUT;

   esphdr = skb_header_pointer(skb, dataoff, sizeof(_esphdr), &_esphdr);

   DEBUGP("%s SPI(0x%x) %u.%u.%u.%u <-> %u.%u.%u.%u ct %p\n",
           __FUNCTION__, esphdr->spi, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr), ct);

   if( (esp_entry = search_esp_entry_by_spi(esphdr->spi, 0)) != NULL ) 
      esp_entry->ct = ct;
   else
      DEBUGP("In esp_new(), cannot find an entry with SPI %x\n", esphdr->spi);

   return 1;
}

/* protocol helper struct */
static struct nf_conntrack_l4proto nf_conntrack_l4proto_esp4 = {
   .l3proto = PF_INET,
   .l4proto = IPPROTO_ESP,
   .name = "esp",
   .pkt_to_tuple = esp_pkt_to_tuple,
   .invert_tuple = esp_invert_tuple,
   .print_tuple = esp_print_tuple,
   .print_conntrack = esp_print_conntrack,
   .packet = esp_packet,
   .new = esp_new,
   .me = THIS_MODULE,
};

static int __init nf_ct_proto_esp_init(void)
{
   return nf_conntrack_l4proto_register(&nf_conntrack_l4proto_esp4);
}

static void nf_ct_proto_esp_fini(void)
{
   nf_conntrack_l4proto_unregister(&nf_conntrack_l4proto_esp4);
}
module_init(nf_ct_proto_esp_init);
module_exit(nf_ct_proto_esp_fini);

MODULE_LICENSE("GPL");
