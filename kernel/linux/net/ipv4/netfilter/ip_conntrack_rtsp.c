/*
 * RTSP extension for IP connection tracking
 * (C) 2003 by Tom Marshall <tmarshall@real.com>
 * based on ip_conntrack_irc.c
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * Module load syntax:
 *   insmod ip_conntrack_rtsp.o ports=port1,port2,...port<MAX_PORTS>
 *                              max_outstanding=n setup_timeout=secs
 *
 * If no ports are specified, the default will be port 554.
 *
 * With max_outstanding you can define the maximum number of not yet
 * answered SETUP requests per RTSP session (default 8).
 * With setup_timeout you can specify how long the system waits for
 * an expected data channel (default 300 seconds).
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/checksum.h>
#include <net/tcp.h>

#include <linux/netfilter_ipv4/lockhelp.h>
#include <linux/netfilter_ipv4/ip_nat_rtsp.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_conntrack_rtsp.h>

#include <linux/ctype.h>
#define NF_NEED_STRNCASECMP
#define NF_NEED_STRTOU16
#define NF_NEED_STRTOU32
#define NF_NEED_NEXTLINE
#include <linux/netfilter_helpers.h>
#define NF_NEED_MIME_NEXTLINE
#include <linux/netfilter_mime.h>

#define MAX_SIMUL_SETUP 8 /* XXX: use max_outstanding */

/*
 * To enable debugging, replace the line below with #define IP_NF_RTSP_DEBUG 1
 */
#undef IP_NF_RTSP_DEBUG 
#define INFOP(args...) printk(KERN_INFO args)
#ifdef IP_NF_RTSP_DEBUG
#define DEBUGP(args...) printk(KERN_DEBUG "%s:%s ", __FILE__, __FUNCTION__); \
                        printk(args)
#else
#define DEBUGP(args...)
#endif

#define MAX_PORTS 8
static int ports[MAX_PORTS];
static int num_ports = 0;
static int max_outstanding = 8;
static unsigned int setup_timeout = 300;

/* This is slow, but it's simple. --RR */
static char rtsp_buffer[65536];

MODULE_AUTHOR("Tom Marshall <tmarshall@real.com>");
MODULE_DESCRIPTION("RTSP connection tracking module");
MODULE_LICENSE("GPL");
#ifdef MODULE_PARM
MODULE_PARM(ports, "1-" __MODULE_STRING(MAX_PORTS) "i");
MODULE_PARM_DESC(ports, "port numbers of RTSP servers");
MODULE_PARM(max_outstanding, "i");
MODULE_PARM_DESC(max_outstanding, "max number of outstanding SETUP requests per RTSP session");
//MODULE_PARM(setup_timeout, "i");
//MODULE_PARM_DESC(setup_timeout, "timeout on for unestablished data channels");
#endif

DECLARE_LOCK(ip_rtsp_lock);
struct module* ip_conntrack_rtsp = THIS_MODULE;

/*
 * Max mappings we will allow for one RTSP connection (for RTP, the number
 * of allocated ports is twice this value).  Note that SMIL burns a lot of
 * ports so keep this reasonably high.  If this is too low, you will see a
 * lot of "no free client map entries" messages.
 */
#define MAX_PORT_MAPS 16
static u_int16_t g_tr_port = 7000;

#define PAUSE_TIMEOUT      (5 * HZ)
#define RTSP_PAUSE_TIMEOUT (6 * HZ)

/*** default port list was here in the masq code: 554, 3030, 4040 ***/

#define SKIP_WSPACE(ptr,len,off) while(off < len && isspace(*(ptr+off))) { off++; }
/*
 * Structure to hold the mappings from client to NAT vice versa. If we
 * mangle UDP ports in the outgoing SETUP message, we must properly
 * mangle them in the return direction so that the client will
 * process the packets appropriately.
 */
