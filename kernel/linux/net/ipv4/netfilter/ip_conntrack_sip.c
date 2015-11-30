/* SIP extension for IP connection tracking.
 *
 * (C) 2006 by polowang <polowang@broadcom.com>
 * based on RR's ip_conntrack_ftp.c and other modules.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/ctype.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <net/checksum.h>
#include <net/udp.h>

#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_conntrack_sip.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("polowang <polowang@broadcom.com>");
MODULE_DESCRIPTION("SIP connection tracking helper");

DECLARE_LOCK(sipbf_lock);

#define MAX_PORTS	8
#define SIP_MAP_PORT 50000
static int ports[MAX_PORTS];
static int ports_c;
module_param_array(ports, int, ports_c, 0400);
/*module_param_array(ports, int, &ports_c, 0400);*/
MODULE_PARM_DESC(ports, " port numbers of sip servers");

static unsigned int sip_timeout = SIP_TIMEOUT;

module_param(sip_timeout, int, 0600);
MODULE_PARM_DESC(sip_timeout, "timeout for the master sip session");

int ct_sip_get_info(const char *dptr, size_t dlen, 
				unsigned int *matchoff, 
				unsigned int *matchlen, 
				struct sip_header_nfo *hnfo);
EXPORT_SYMBOL(ct_sip_get_info);

int sip_getpkt_type(const char *dptr,size_t datalen);
EXPORT_SYMBOL(sip_getpkt_type);

int set_expected_rtp(struct sk_buff **pskb, 
			struct ip_conntrack *ct,
			enum ip_conntrack_info ctinfo, 
			struct ip_ct_sip_master *ct_sip_info
			);

EXPORT_SYMBOL(set_expected_rtp);
#if DEBUG_SIP_ALG
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif


static int sip_expect(struct ip_conntrack *conntrack);
static int skp_callid_len(const char *dptr, const char *limit, int *shift);
static int digits_len(const char *dptr, const char *limit, int *shift);
static int epaddr_len(const char *dptr, const char *limit, int *shift);
static int skp_digits_len(const char *dptr, const char *limit, int *shift);
static int skp_epaddr_len(const char *dptr, const char *limit, int *shift);
static int cseq_len(const char *dptr, const char *limit, int *shift);
static int sipGetIPPort(const char *dptr,unsigned int matchoff,unsigned int matchlen,uint32_t *pIpaddr,uint16_t *pPort);
static int sipGetIP(const char *dptr,unsigned int matchoff,unsigned int matchlen,uint32_t *pIpaddr);
static int sipCopyExp2Master(struct ip_ct_sip_master *ct_sip_info,struct ip_ct_sip_expect *exp_sip_info);
static int sipCopySipInfo(struct ip_ct_sip_master *ct_sip_dst_info,struct ip_ct_sip_master *ct_sip_src_info);
static int sip_rtp_expect(struct ip_conntrack *conntrack);

struct ip_ct_sip_data ct_sip_data[MAX_PORT_MAPS];
EXPORT_SYMBOL(ct_sip_data);

struct sip_header_nfo ct_sip_hdrs[] = {
	{ 	/* Via header */
		"Via:",		sizeof("Via:") - 1,
		"\r\nv:",	sizeof("\r\nv:") - 1, /* rfc3261 "\r\n" */
		"UDP ", 	sizeof("UDP ") - 1,
		epaddr_len
	},
	{ 	/* Contact header */
		"Contact:",	sizeof("Contact:") - 1,
		"\r\nm:",	sizeof("\r\nm:") - 1,
		"sip:",		sizeof("sip:") - 1,
		skp_epaddr_len
	},
	{ 	/* Content length header */
		"Content-Length:", sizeof("Content-Length:") - 1,
		"\r\nl:",	sizeof("\r\nl:") - 1,
		":",		sizeof(":") - 1, 
		skp_digits_len
	},
	{	/* SDP media info */
		"\nm=",		sizeof("\nm=") - 1,	
		"\rm=",		sizeof("\rm=") - 1,
		"audio ",	sizeof("audio ") - 1,
		digits_len
	},
	{ 	/* SDP owner address*/	
		"\no=",		sizeof("\no=") - 1, 
		"\ro=",		sizeof("\ro=") - 1,
		"IN IP4 ",	sizeof("IN IP4 ") - 1,
		epaddr_len
	},
	{ 	/* SDP connection info */
		"\nc=",		sizeof("\nc=") - 1, 
		"\rc=",		sizeof("\rc=") - 1,
		"IN IP4 ",	sizeof("IN IP4 ") - 1,
		epaddr_len
	},
	{ 	/* Requests headers */
		"sip:",		sizeof("sip:") - 1,
		"sip:",		sizeof("sip:") - 1, /* yes, i know.. ;) */
		"@", 		sizeof("@") - 1, 
		epaddr_len
	},
	{ 	/* SDP version header */
		"\nv=",		sizeof("\nv=") - 1,
		"\rv=",		sizeof("\rv=") - 1,
		"=", 		sizeof("=") - 1, 
		digits_len
	},