struct _rtsp_data_ports {
    u_int32_t           client_ip;
    u_int16_t           client_tcp_port;
    u_int16_t           client_udp_lo;
    u_int16_t           client_udp_hi;
    portblock_t         pbtype;
    u_int16_t           nat_udp_lo;
    u_int16_t           nat_udp_hi;
    struct timer_list   pause_timeout;
    struct ip_conntrack *ct_lo;
    struct ip_conntrack *ct_hi;
    int                 timeout_active;
    int                 in_use;
} rtsp_data_ports[MAX_PORT_MAPS];

static u_int16_t rtsp_nat_to_client_pmap(u_int16_t nat_port);

static void
save_ct(struct ip_conntrack *ct)
{
    int i    = 0;
    struct ip_conntrack_tuple *tp = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;

    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        if (!rtsp_data_ports[i].in_use)
        {
            continue;
        }
        if (rtsp_data_ports[i].nat_udp_lo == ntohs((tp)->dst.u.all))
        {
            rtsp_data_ports[i].ct_lo = ct;
            break;
        }
        else if (rtsp_data_ports[i].nat_udp_hi == ntohs((tp)->dst.u.all))
        {
            rtsp_data_ports[i].ct_hi = ct;
            break;
        }
    }
}

static void
rtsp_pause_timeout(unsigned long data)
{
    int    index = (int)data;
    struct _rtsp_data_ports *rtsp_data = &rtsp_data_ports[index];
    struct ip_conntrack *ct_lo = rtsp_data->ct_lo;

    if (rtsp_data->in_use) {
        rtsp_data->pause_timeout.expires = jiffies + PAUSE_TIMEOUT;
        rtsp_data->pause_timeout.function = rtsp_pause_timeout;
        rtsp_data->pause_timeout.data = data;
        rtsp_data->timeout_active = 1;
        ip_ct_refresh(ct_lo, RTSP_PAUSE_TIMEOUT);
        add_timer(&rtsp_data->pause_timeout);
    }
}

static void
ip_conntrack_rtsp_proc_play(struct ip_conntrack *ct, const struct iphdr *iph)
{
    int i    = 0;
    struct tcphdr *tcph = (void *)iph + iph->ihl * 4;

    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        if (!rtsp_data_ports[i].in_use)
        {
            continue;
        }
        DEBUGP("Searching client info IP %u.%u.%u.%u->%hu PORTS (%hu-%hu)\n",
                NIPQUAD(iph->saddr), tcph->source, rtsp_data_ports[i].client_udp_lo,
                rtsp_data_ports[i].client_udp_hi);
        if ((rtsp_data_ports[i].client_ip == iph->saddr) &&
            (rtsp_data_ports[i].client_tcp_port == tcph->source))
        {
            DEBUGP("Found client info SRC IP %u.%u.%u.%u TCP PORT %hu UDP PORTS (%hu-%hu)\n",
                    NIPQUAD(iph->saddr), tcph->source, rtsp_data_ports[i].client_udp_lo,
                    rtsp_data_ports[i].client_udp_hi);
            if (rtsp_data_ports[i].timeout_active)
            {
                del_timer(&rtsp_data_ports[i].pause_timeout);
                rtsp_data_ports[i].timeout_active = 0;
            }
        }
    }
}

static void
ip_conntrack_rtsp_proc_pause(struct ip_conntrack *ct, const struct iphdr *iph)
{
    int i    = 0;
    struct tcphdr *tcph = (void *)iph + iph->ihl * 4;
    struct ip_conntrack_tuple *tp_lo;
    struct ip_conntrack_tuple *tp_hi;
    struct ip_conntrack *ct_lo;
    struct ip_conntrack *ct_hi;

    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        if (!rtsp_data_ports[i].in_use)
        {
            continue;
        }
        DEBUGP("Searching client info IP %u.%u.%u.%u->%hu PORTS (%hu-%hu)\n",
                NIPQUAD(iph->saddr), tcph->source, rtsp_data_ports[i].client_udp_lo,
                rtsp_data_ports[i].client_udp_hi);
        if ((rtsp_data_ports[i].client_ip == iph->saddr) &&
            (rtsp_data_ports[i].client_tcp_port == tcph->source))
        {
            DEBUGP("Found client info SRC IP %u.%u.%u.%u TCP PORT %hu UDP PORTS (%hu-%hu)\n",
                    NIPQUAD(iph->saddr), tcph->source, rtsp_data_ports[i].client_udp_lo,
                    rtsp_data_ports[i].client_udp_hi);
            if (rtsp_data_ports[i].timeout_active != 0 ||
                rtsp_data_ports[i].ct_lo == NULL)
            {
                break;
            }
            rtsp_data_ports[i].pause_timeout.expires = jiffies + PAUSE_TIMEOUT;
            rtsp_data_ports[i].pause_timeout.function = rtsp_pause_timeout;
            rtsp_data_ports[i].pause_timeout.data = (unsigned long)i;
            add_timer(&rtsp_data_ports[i].pause_timeout);
            rtsp_data_ports[i].timeout_active = 1;
            rtsp_data_ports[i].ct_lo = ct;
            tp_lo = &ct_lo->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
            tp_hi = &ct_hi->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
            ip_ct_refresh(ct, RTSP_PAUSE_TIMEOUT);
        }
    }
}

static int
rtp_expect(struct ip_conntrack *ct)
{
    u_int16_t nat_port = ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u.udp.port;
    u_int16_t orig_port = 0;
    DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
    DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);
    orig_port = rtsp_nat_to_client_pmap(nat_port);
    if (orig_port)
    {
        ct->nat.rtsp_info.orig_port = orig_port;
    } else {
        return NF_DROP;
    }
    DEBUGP("UDP client port %hu\n", ct->nat.rtsp_info.orig_port);
    save_ct(ct);

    return NF_ACCEPT;
}

/*
 * Maps client ports that are overlapping with other client UDP transport to
 * new NAT ports that will be tracked and converted back to client assigned
 * UDP ports.
 * Return (N/A)
 */
static int
rtsp_client_to_nat_pmap(struct ip_ct_rtsp_expect *prtspexp, const struct iphdr *iph,
                        struct ip_conntrack *ct)
{
    int i  = 0;
    int rc = 0;
    struct tcphdr *tcph   = (void *)iph + iph->ihl * 4;

    DEBUGP("IP %u.%u.%u.%u->%u.%u.%u.%u PORTS (%hu-%hu)\n", NIPQUAD(iph->saddr),
           NIPQUAD(iph->daddr), tcph->source, tcph->dest);

    for (i = 0; i < MAX_PORT_MAPS; i++) {
        if (rtsp_data_ports[i].in_use) {
            DEBUGP("Index %d in_use flag %d IP %u.%u.%u.%u CLIENT %hu-%hu NAT %hu-%hu\n", i,
                   rtsp_data_ports[i].in_use, NIPQUAD(rtsp_data_ports[i].client_ip),
                   rtsp_data_ports[i].client_udp_lo, rtsp_data_ports[i].client_udp_hi,
                   rtsp_data_ports[i].nat_udp_lo, rtsp_data_ports[i].nat_udp_hi);
            if (ntohl(iph->saddr) == rtsp_data_ports[i].client_ip &&
                ntohs(tcph->source) == rtsp_data_ports[i].client_tcp_port &&
                ntohs(prtspexp->loport) == rtsp_data_ports[i].client_udp_lo &&
                ntohs(prtspexp->hiport) == rtsp_data_ports[i].client_udp_hi)
            {
                prtspexp->loport  = rtsp_data_ports[i].nat_udp_lo;
                prtspexp->hiport  = rtsp_data_ports[i].nat_udp_hi;
                return rc = 2;
            }
            continue;
        }
        rtsp_data_ports[i].client_ip       = ntohl(iph->saddr);
        rtsp_data_ports[i].client_tcp_port = ntohs(tcph->source);
        rtsp_data_ports[i].client_udp_lo   = ntohs(prtspexp->loport);
        rtsp_data_ports[i].client_udp_hi   = ntohs(prtspexp->hiport);
        rtsp_data_ports[i].pbtype          = prtspexp->pbtype;
        rtsp_data_ports[i].in_use          = 1;
        init_timer(&rtsp_data_ports[i].pause_timeout);
        DEBUGP("Mapped at index %d ORIGINAL PORTS %hu-%hu\n", i,
               ntohs(prtspexp->loport), ntohs(prtspexp->hiport));
        prtspexp->loport  = rtsp_data_ports[i].nat_udp_lo = g_tr_port++;
        prtspexp->hiport  = rtsp_data_ports[i].nat_udp_hi = g_tr_port++;
        DEBUGP("NEW PORTS %hu-%hu\n", ntohs(prtspexp->loport), ntohs(prtspexp->hiport));
        return rc = 1;
    }
    return rc;
}