	{ 	/* Via header */
		"REGISTER",		sizeof("REGISTER") - 1,
		"\r\nv:",	sizeof("\r\nv:") - 1, /* rfc3261 "\r\n" */
		"sip:", 	sizeof("sip:") - 1,
		epaddr_len
	},
	{	/* SDP media info */
		"\nm=",		sizeof("\nm=") - 1,	
		"\rm=",		sizeof("\rm=") - 1,
		"video ",	sizeof("video ") - 1,
		digits_len
	},
	{ 	/* Content length header */
		"Call-ID:", sizeof("Call-ID:") - 1,
		"\r\nl:",	sizeof("\r\nl:") - 1,
		": ",		sizeof(": ") - 1, 
		skp_callid_len
	},
	{ 	/* Content length header */
		"SIP/2.0", sizeof("SIP/2.0") - 1,
		"\r\nl:",	sizeof("\r\nl:") - 1,
		" ",		sizeof(" ") - 1, 
		digits_len
	},

	{ 	/* Via header */
		"CSeq:",	sizeof("CSeq:") - 1,
		"\r\nv:",	sizeof("\r\nv:") - 1, 
		"REGISTER", 	sizeof("REGISTER") - 1,
		cseq_len
	},

	{ 	/* Via header */
		"CSeq:",	sizeof("CSeq:") - 1,
		"\r\nv:",	sizeof("\r\nv:") - 1,
		"INVITE", sizeof("INVITE") - 1,
		cseq_len
	},
	{ 	/* Contact header */
		"INVITE",	sizeof("INVITE") - 1,
		"\r\nv:",	sizeof("\r\nv:") - 1,
		"sip:",		sizeof("sip:") - 1,
		skp_epaddr_len
	},

	{ 	/* Contact header */
		"Route:",	sizeof("Contact:") - 1,
		"\r\nm:",	sizeof("\r\nm:") - 1,
		"sip:",		sizeof("sip:") - 1,
		skp_epaddr_len
	},
	{ 	/* Via header */
		"CSeq:",	sizeof("CSeq:") - 1,
		"\r\nv:",	sizeof("\r\nv:") - 1,
		"BYE", sizeof("BYE") - 1,
		cseq_len
	},
};
EXPORT_SYMBOL(ct_sip_hdrs);

static u_int16_t sip_srch_unused_index(void)
{
	u_int16_t i;
	
	for (i = 0; i < MAX_PORT_MAPS; i++)
	{
		if (ct_sip_data[i].used <= 0)
	   		return i;
	}

	if (isDbgAlg(IP_ALG_DBG_SIP))
	{
		printk("Warning:sip data table is full\n");
	}
	
	return 0XFFFF;
}

static void  sip_init_tbl(void)
{
	u_int16_t i;
	
	for (i = 0; i < MAX_PORT_MAPS; i++)
	{
		ct_sip_data[i].client_map_port = 0;
		ct_sip_data[i].ct_audio = NULL;
		ct_sip_data[i].ct_video = NULL;
		ct_sip_data[i].ctl_audio = NULL;
		ct_sip_data[i].ctl_video = NULL;
		ct_sip_data[i].used       = 0;
	}
}

static void sip_set_used_index(u_int16_t index)
{
	if (index >= MAX_PORT_MAPS)
	{
		return;
	}
	else
	{
		ct_sip_data[index].used = 1;
	}


}
static int skp_callid_len(const char *dptr, const char *limit, int *shift)
{
	int len = 0;	

	while (dptr <= limit &&( isdigit(*dptr)||isalpha(*dptr)||(*dptr == '@')||(*dptr == '.')))
	{
		dptr++;
		len++;
	}
	
	return len;
}

static int cseq_len(const char *dptr, const char *limit, int *shift)
{
	int len = 1;	
	return len;
} 

static int digits_len(const char *dptr, const char *limit, int *shift)
{
	int len = 0;	
	while (dptr <= limit && isdigit(*dptr))
	{
		dptr++;
		len++;
	}
	return len;
} 

/* get digits lenght, skiping blank spaces. */
static int skp_digits_len(const char *dptr, const char *limit, int *shift)
{
	for (; dptr <= limit && *dptr == ' '; dptr++)
		(*shift)++;
		
	return digits_len(dptr, limit, shift);
}


/* Simple ipaddr parser.. */
static int parse_ipaddr(const char *cp,	const char **endp, 
			uint32_t *ipaddr, const char *limit)
{
	unsigned long int val;
	int i, digit = 0;
	
	for (i = 0, *ipaddr = 0; cp <= limit && i < 4; i++)
	{
		digit = 0;
		if (!isdigit(*cp))
			break;
		
		val = simple_strtoul(cp, (char **)&cp, 10);
		if (val > 0xFF)
			return -1;
	
		((uint8_t *)ipaddr)[i] = val;	
		digit = 1;
	
		if (*cp != '.')
			break;
		cp++;
	}
	if (!digit)
		return -1;
	
	if (endp)
		*endp = cp;

	return 0;
}

/* skip ip address. returns it lenght. */
static int epaddr_len(const char *dptr, const char *limit, int *shift)
{
	const char *aux = dptr;
	uint32_t ip;
	
	if (parse_ipaddr(dptr, &dptr, &ip, limit) < 0)
	{
		if (isDbgAlg(IP_ALG_DBG_SIP))
			printk("ip: %s parse failed.!\n", dptr);
		return 0;
	}

	/* Port number */
	if (*dptr == ':')
	{
		dptr++;
		dptr += digits_len(dptr, limit, shift);
	}
	return dptr - aux;
}