/*
 * Performs NAT to client port mapping. Incoming UDP ports are looked up and
 * appropriate client ports are extracted from the table and returned.
 * Return client_udp_port or 0 when no matches found.
 */
static u_int16_t
rtsp_nat_to_client_pmap(u_int16_t nat_port)
{
    int           i       = 0;
    u_int16_t     tr_port = 0;

    for (i = 0; i < MAX_PORT_MAPS; i++) {
        if (!rtsp_data_ports[i].in_use) {
            continue;
        }
        /*
         * Check if the UDP ports match any of our NAT ports and return
         * the client UDP ports.
         */
        DEBUGP("Searching at index %d NAT_PORT %hu CLIENT PORTS (%hu-%hu)\n", i,
               ntohs(nat_port), rtsp_data_ports[i].client_udp_lo,
               rtsp_data_ports[i].client_udp_hi);
        if (ntohs(nat_port) == rtsp_data_ports[i].nat_udp_lo ||
            ntohs(nat_port) == rtsp_data_ports[i].client_udp_lo) {
            tr_port = rtsp_data_ports[i].client_udp_lo;
            DEBUGP("Found at index %d NAT_PORT %hu CLIENT PORTS (%hu-%hu) tr_port %hu\n", i,
                   nat_port, rtsp_data_ports[i].client_udp_lo,
                   rtsp_data_ports[i].client_udp_hi, tr_port);
        } else if (ntohs(nat_port) == rtsp_data_ports[i].nat_udp_hi ||
                   ntohs(nat_port) == rtsp_data_ports[i].client_udp_hi) {
            tr_port = rtsp_data_ports[i].client_udp_hi;
            DEBUGP("Found at index %d NAT_PORT %hu CLIENT PORTS %hu-%hu tr_port %hu\n", i,
                   nat_port, rtsp_data_ports[i].client_udp_lo,
                   rtsp_data_ports[i].client_udp_hi, tr_port);
            return tr_port;
        }
    }
    return tr_port;
}

static void
ip_conntrack_rtsp_proc_teardown(struct iphdr *iph)
{
    int i    = 0;
    struct tcphdr *tcph = (void *)iph + iph->ihl * 4;

    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        if (!rtsp_data_ports[i].in_use)
        {
            continue;
        }
        DEBUGP("Searching client info IP %u.%u.%u.%u->%hu PORTS (%hu-%hu)\n",
                NIPQUAD(iph->saddr), tcph->source, rtsp_data_ports[i].client_udp_lo,
                rtsp_data_ports[i].client_udp_hi);
        if ((rtsp_data_ports[i].client_ip == iph->saddr) &&
            (rtsp_data_ports[i].client_tcp_port == tcph->source))
        {
            DEBUGP("Found client info SRC IP %u.%u.%u.%u TCP PORT %hu UDP PORTS (%hu-%hu)\n",
                    NIPQUAD(iph->saddr), tcph->source, rtsp_data_ports[i].client_udp_lo,
                    rtsp_data_ports[i].client_udp_hi);
            if (rtsp_data_ports[i].timeout_active)
            {
                del_timer(&rtsp_data_ports[i].pause_timeout);
                rtsp_data_ports[i].timeout_active = 0;
            }
            memset(&rtsp_data_ports[i], 0, sizeof(struct _rtsp_data_ports));
            rtsp_data_ports[i].in_use = 0;
            //break;
        }
    }
}

static void *
find_char(void *str, int ch, size_t len)
{
    unsigned char *pStr = NULL;
    if (len != 0) {
        pStr = str;
    }
    do {
        if (*pStr++ == ch) {
            return ((void *)(pStr - 1));
        }
    } while (--len != 0);
    return (NULL);
}