/* get address lenght, skiping user info. */
static int skp_epaddr_len(const char *dptr, const char *limit, int *shift)
{
	int find =0;
	const char *tmp;
	int oldshift;
	tmp = dptr;
	oldshift = (*shift);

	for (; tmp <= limit; tmp++)
	{
		if (*tmp == '@')
		{
			find =1;
			break;
		}
		(*shift)++;	
	}

	if (1 == find)
	{
		if (*tmp == '@')
		{
			tmp++;
			(*shift)++;
			return epaddr_len(tmp, limit, shift);
		}
	}
	else
	{
		*shift = oldshift;
		return epaddr_len(dptr, limit, shift);
	}

	return  0;
}

/* Returns 0 if not found, -1 error parsing. */
int ct_sip_get_info(const char *dptr, size_t dlen, 
		unsigned int *matchoff, 
		unsigned int *matchlen,
		struct sip_header_nfo *hnfo)
{
	const char *limit, *aux, *eofl,*k = dptr;
	int shift = 0;
	const char *data = dptr;
	
	limit = data + (dlen - hnfo->lnlen);
 
	while (data <= limit)
	{
		if ((strncmp(data, hnfo->lname, hnfo->lnlen) != 0) &&
			(strncmp(data, hnfo->sname, hnfo->snlen) != 0))
		{
			data++;
			continue;
		}

		aux = ct_sip_search(hnfo->ln_str, data, hnfo->ln_strlen, 
						ct_sip_lnlen(data, limit));
		if (!aux)
		{
			if (isDbgAlg(IP_ALG_DBG_SIP))
				printk("'%s' not found in '%s'.\n", hnfo->ln_str, hnfo->lname);
			return -1;
		}
		
		aux += hnfo->ln_strlen;
		eofl = aux;
		eofl = aux + ct_sip_lnlen(eofl, limit);
		*matchlen = hnfo->match_len(aux, eofl, &shift);/*changed limit to eofl*/
		if (!*matchlen)
			return -1;

		*matchoff = (aux - k) + shift; 
		if (isDbgAlg(IP_ALG_DBG_SIP))
			printk("%s match succeeded! - len: %u\n", hnfo->lname, *matchlen);
		return 1;
	}
        
	if (isDbgAlg(IP_ALG_DBG_SIP))
		printk("%s header not found.\n", hnfo->lname);
	return 0;
}

int sip_getpkt_type(const char *dptr,size_t datalen)
{
	unsigned int ret = 0;
	unsigned int matchoff, matchlen;

	if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_CSEQREG]) > 0)
	{
		return PKT_REGISTER;	/*register packets*/;
	}
	
	if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_CSEQINVT]) > 0)
	{
		return PKT_INVITE;	/*invite packets*/;
	}

	if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_CSEQBYE]) > 0)
	{
		return PKT_BYE;	/*invite packets*/;
	}
	
	return ret;
}

static int sipGetIPPort(const char *dptr,unsigned int matchoff,unsigned int matchlen,uint32_t *pIpaddr,uint16_t *pPort)
{
	const char *end;
	const char *data;
	
	if (NULL == dptr||NULL ==pIpaddr||NULL == pPort)
		return -1;

	if (matchlen > sizeof("nnn.nnn.nnn.nnn:nnnnn"))
		return -1;

	*pIpaddr = 0;
	*pPort = 0;
	data = dptr + matchoff;
	end = data;

	if (parse_ipaddr(data, &end, pIpaddr,data + matchlen) < 0)
	{
		return -1;
	}

	
	if (end == (dptr + matchoff + matchlen))
	{
		*pPort = 0;
	}
	else
	{
		end++;
		*pPort = simple_strtoul(end, NULL, 10);
		if (*pPort  < 1024)
		{
			if (isDbgAlg(IP_ALG_DBG_SIP))
				printk("Warning:port = %d\n",*pPort);
		}			
	}

	return 0;
}


static int sipGetIP(const char *dptr,unsigned int matchoff,unsigned int matchlen,uint32_t *pIpaddr)
{
	//const char **end;
	const char *data;
	
	if (NULL == dptr||NULL ==pIpaddr)
		return -1;

	if (matchlen > sizeof("nnn.nnn.nnn.nnn"))
		return -1;

	*pIpaddr = 0;
	data = dptr + matchoff;
	
	if (parse_ipaddr(data, NULL, pIpaddr,data + matchlen) < 0)
	{
		return -1;
	}

	return 0;
}

static int sipCopyExp2Master(struct ip_ct_sip_master *ct_sip_info,struct ip_ct_sip_expect *exp_sip_info)
{
	if ((NULL == ct_sip_info)||(NULL == exp_sip_info))
	{
		return 0;
	}

	ct_sip_info->port			= exp_sip_info->port; 		
	ct_sip_info->voicePort			= exp_sip_info->voicePort;
	ct_sip_info->videoPort			= exp_sip_info->videoPort;
	ct_sip_info->voiceRPort			= exp_sip_info->voiceRPort; 
	ct_sip_info->videoRPort			= exp_sip_info->videoRPort; 
	ct_sip_info->publicIP			= exp_sip_info->publicIP; 
	ct_sip_info->privateIP			= exp_sip_info->privateIP;
	ct_sip_info->siptype			= exp_sip_info->siptype;
	ct_sip_info->invitdir			= exp_sip_info->invitdir;
	//memcpy(ct_sip_info->callid,exp_sip_info->callid,MAX_SIP_CALLID_LEN);
	return 0;
}


static int sipCopySipInfo(struct ip_ct_sip_master *ct_sip_dst_info,struct ip_ct_sip_master *ct_sip_src_info)
{
	if ((NULL == ct_sip_dst_info)||(NULL == ct_sip_src_info))
	{
		return 0;
	}

	//ct_sip_dst_info->port			= ct_sip_src_info->port; 		
	ct_sip_dst_info->voicePort	= ct_sip_src_info->voicePort;
	ct_sip_dst_info->videoPort	= ct_sip_src_info->videoPort;
	ct_sip_dst_info->voiceRPort	= ct_sip_src_info->voiceRPort; 
	ct_sip_dst_info->videoRPort	= ct_sip_src_info->videoRPort; 
	ct_sip_dst_info->publicIP	= ct_sip_src_info->publicIP; 
	ct_sip_dst_info->privateIP	= ct_sip_src_info->privateIP;
	ct_sip_dst_info->siptype	= ct_sip_src_info->siptype;
	ct_sip_dst_info->invitdir	= ct_sip_src_info->invitdir;
	//memcpy(ct_sip_dst_info->callid,ct_sip_src_info->callid,MAX_SIP_CALLID_LEN);
	return 0;
}

static int parse_Ok200(const char *dptr, size_t datalen,struct ip_ct_sip_master *ct_sip_info)
{
	uint32_t ipaddr;
	uint16_t port,audioPort=0,videoPort=0;
	int ret = 0;
	unsigned int matchoff, matchlen,sum;

	if (NULL == dptr)
		return ret;
	if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_OWNER]) > 0)
	{
		if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_CONECTION]) > 0)
		{
			sipGetIPPort(dptr,matchoff,matchlen,&ipaddr,&port);
			if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_MEDIA]) > 0)
			{
				audioPort =  simple_strtoul((dptr + matchoff), NULL, 10);
			}

			sum = matchoff + matchlen;
			if (ct_sip_get_info(dptr + sum, datalen -sum, &matchoff, &matchlen, &ct_sip_hdrs[POS_MVIDEO]) > 0)
			{
				videoPort = simple_strtoul((dptr + sum + matchoff), NULL, 10);
			}
			
			if (IP_CT_SIP_INVIT_PRI2PUB == ct_sip_info->invitdir)
			{
				ct_sip_info->publicIP 		= ipaddr;
				ct_sip_info->voiceRPort 	= audioPort;
				ct_sip_info->videoRPort 	= videoPort;
				if (isDbgAlg(IP_ALG_DBG_SIP))
					printk("\nIP_CT_SIP_INVIT_PRI2PUB:parse_Ok200:publicIP = %X,voiceRPort = %d,videoRPort = %d\n",
						ct_sip_info->publicIP,ct_sip_info->voiceRPort,ct_sip_info->videoRPort);
			}
			else
			{
				ct_sip_info->privateIP 	= ipaddr;
				ct_sip_info->voicePort 	= audioPort;
				ct_sip_info->videoPort 	= videoPort;
				if (isDbgAlg(IP_ALG_DBG_SIP))
					printk("\nIP_CT_SIP_INVIT_PUB2PRI:parse_Ok200:privateIP = %X,voicePort = %d,videoPort = %d\n",
						ct_sip_info->privateIP,ct_sip_info->voicePort,ct_sip_info->videoPort);
			}
		}
	}
	return ret;
}


static int sip_help(struct sk_buff *pskb,
		struct ip_conntrack *ct,
		enum ip_conntrack_info ctinfo)
{
	unsigned int dataoff;
	size_t datalen;
	int pkt_type;
	const char *dptr;
	const char *firstline;
	int ret = NF_ACCEPT;
	unsigned int matchoff, matchlen,sum;
	uint32_t ipaddr;
	uint16_t port,audioPort =0,videoPort = 0;
	unsigned long sip_status;
	struct ip_ct_sip_master *ct_sip_dst_info;

	int dir = CTINFO2DIR(ctinfo);
	struct ip_conntrack_expect *exp = NULL;
	struct ip_ct_sip_expect *exp_sip_info = NULL;
	struct ip_ct_sip_master *ct_sip_info =  &ct->help.ct_sip_info;
	struct ip_conntrack *master = NULL;

       /*ip_ct_refresh(ct, *pskb, sip_timeout * HZ);*/
	if (NULL != ct)
	{
		ip_ct_refresh(ct, sip_timeout * HZ);
	}
			

	dataoff = pskb->nh.iph->ihl*4 + sizeof(struct udphdr);
	if (dataoff >= pskb->len)
	{
		if (isDbgAlg(IP_ALG_DBG_SIP))
			printk("skb->len = %u\n", pskb->len);
		return NF_ACCEPT;
        }
	
	
	if ((dataoff + pskb->len - dataoff) <= skb_headlen(pskb))
		dptr = pskb->data + dataoff;
	else
	{
		printk("Copy of skbuff not supported yet.\n");
		return NF_ACCEPT;
	}