/*
 * Parse an RTSP packet.
 *
 * Returns zero if parsing failed.
 *
 * Parameters:
 *  IN      ptcp        tcp data pointer
 *  IN      tcplen      tcp data len
 *  IN/OUT  ptcpoff     points to current tcp offset
 *  OUT     phdrsoff    set to offset of rtsp headers
 *  OUT     phdrslen    set to length of rtsp headers
 *  OUT     pcseqoff    set to offset of CSeq header
 *  OUT     pcseqlen    set to length of CSeq header
 */
static int
rtsp_parse_message(char* ptcp, uint tcplen, uint* ptcpoff,
                   uint* phdrsoff, uint* phdrslen,
                   uint* pcseqoff, uint* pcseqlen)
{
    uint    entitylen = 0;
    uint    lineoff;
    uint    linelen;

    if (!nf_nextline(ptcp, tcplen, ptcpoff, &lineoff, &linelen))
    {
        return 0;
    }

    *phdrsoff = *ptcpoff;
    while (nf_mime_nextline(ptcp, tcplen, ptcpoff, &lineoff, &linelen))
    {
        if (linelen == 0)
        {
            if (entitylen > 0)
            {
                *ptcpoff += min(entitylen, tcplen - *ptcpoff);
            }
            break;
        }
        if (lineoff+linelen > tcplen)
        {
            INFOP("!! overrun !!\n");
            break;
        }

        if (nf_strncasecmp(ptcp+lineoff, "CSeq:", 5) == 0)
        {
            *pcseqoff = lineoff;
            *pcseqlen = linelen;
        }
        if (nf_strncasecmp(ptcp+lineoff, "Content-Length:", 15) == 0)
        {
            uint off = lineoff+15;
            SKIP_WSPACE(ptcp+lineoff, linelen, off);
            nf_strtou32(ptcp+off, &entitylen);
        }
    }
    *phdrslen = (*ptcpoff) - (*phdrsoff);

    return 1;
}

/*
 * Find lo/hi client ports (if any) in transport header
 * In:
 *   ptcp, tcplen = packet
 *   tranoff, tranlen = buffer to search
 *
 * Out:
 *   pport_lo, pport_hi = lo/hi ports (host endian)
 *
 * Returns nonzero if any client ports found
 *
 * Note: it is valid (and expected) for the client to request multiple
 * transports, so we need to parse the entire line.
 */
static int
rtsp_parse_transport(char* ptran, uint tranlen,
                     struct ip_ct_rtsp_expect* prtspexp)
{
    int     rc = 0;
    uint    off = 0;

    if (tranlen < 10 || !iseol(ptran[tranlen-1]) ||
        nf_strncasecmp(ptran, "Transport:", 10) != 0)
    {
        INFOP("sanity check failed\n");
        return 0;
    }
    DEBUGP("tran='%.*s'\n", (int)tranlen, ptran);
    off += 10;
    SKIP_WSPACE(ptran, tranlen, off);

    /* Transport: tran;field;field=val,tran;field;field=val,... */
    while (off < tranlen)
    {
        const char* pparamend;
        uint        nextparamoff;

        pparamend = find_char(ptran+off, ',', tranlen-off);
        pparamend = (pparamend == NULL) ? ptran+tranlen : pparamend+1;
        nextparamoff = pparamend-ptran;

        while (off < nextparamoff)
        {
            const char* pfieldend;
            uint        nextfieldoff;

            pfieldend = find_char(ptran+off, ';', nextparamoff-off);
            nextfieldoff = (pfieldend == NULL) ? nextparamoff : pfieldend-ptran+1;

            if (strncmp(ptran+off, "client_port=", 12) == 0)
            {
                u_int16_t   port;
                uint        numlen;

                off += 12;
                numlen = nf_strtou16(ptran+off, &port);
                off += numlen;
                if (prtspexp->loport != 0 && prtspexp->loport != port)
                {
                    DEBUGP("multiple ports found, port %hu ignored\n", port);
                }
                else
                {
                    prtspexp->loport = prtspexp->hiport = port;
                    DEBUGP("DASH or SLASH 0x%x\n", ptran[off]);
                    if (ptran[off] == '-')
                    {
                        off++;
                        numlen = nf_strtou16(ptran+off, &port);
                        off += numlen;
                        prtspexp->pbtype = pb_range;
                        prtspexp->hiport = port;

                        // If we have a range, assume rtp:
                        // loport must be even, hiport must be loport+1
                        if ((prtspexp->loport & 0x0001) != 0 ||
                            prtspexp->hiport != prtspexp->loport+1)
                        {
                            DEBUGP("incorrect range: %hu-%hu, correcting\n",
                                   prtspexp->loport, prtspexp->hiport);
                            prtspexp->loport &= 0xfffe;
                            prtspexp->hiport = prtspexp->loport+1;
                        }
                    }
                    else if (ptran[off] == '/')
                    {
                        off++;
                        numlen = nf_strtou16(ptran+off, &port);
                        off += numlen;
                        prtspexp->pbtype = pb_discon;
                        prtspexp->hiport = port;
                    }
                    rc = 1;
                }
            }

            /*
             * Note we don't look for the destination parameter here.
             * If we are using NAT, the NAT module will handle it.  If not,
             * and the client is sending packets elsewhere, the expectation
             * will quietly time out.
             */

            off = nextfieldoff;
        }

        off = nextparamoff;
    }

    return rc;
}

/*** conntrack functions ***/

/* outbound packet: client->server */
static int
help_out(struct iphdr* iph, char* pdata, size_t datalen,
                struct ip_conntrack* ct, enum ip_conntrack_info ctinfo)
{
    int dir = CTINFO2DIR(ctinfo);   /* = IP_CT_DIR_ORIGINAL */
    uint    dataoff = 0;
    struct tcphdr *tcph = (void *)iph + iph->ihl * 4;

    struct ip_conntrack_expect exp;