	datalen = pskb->len - dataoff;
	firstline = dptr;

	LOCK_BH(&sipbf_lock);
	/* No Data ? */ 
	if (memcmp(firstline, "REGISTER sip:", sizeof("REGISTER sip:") - 1) == 0)
	{
		if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_VIA]) > 0)
		{
			sipGetIPPort(dptr,matchoff,matchlen,&ipaddr,&port);
                        if (isDbgAlg(IP_ALG_DBG_SIP))
				printk("SIP:register port = %d\n",port);
			if ((0 == port)||(htons(port) ==ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.udp.port))
			{
				ct_sip_info->port		= port; 		
				ct_sip_info->voicePort		= 0;
				ct_sip_info->videoPort		= 0;
				ct_sip_info->voiceRPort		= 0; 
				ct_sip_info->videoRPort		= 0; 
				ct_sip_info->publicIP		= 0; 
				ct_sip_info->privateIP		= 0;
				ct_sip_info->siptype		= IP_CT_SIP_WAIT_REGREPLY;
				ct_sip_info->invitdir		= IP_CT_SIP_INVIT_IDLE;
			}
			else
			{
				exp = ip_conntrack_expect_alloc();
				if (exp == NULL)
					goto out;

				exp_sip_info = &exp->help.exp_sip_info;
				exp_sip_info->siptype = IP_CT_SIP_WAIT_REGREPLY;
				exp_sip_info->invitdir = IP_CT_SIP_INVIT_IDLE;
				exp_sip_info->port = port;
			
				exp->tuple = ct->tuplehash[IP_CT_DIR_REPLY].tuple;
				exp->mask.dst.u.udp.port = 0X0;
				exp->tuple.dst.u.udp.port = htons(port);

				exp->mask.src.ip = 0XFFFFFFFF;
				exp->mask.src.u.udp.port = 0XFFFF;
				exp->mask.src.u.udp.port = 0XFFFF;

				exp->mask.dst.protonum = 0XFFFF;
				exp->mask.dst.ip = 0X0;
				exp->mask.dst.u.udp.port = 0XFFFF;
				exp->expectfn = sip_expect;
				sipCopyExp2Master(ct_sip_info,exp_sip_info);
				ip_conntrack_expect_related(exp, ct);
			       
			}
		}
	}

	if (memcmp(firstline, "INVITE sip:", sizeof("INVITE sip:") - 1) == 0)
	{
		if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_OWNER]) > 0)
		{
			if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_CONECTION]) > 0)
			{
				sipGetIP(dptr,matchoff,matchlen,&ipaddr);
				if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_MEDIA]) > 0)
				{
					audioPort = simple_strtoul((dptr + matchoff), NULL, 10);
				}

				sum = matchoff + matchlen;
				if (ct_sip_get_info(dptr + sum, datalen -sum, &matchoff, &matchlen, &ct_sip_hdrs[POS_MVIDEO]) > 0)
				{
					videoPort = simple_strtoul((dptr + sum + matchoff), NULL, 10);
				}
					
				master = master_ct(ct);
				if (NULL != master)
				{
					if (IP_CT_DIR_ORIGINAL == dir)
					{
						ct_sip_info->invitdir =  IP_CT_SIP_INVIT_PUB2PRI;
						ct_sip_info->publicIP =  ipaddr;
						ct_sip_info->voiceRPort = audioPort;
						ct_sip_info->videoRPort = videoPort;
					}
					else
					{
						ct_sip_info->invitdir = IP_CT_SIP_INVIT_PRI2PUB;
						ct_sip_info->privateIP =  ipaddr;
						ct_sip_info->voicePort = audioPort;
						ct_sip_info->videoPort = videoPort;			
					}
				}
				else
				{
					if (IP_CT_DIR_ORIGINAL == dir)
					{
						ct_sip_info->invitdir = IP_CT_SIP_INVIT_PRI2PUB;
						ct_sip_info->privateIP =  ipaddr;
						ct_sip_info->voicePort = audioPort;
						ct_sip_info->videoPort = videoPort;
					}
					else
					{
						ct_sip_info->invitdir = IP_CT_SIP_INVIT_PUB2PRI;
						ct_sip_info->publicIP =  ipaddr;
						ct_sip_info->voiceRPort = audioPort;
						ct_sip_info->videoRPort = videoPort;
					}
				}
			}
		}	

		/*
			(1)call from pub to pri
			    a.change first line of ip address,
			(2)call from pri to pub
			    a.change ip address,
		*/
		if (IP_CT_SIP_INVIT_PRI2PUB == ct_sip_info->invitdir)
		{
			if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_VIA]) > 0)
			{
				sipGetIPPort(dptr, matchoff, matchlen, &ipaddr,&port);
				if ((0 == port)||(htons(port) ==ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.udp.port))
				{
				    ct_sip_info->port = port;	
				}
				else
				{
					exp = ip_conntrack_expect_alloc();
					if (exp == NULL)
						goto out;

					exp_sip_info = &exp->help.exp_sip_info;
					exp_sip_info->siptype = IP_CT_SIP_WAIT_INVITREPLY;
					exp_sip_info->port = port;
					ct_sip_info->port = port;
					exp_sip_info->voicePort = 0;
					exp_sip_info->videoPort = 0;
					exp->expectfn = sip_expect;

					exp->tuple = ct->tuplehash[IP_CT_DIR_REPLY].tuple;
					exp->mask.dst.u.udp.port = 0X0;
					exp->tuple.dst.u.udp.port = htons(port);
					exp->mask.src.ip = 0XFFFFFFFF;
					exp->mask.src.u.udp.port = 0XFFFF;
					exp->mask.src.u.udp.port = 0XFFFF;

					exp->mask.dst.protonum = 0XFFFF;
					exp->mask.dst.ip = 0X0;
					exp->mask.dst.u.udp.port = 0XFFFF;
					ip_conntrack_expect_related(exp, ct);
				}
			}
		}
		else
		{
			if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_INVITE]) > 0)
			{
				sipGetIPPort(dptr, matchoff, matchlen, &ipaddr,&port);
				ct_sip_info->port = port;
			}
			/*copy information to ct_sip_info of master ct */
		}
		//sipCopyExp2Master(ct_sip_info,exp_sip_info);
		master = master_ct(ct);
		if (master)
			sipCopySipInfo(&master->help.ct_sip_info,ct_sip_info);
		
		if (isDbgAlg(IP_ALG_DBG_SIP))
		{
			printk("ip_conntrack_sip:INVITE process invitdir = %d, ipaddress = %X, port = %d\n",ct_sip_info->invitdir,ipaddr,ct_sip_info->port);
			printk("remote ip = %X, audio port = %d, video port = %d,local ip address = %X, remote audio port = %d, remote video port = %d\n",
				ct_sip_info->publicIP,ct_sip_info->voicePort,
				ct_sip_info->videoPort,ct_sip_info->privateIP,
				ct_sip_info->voiceRPort,ct_sip_info->videoRPort);
	        }
	}

	if (memcmp(firstline, "SIP/2.0", sizeof("SIP/2.0") - 1) == 0)
	{
		if (ct_sip_get_info(dptr, datalen, &matchoff, &matchlen, &ct_sip_hdrs[POS_STATUS]) > 0)
		{
			sip_status = simple_strtoul((dptr + matchoff), NULL, 10);
		}
		else
			sip_status = 0;

		pkt_type = sip_getpkt_type(dptr,datalen);
		if (isDbgAlg(IP_ALG_DBG_SIP))
			printk("ip_conntrack_sip:process reply sip_status = %lu,pkt_type = %d\n",sip_status,pkt_type);	
		if (PKT_INVITE == pkt_type)
		{
			/*inviting packet's reponse*/
			if (200 == sip_status)
			{
				master = master_ct(ct);
				if (NULL != master)
				{
					ct_sip_dst_info = &master->help.ct_sip_info;
				}
				else
				{
					ct_sip_dst_info = &ct->help.ct_sip_info;
				}
				parse_Ok200(dptr,datalen,ct_sip_dst_info);
			}
		}
	}