    while (dataoff < datalen)
    {
        uint    cmdoff = dataoff;
        uint    hdrsoff = 0;
        uint    hdrslen = 0;
        uint    cseqoff = 0;
        uint    cseqlen = 0;
        uint    lineoff = 0;
        uint    linelen = 0;
        uint    off;
        int     rc;
        uint    port = 0;
        struct  ip_conntrack_expect *new_exp = NULL;
        int     ret = 0;

        if (!rtsp_parse_message(pdata, datalen, &dataoff,
                                &hdrsoff, &hdrslen,
                                &cseqoff, &cseqlen))
        {
            break;      /* not a valid message */
        }

        if (strncmp(pdata+cmdoff, "PLAY ", 5) == 0)
        {
            ip_conntrack_rtsp_proc_play(ct, iph);
            continue;
        }

        if (strncmp(pdata+cmdoff, "PAUSE ", 6) == 0)
        {
            ip_conntrack_rtsp_proc_pause(ct, iph);
            continue;
        }

        if (strncmp(pdata+cmdoff, "TEARDOWN ", 6) == 0)
        {
            ip_conntrack_rtsp_proc_teardown(iph);   /* TEARDOWN message */
            continue;
        }

        if (strncmp(pdata+cmdoff, "SETUP ", 6) != 0)
        {
            continue;   /* not a SETUP message */
        }
        DEBUGP("found a setup message\n");

        memset(&exp, 0, sizeof(exp));

        off = 0;
        
        while (nf_mime_nextline(pdata+hdrsoff, hdrslen, &off,
                                &lineoff, &linelen))
        {
            if (linelen == 0)
            {
                break;
            }
            if (off > hdrsoff+hdrslen)
            {
                INFOP("!! overrun !!");
                break;
            }

            if (nf_strncasecmp(pdata+hdrsoff+lineoff, "Transport:", 10) == 0)
            {
                rtsp_parse_transport(pdata+hdrsoff+lineoff, linelen,
                                     &exp.help.exp_rtsp_info);
            }
        }

        if (exp.help.exp_rtsp_info.loport == 0)
        {
            DEBUGP("no udp transports found\n");
            continue;   /* no udp transports found */
        }

        DEBUGP("udp transport found, ports=(%d,%hu,%hu)\n",
              (int)exp.help.exp_rtsp_info.pbtype,
              exp.help.exp_rtsp_info.loport,
              exp.help.exp_rtsp_info.hiport);

        LOCK_BH(&ip_rtsp_lock);
       /*
         * Translate the original ports to the NAT ports and note them
         * down to translate back in the return direction.
         */
        if (!(ret = rtsp_client_to_nat_pmap(&exp.help.exp_rtsp_info, iph, ct)))
        {
            DEBUGP("Dropping the packet. No more space in the mapping table\n");
            UNLOCK_BH(&ip_rtsp_lock);
            return NF_DROP;
        }
        port = exp.help.exp_rtsp_info.loport;
        while (port <= exp.help.exp_rtsp_info.hiport) {
            /*
             * Allocate expectation for tracking this connection
             */
            new_exp = ip_conntrack_expect_alloc();
            if (!new_exp) {
                INFOP("Failed to get a new expectation entry\n");
                UNLOCK_BH(&ip_rtsp_lock);
                return NF_DROP;
            }
            memcpy(new_exp, &exp, sizeof(struct ip_conntrack_expect));
            new_exp->seq = ntohl(tcph->seq) + hdrsoff; /* mark all the headers */
            new_exp->help.exp_rtsp_info.len = hdrslen;

            DEBUGP("Adding UDP port %hu,%hu\n", htons(port), ntohs(port));

            new_exp->tuple = ct->tuplehash[!dir].tuple;
            if (ret == 2) {
                    new_exp->tuple.dst.u.udp.port = htons(g_tr_port);
                    g_tr_port++;
            } else
                new_exp->tuple.dst.u.udp.port = htons(port);
            new_exp->tuple.dst.protonum = IPPROTO_UDP;
            new_exp->mask.src.ip  = 0xffffffff;
            new_exp->mask.dst.ip  = 0xffffffff;
            //new_exp->mask.dst.u.udp.port  = (exp.help.exp_rtsp_info.pbtype == pb_range) ? 0xfffe : 0xffff;
            new_exp->mask.dst.u.udp.port  = 0xffff;
            new_exp->expectfn = rtp_expect;
            new_exp->mask.dst.protonum  = 0xffff;

            DEBUGP("expect_related %u.%u.%u.%u:%u-%u.%u.%u.%u:%u\n",
                    NIPQUAD(new_exp->tuple.src.ip),
                    ntohs(new_exp->tuple.src.u.tcp.port),
                    NIPQUAD(new_exp->tuple.dst.ip),
                    ntohs(new_exp->tuple.dst.u.tcp.port));

            /* pass the request off to the nat helper */
            rc = ip_conntrack_expect_related(new_exp, ct);
            if (rc == 0)
            {
                DEBUGP("ip_conntrack_expect_related succeeded loport\n");
            }
            else
            {
                DEBUGP("ip_conntrack_expect_related loport failed (%d)\n", rc);
            }
            port++;
        }
        UNLOCK_BH(&ip_rtsp_lock);
    }

    return NF_ACCEPT;
}

/* inbound packet: server->client */
static int
help_in(struct tcphdr* tcph, char* pdata, size_t datalen,
                struct ip_conntrack* ct, enum ip_conntrack_info ctinfo)
{
    return NF_ACCEPT;
}

static int
help(struct sk_buff* skb,
                struct ip_conntrack* ct, enum ip_conntrack_info ctinfo)
{
    uint dataoff;
    struct iphdr *iph = skb->nh.iph;
    struct tcphdr tcph;
    char* data;
    uint datalen;

    /* Until there's been traffic both ways, don't look in packets. */
    if (ctinfo != IP_CT_ESTABLISHED && ctinfo != IP_CT_ESTABLISHED + IP_CT_IS_REPLY)
    {
        DEBUGP("conntrackinfo = %u\n", ctinfo);
        return NF_ACCEPT;
    }

    /* Not whole TCP header? */
    if (skb_copy_bits(skb, skb->nh.iph->ihl*4, &tcph, sizeof(tcph)) != 0)
    {
        return NF_ACCEPT;
    }

    /* No data? */
    dataoff = skb->nh.iph->ihl*4 + tcph.doff*4;
    if ( skb->nh.iph->ihl*4 + tcph.doff*4 >= skb->len)
    {
        return NF_ACCEPT;
    }

    LOCK_BH(&ip_rtsp_lock);
    skb_copy_bits(skb, dataoff, rtsp_buffer, skb->len - dataoff);
    data = rtsp_buffer;
    datalen = skb->len - dataoff;
    switch (CTINFO2DIR(ctinfo))
    {
    case IP_CT_DIR_ORIGINAL:
        help_out(iph, data, datalen, ct, ctinfo);
        break;
    case IP_CT_DIR_REPLY:
        help_in(&tcph, data, datalen, ct, ctinfo);
        break;
    default:
        /* oops */
        break;
    }
    UNLOCK_BH(&ip_rtsp_lock);

    return NF_ACCEPT;
}

static struct ip_conntrack_helper rtsp_helpers[MAX_PORTS];
static char rtsp_names[MAX_PORTS][10];

static void
fini(void)
{
    int i;
    for (i = 0; i < num_ports; i++)
    {
        DEBUGP("unregistering port %d\n", ports[i]);
        ip_conntrack_helper_unregister(&rtsp_helpers[i]);
    }
    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        if (!rtsp_data_ports[i].in_use)
        {
            continue;
        }
        if (rtsp_data_ports[i].timeout_active == 1) {
            del_timer(&rtsp_data_ports[i].pause_timeout);
        }
    }
}

static int __init
init(void)
{
    int i, ret;
    struct ip_conntrack_helper *hlpr;
    char *tmpname;

    printk("ip_conntrack_rtsp v" IP_NF_RTSP_VERSION " loading\n");

    if (max_outstanding < 1)
    {
        printk("ip_conntrack_rtsp: max_outstanding must be a positive integer\n");
        return -EBUSY;
    }
    if (setup_timeout < 0)
    {
        printk("ip_conntrack_rtsp: setup_timeout must be a positive integer\n");
        return -EBUSY;
    }

    /* If no port given, default to standard rtsp port */
    if (ports[0] == 0)
    {
        ports[0] = RTSP_PORT;
    }

    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        memset(&rtsp_data_ports[i], 0, sizeof(struct _rtsp_data_ports));
        rtsp_data_ports[i].in_use     = 0;
    }

    for (i = 0; (i < MAX_PORTS) && ports[i]; i++)
    {
        hlpr = &rtsp_helpers[i];
        memset(hlpr, 0, sizeof(struct ip_conntrack_helper));
        hlpr->tuple.src.u.tcp.port = htons(ports[i]);
        hlpr->tuple.dst.protonum = IPPROTO_TCP;
        hlpr->mask.src.u.tcp.port = 0xFFFF;
        hlpr->mask.dst.protonum = 0xFFFF;
        hlpr->max_expected = max_outstanding;
        hlpr->timeout = 0;
        hlpr->flags = IP_CT_HELPER_F_REUSE_EXPECT;
        hlpr->me = ip_conntrack_rtsp;
        hlpr->help = help;

        tmpname = &rtsp_names[i][0];
        if (ports[i] == RTSP_PORT)
        {
            sprintf(tmpname, "rtsp");
        }
        else
        {
            sprintf(tmpname, "rtsp-%d", i);
        }
        hlpr->name = tmpname;

        DEBUGP("port #%d: %d\n", i, ports[i]);

        ret = ip_conntrack_helper_register(hlpr);

        if (ret)
        {
            printk("ip_conntrack_rtsp: ERROR registering port %d\n", ports[i]);
            fini();
            return -EBUSY;
        }
        num_ports++;
    }
    return 0;
}

PROVIDES_CONNTRACK(rtsp);
EXPORT_SYMBOL(ip_rtsp_lock);

module_init(init);
module_exit(fini);