out:	UNLOCK_BH(&sipbf_lock);
	return ret;
}


static struct ip_conntrack_helper sipext = {
	.name = "sipext",
	.flags = IP_CT_HELPER_F_REUSE_EXPECT,
	.max_expected = 8,
	.timeout = 240,
	.tuple = { .dst = { .protonum = IPPROTO_UDP } },
	.mask = { .src = { .u = { 0xFFFF } },
		  .dst = { .protonum = 0xFFFF } },
	.help = sip_help
};


static struct ip_conntrack_helper sip[MAX_PORTS];
static char sip_names[MAX_PORTS][10];

int set_expected_rtp(struct sk_buff **pskb, 
			struct ip_conntrack *ct,
			enum ip_conntrack_info ctinfo, 
			struct ip_ct_sip_master *ct_sip_info
			)
{

	const char *dptr;
	size_t datalen;
	unsigned int dataoff;
	struct ip_conntrack_expect *exp;
	struct ip_conntrack *master = master_ct(ct);
	int ret = NF_ACCEPT;


	dataoff = (*pskb)->nh.iph->ihl*4 + sizeof(struct udphdr);
	if (dataoff >= (*pskb)->len)
	{
		printk("skb->len = %u\n", (*pskb)->len);
		return ret;
        }

	
	if ((dataoff + (*pskb)->len - dataoff) <= skb_headlen(*pskb))
		dptr = (*pskb)->data + dataoff;
	else
	{
		printk("Copy of skbuff not supported yet.\n");
		return ret;
	}	
 
        if (isDbgAlg(IP_ALG_DBG_SIP))
	    printk("ip_nat_sip:set_expected_rtp\n");
	datalen =   (*pskb)->len - dataoff;



	if (isDbgAlg(IP_ALG_DBG_SIP))
		printk("ct_sip_info:publicIP = %X,localip = %X,voicePort = %d,videoPort = %d,voiceRPort = %d,videoRPort = %d\n",
		ct_sip_info->publicIP,
		ct_sip_info->privateIP,
		ct_sip_info->voicePort,
		ct_sip_info->videoPort,
		ct_sip_info->voiceRPort,
		ct_sip_info->videoRPort);

		/**/

	if ((ct_sip_info->voiceRPort > 0)&&(ct_sip_info->voicePort > 0))
	{
		exp = ip_conntrack_expect_alloc();
 		if (exp == NULL)
			return NF_DROP;
		exp->tuple.src.ip = ct_sip_info->publicIP;
		exp->tuple.src.u.udp.port = 0XFFFF;//htons(ct_sip_info->voiceRPort);
		exp->tuple.dst.protonum = IPPROTO_UDP;

		if ((ct_sip_info->invitdir == IP_CT_SIP_INVIT_PRI2PUB))
		{
			if (NULL == master)
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
			else
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
		}
		else
		{
			if (NULL == master)
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
			else
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;		
		}
		
		exp->tuple.dst.u.udp.port = htons(ct_sip_info->voicePort);
			
		exp->mask.src.ip = 0XFFFFFFFF;
		exp->mask.src.u.udp.port = 0X0;//FFFF;
		exp->mask.dst.protonum = 0XFFFF;
		exp->mask.dst.u.udp.port = 0XFFFF;
		exp->mask.dst.ip = 0XFFFFFFFF;
		exp->expectfn = sip_rtp_expect;
                if (isDbgAlg(IP_ALG_DBG_SIP))
			printk("ip_nat_sip:set_expected_rtp:audioPort\n");
		if (NULL == master)
		{
			ip_conntrack_expect_related(exp,ct);
		}
		else
		{
			ip_conntrack_expect_related(exp,master);		
		}
		ct_sip_info->siptype = IP_CT_SIP_WAIT_INVITAUDIO;
	}

	/*creat audio expect*/
	if ((ct_sip_info->voiceRPort > 0)&&(ct_sip_info->voicePort > 0))
	{
		exp = ip_conntrack_expect_alloc();
 		if (exp == NULL)
			return NF_DROP;
		exp->tuple.src.ip = ct_sip_info->publicIP;
		exp->tuple.src.u.udp.port = 0XFFFE;//htons((ct_sip_info->voiceRPort));
		exp->tuple.dst.protonum = IPPROTO_UDP;
		//exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
		if ((ct_sip_info->invitdir == IP_CT_SIP_INVIT_PRI2PUB))
		{
			if (NULL == master)
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
			else
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
		}
		else
		{
			if (NULL == master)
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
			else
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;		
		}
		
		exp->tuple.dst.u.udp.port = htons((ct_sip_info->voicePort + 1));
		exp->mask.src.ip = 0XFFFFFFFF;
		exp->mask.src.u.udp.port = 0X0;//FFFF;
		exp->mask.dst.protonum = 0XFFFF;
		exp->mask.dst.u.udp.port = 0XFFFF;
		exp->mask.dst.ip = 0XFFFFFFFF;
		exp->expectfn = sip_rtp_expect;
		if (isDbgAlg(IP_ALG_DBG_SIP))
			printk("ip_nat_sip:set_expected_rtp:audioPort\n");
		if (NULL == master)
		{
			ip_conntrack_expect_related(exp,ct);
		}
		else
		{
			ip_conntrack_expect_related(exp,master);		
		}

		ct_sip_info->siptype = IP_CT_SIP_WAIT_INVITAUDIO;

	}

	if ((ct_sip_info->videoRPort > 0)&&(ct_sip_info->videoPort > 0))
	{

		exp = ip_conntrack_expect_alloc();
 		if (exp == NULL)
			return NF_DROP;
		exp->tuple.src.ip = ct_sip_info->publicIP;
		exp->tuple.src.u.udp.port = 0XFFFD;//htons(ct_sip_info->videoRPort);
		exp->tuple.dst.protonum = IPPROTO_UDP;
		//exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
		if ((ct_sip_info->invitdir == IP_CT_SIP_INVIT_PRI2PUB))
		{
			if (NULL == master)
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
			else
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
		}
		else
		{
			if (NULL == master)
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
			else
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;		
		}
		
		exp->tuple.dst.u.udp.port = htons(ct_sip_info->videoPort);
		exp->mask.src.ip = 0XFFFFFFFF;
		exp->mask.src.u.udp.port = 0X0;//FFFF;
		exp->mask.dst.protonum = 0XFFFF;
		exp->mask.dst.u.udp.port = 0XFFFF;
		exp->mask.dst.ip = 0XFFFFFFFF;
		exp->expectfn = sip_rtp_expect;
		if (isDbgAlg(IP_ALG_DBG_SIP))	
			printk("ip_nat_sip:set_expected_rtp:videoPort\n");
		DUMP_TUPLE(&exp->tuple);
		DUMP_TUPLE(&exp->mask);
		if (NULL == master)
		{
			ip_conntrack_expect_related(exp,ct);
		}
		else
		{
			ip_conntrack_expect_related(exp,master);		
		}

		ct_sip_info->siptype = IP_CT_SIP_WAIT_INVITVIDEO;
		
	}

	if ((ct_sip_info->videoRPort > 0)&&(ct_sip_info->videoPort > 0))
	{

		exp = ip_conntrack_expect_alloc();
 		if (exp == NULL)
			return NF_DROP;
		exp->tuple.src.ip = ct_sip_info->publicIP;
		exp->tuple.src.u.udp.port = 0XFFFC;//htons(ct_sip_info->videoRPort + 1);
		exp->tuple.dst.protonum = IPPROTO_UDP;
		//exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
		if ((ct_sip_info->invitdir == IP_CT_SIP_INVIT_PRI2PUB))
		{
			if (NULL == master)
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
			else
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;
		}
		else
		{
			if (NULL == master)
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;
			else
				exp->tuple.dst.ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.ip;		
		}
		
		exp->tuple.dst.u.udp.port = htons(ct_sip_info->videoPort + 1);
			
		exp->mask.src.ip = 0XFFFFFFFF;
		exp->mask.src.u.udp.port = 0X0;//FFFF;
		exp->mask.dst.protonum = 0XFFFF;
		exp->mask.dst.u.udp.port = 0XFFFF;
		exp->mask.dst.ip = 0XFFFFFFFF;
		exp->expectfn = sip_rtp_expect;
		if (isDbgAlg(IP_ALG_DBG_SIP))	
			printk("ip_nat_sip:set_expected_rtp:videoPort\n");
		DUMP_TUPLE(&exp->tuple);
		DUMP_TUPLE(&exp->mask);
		if (NULL == master)
		{
			ip_conntrack_expect_related(exp,ct);
		}
		else
		{
			ip_conntrack_expect_related(exp,master);		
		}
		ct_sip_info->siptype = IP_CT_SIP_WAIT_INVITVIDEO;
	}

	if (((ct_sip_info->voiceRPort > 0)&&(ct_sip_info->voicePort > 0))&&((ct_sip_info->videoRPort > 0)&&(ct_sip_info->videoPort > 0)))
	{
		ct_sip_info->siptype = IP_CT_SIP_WAIT_BOTHSTREAM;
	}

	if (((ct_sip_info->voiceRPort > 0)&&(ct_sip_info->voicePort > 0))&&(!((ct_sip_info->videoRPort > 0)&&(ct_sip_info->videoPort > 0))))
		ct_sip_info->siptype = IP_CT_SIP_WAIT_INVITAUDIO;

	if ((!((ct_sip_info->voiceRPort > 0)&&(ct_sip_info->voicePort > 0)))&&((ct_sip_info->videoRPort > 0)&&(ct_sip_info->videoPort > 0)))
		ct_sip_info->siptype = IP_CT_SIP_WAIT_INVITVIDEO;

	if (!(ct_sip_info->flags&SIP_CT_INVIT_200OK))
	{
		ct_sip_info->index = sip_srch_unused_index();
		sip_set_used_index(ct_sip_info->index);
		ct_sip_info->flags = ct_sip_info->flags|SIP_CT_INVIT_200OK;
	}
	if (isDbgAlg(IP_ALG_DBG_SIP))	
		printk("ip_nat_sip:set_expected_rtp:index = %d\n",ct_sip_info->index);
	return ret;
}

static int sip_expect(struct ip_conntrack *conntrack)
{
	if (NULL == conntrack)
	{
		return 0;
	}

	conntrack->helper = &sipext;
	return 0;
}

static int sip_rtp_expect(struct ip_conntrack *conntrack)
{
	if (NULL == conntrack)
	{
		return 0;
	}

	if (isDbgAlg(IP_ALG_DBG_SIP))
		printk("Warning:sip_rtp_expect called\n");
	return 0;
}


static void fini(void)
{
	int i = 0;
	for (; i < ports_c; i++)
	{
		printk("unregistering helper for port %d\n", ports[i]);
		ip_conntrack_helper_unregister(&sip[i]);
	} 
}

static int __init init(void)
{
	int i, ret;
	char *tmpname;

       sip_init_tbl();
	if (ports_c == 0)
		ports[ports_c++] = SIP_PORT;

	for (i = 0; i < ports_c; i++) {
		/* Create helper structure */
		memset(&sip[i], 0, sizeof(struct ip_conntrack_helper));
		sip[i].flags = IP_CT_HELPER_F_REUSE_EXPECT;
		sip[i].tuple.dst.protonum = IPPROTO_UDP;
		sip[i].tuple.src.u.udp.port = htons(ports[i]);
		sip[i].mask.src.u.udp.port = 0xFFFF;
		sip[i].mask.dst.protonum = 0xFF;
		sip[i].max_expected = 8;
		sip[i].timeout = 3 * 60; /* 3 minutes */
		sip[i].me = THIS_MODULE;
		sip[i].help = sip_help;

		tmpname = &sip_names[i][0];
		if (ports[i] == SIP_PORT)
			sprintf(tmpname, "sip");
		else
			sprintf(tmpname, "sip-%d", i);
		sip[i].name = tmpname;

		ret=ip_conntrack_helper_register(&sip[i]);
		if (ret)
		{
			printk("ERROR registering helper for port %d\n",ports[i]);
			fini();
			return(ret);
		}
	}
	return(0);
}
PROVIDES_CONNTRACK(sip);
EXPORT_SYMBOL(sipbf_lock);
module_init(init);
module_exit(fini);
