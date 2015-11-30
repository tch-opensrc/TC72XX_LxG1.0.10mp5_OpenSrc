/*****************************************************************************
 * Copyright (C) 2004,2005,2006,2007,2008 Katalix Systems Ltd
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <errno.h>
#include <syscall.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/utsname.h>

#include <wait.h>

#include "usl.h"
#include "l2tp_private.h"

extern int l2tp_config_parse(FILE **fp);

#define L2TP_PID_FILENAME	"/var/run/openl2tpd.pid"
#define L2TP_CONFIG_FILENAME	"/etc/openl2tpd.conf"

/* Patch information.
 * Each patch adds its version number to this array.
 */
static int l2tp_installed_patches[] = {
	0,			/* end of list */
};

/* Private variables */

#ifdef L2TP_FEATURE_LOCAL_CONF_FILE
static char			*l2tp_opt_config_filename = NULL;
#endif
#ifdef L2TP_FEATURE_LOCAL_STAT_FILE
static int			l2tp_opt_write_status_files;
#endif
static pid_t			l2tp_my_pid;
static int			l2tp_rand_fd = -1;
#ifdef L2TP_DMALLOC
static unsigned long 		l2tp_dmalloc_mark;
#endif

int				l2tp_opt_nodaemon;
uint32_t			l2tp_opt_trace_flags;
int				l2tp_opt_debug;
int				l2tp_opt_udp_port;
int				l2tp_opt_flags;
int				l2tp_opt_remote_rpc;
int				l2tp_opt_log_facility = LOG_DAEMON;
int				l2tp_opt_throttle_ppp_setup_rate;
void		 		*l2tp_sig_notifier;
uint16_t			l2tp_firmware_revision;
char				*l2tp_kernel_version;
char				*l2tp_cpu_name;

#ifdef BRCM_L2TP_SUPPORT
//#define NAMELEN 64
static char iuserid[NAMELEN]="";
static char ipassword[NAMELEN]="";
static char iremote[NAMELEN]="";
char devname[NAMELEN]="";
#endif

struct l2tp_stats		l2tp_stats;

//static struct l2tp_api_system_msg_data system_config;
//static struct l2tp_api_peer_profile_msg_data peer_profile;
//static struct l2tp_api_tunnel_profile_msg_data tunnel_profile;
//static struct l2tp_api_session_profile_msg_data session_profile;
static struct l2tp_api_ppp_profile_msg_data ppp_profile;
static struct l2tp_api_tunnel_msg_data tunnel;
static struct l2tp_api_session_msg_data session;

/* allow plugins to inhibit loading of the default ppp_unix plugin */
int				l2tp_inhibit_default_plugin;

/* profile management hooks */
int (*l2tp_profile_created_hook)(enum l2tp_profile_type type, const char *name) = NULL;
int (*l2tp_profile_deleted_hook)(enum l2tp_profile_type type, const char *name) = NULL;
int (*l2tp_profile_modified_hook)(enum l2tp_profile_type type, const char *name) = NULL;

static void l2tp_init(void);
static void l2tp_cleanup(void);
static int l2tp_parse_args(int argc, char **argv);

static void l2tp_network_check(struct l2tp_api_tunnel_msg_data msg);

static void usage(char **argv, int exit_code)
{
	fprintf(stderr, "Usage: %s [-f] [-D] [-d debugmask] [-u udpport] [-p plugin]\n\t"
#ifdef L2TP_FEATURE_RPC_MANAGEMENT
		"[-R] "
#endif
		"[-L facility] [-h]\n", argv[0]);
	fprintf(stderr, 
		"\t-h			This message\n"
		"\t-f			Run in foreground\n"
		"\t-u <port>		UDP port\n"
		"\t-p <plugin>		Load plugin\n"
#ifdef L2TP_FEATURE_LOCAL_CONF_FILE
		"\t-c <config-file>	Use specified file instead of " L2TP_CONFIG_FILENAME "\n"
#endif
#ifdef L2TP_FEATURE_LOCAL_STAT_FILE
		"\t-S                   Write peer/tunnel/session/profile status files\n"
#endif
#ifdef L2TP_FEATURE_RPC_MANAGEMENT
		"\t-R			Enable remote management (SUN RPC)\n"
#endif
		"\t-L <facility>\t	Send syslog messages to the specified facility\n"
 		"\t			local0..local7\n"
		"\t-D			Enable debug messages in new objects created\n"
		"\t                     unless overridden by trace_flags\n"
		"\t-d <flags>		Enable debug trace messages\n"
		"\t-y <num>		Throttle PPP setup rate by limiting the number of PPP"
		"\t\t\t\tprocesses trying to establish simultaneously. Default 0 (unlimited)\n"
);
	exit(exit_code);
}

int main(int argc, char **argv)
{
	int result;
	int log_flags = LOG_PID;
	struct utsname name;
	int fd;
	char pidstr[10];

	/* Parse arguments */
	result = l2tp_parse_args(argc, argv);
	if (result < 0) {
		fprintf(stderr, "Invalid argument\n");
		return result;
	}

	/* Create a pid file, error if already exists */
	fd = open(L2TP_PID_FILENAME, O_WRONLY | O_CREAT | O_EXCL, 0660);
	if (fd < 0) {
		if (errno == EEXIST) {
			fprintf(stderr, "File %s already exists. Is %s already running?\n",
				L2TP_PID_FILENAME, argv[0]);
		} else {
			fprintf(stderr, "File %s: %m", L2TP_PID_FILENAME);
		}
		exit(1);
	}

	/* Get system kernel info, which is used to build our vendor name */
	result = uname(&name);
	if (result < 0) {
		fprintf(stderr, "Failed to get system version info: %m");
		return result;
	}
	l2tp_kernel_version = strdup(name.release);
	l2tp_cpu_name = strdup(name.machine);
	if ((l2tp_kernel_version == NULL) || (l2tp_cpu_name == NULL)) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}
	l2tp_firmware_revision = (((L2TP_APP_MAJOR_VERSION & 0xff) << 8) |
				  ((L2TP_APP_MINOR_VERSION & 0xff) << 0));

	/* Become a daemon */
	if (!l2tp_opt_nodaemon) {
		usl_daemonize();
	}

	/* We write the PID file AFTER the double-fork */
	sprintf(&pidstr[0], "%d", getpid());
	if (write(fd, &pidstr[0], strlen(pidstr)) < 0)
		syslog(LOG_WARNING, "Failed to write pid file %s", L2TP_PID_FILENAME);
	close(fd);

	/* Open the syslog */
	if (l2tp_opt_debug) {
		log_flags |= LOG_NDELAY;
	}
	openlog("openl2tpd", log_flags, l2tp_opt_log_facility);
	if (l2tp_opt_debug) {
		setlogmask(LOG_UPTO(LOG_DEBUG));
	} else {
		setlogmask(LOG_UPTO(LOG_INFO));
	}
	l2tp_log(LOG_INFO, "Start, trace_flags=%08x%s", l2tp_opt_trace_flags,
		 l2tp_opt_debug ? " (debug enabled)" : "");

	/* Init the app */
	l2tp_init();

	/* Main loop - USL takes care of it */
	usl_main_loop();

	return 0;
}

void l2tp_mem_dump(int level, void *data, int data_len, int hex_only)
{
	int x, y;
	unsigned char *bytep;
	char cbuf[80];
	char nbuf[80];
	char *p;

	bytep = data;
	for (y = 0; y < data_len; y += 16) {
		memset(&cbuf[0], 0, sizeof(cbuf));
		memset(&nbuf[0], 0, sizeof(nbuf));
		p = &nbuf[0];

		for (x = 0; x < 16; x++, bytep++) {
			if ((x + y) >= data_len) {
				break;
			}
			cbuf[x] = isprint(*bytep) ? *bytep : '.';
			p += sprintf(p, "%02x ", *bytep);
		}
		l2tp_log(level, "%8d: %-48s  %s", y, nbuf, hex_only ? "" : cbuf);
	}
}

char *l2tp_buffer_hexify(void *buf, int buf_len)
{
	static char string_buf[80];
	int max_len = (buf_len < ((sizeof(string_buf) - 1) / 3)) ? buf_len : (sizeof(string_buf) - 1) / 3;
	int index;
	char *bufp = &string_buf[0];
	unsigned char *datap = (unsigned char *) buf;

	/* We use 3 chars in the output buffer per octet */
	for (index = 0; index < max_len; index++) {
		bufp += sprintf(bufp, "%02x ", *datap);
		datap++;
	}
	*bufp = '\0';
	
	return &string_buf[0];
}
 

static int l2tp_parse_log_facility(char *arg)
{
	static const struct {
		char *name;
		int facility;
	} codes[] = {
		{ "local0", LOG_LOCAL0 },
		{ "local1", LOG_LOCAL1 },
		{ "local2", LOG_LOCAL2 },
		{ "local3", LOG_LOCAL3 },
		{ "local4", LOG_LOCAL4 },
		{ "local5", LOG_LOCAL5 },
		{ "local6", LOG_LOCAL6 },
		{ "local7", LOG_LOCAL7 },
		{ NULL, 0 }
	};
	int index;

	if (arg == NULL) {
		return -EINVAL;
	}

	for (index = 0; codes[index].name != NULL; index++) {
		if (strcasecmp(arg, codes[index].name) == 0) {
			return codes[index].facility;
		}
	}

	fprintf(stderr, "Expecting local[0-7]\n");
	return -EINVAL;
}

static int l2tp_parse_args(int argc, char **argv)
{
	int opt;
	int result = 0;

#ifdef BRCM_L2TP_SUPPORT
	while((opt = getopt(argc, argv, "p:d:u:L:c:fRDSy:hr:U:P:c:")) != -1) {
#else
	while((opt = getopt(argc, argv, "p:d:u:L:c:fRDSy:h")) != -1) {
#endif
		switch(opt) {
		case 'h':
			usage(argv, 0);
			break;
		case 'f':
			l2tp_opt_nodaemon = 1;
			break;
		case 'D':
			l2tp_opt_debug = 1;
			break;
		case 'd':
			if (l2tp_parse_debug_mask(&l2tp_opt_trace_flags, optarg, 1) < 0) {
				exit(1);
			}
			break;
		case 'p':
			if (l2tp_plugin_add(optarg) < 0) {
				exit(1);
			}
			break;
#ifdef L2TP_FEATURE_LOCAL_CONF_FILE
		case 'c':
			l2tp_opt_config_filename = optarg;
			break;
#endif
#ifdef L2TP_FEATURE_LOCAL_STAT_FILE
		case 'S':
			l2tp_opt_write_status_files = 1;
			break;
#endif
		case 'R':
			l2tp_opt_remote_rpc = 1;
			break;
		case 'L':
			l2tp_opt_log_facility = l2tp_parse_log_facility(optarg);
			if (l2tp_opt_log_facility < 0) {
				result = -EINVAL;
			}
			break;
		case 'u':
			sscanf(optarg, "%d", &l2tp_opt_udp_port);
			L2TP_DEBUG(L2TP_API, "Using port %d", l2tp_opt_udp_port);
			break;
		case 'y':
			sscanf(optarg, "%d", &l2tp_opt_throttle_ppp_setup_rate);
			break;
#ifdef BRCM_L2TP_SUPPORT
		case 'r':
			sscanf(optarg, "%s", iremote);
			break;
		case 'U':
			sscanf(optarg, "%s", iuserid);
			break;
		case 'P':
			sscanf(optarg, "%s", ipassword);
			break;
		case 'c':
			sscanf(optarg, "%s", devname);
			break;
#endif
		default:
			usage(argv, 1);
		}
	}

	return result;
}

static void l2tp_toggle_debug(void)
{
#ifdef USL_DMALLOC
	dmalloc_log_unfreed();
	dmalloc_log_stats();
	dmalloc_log_heap_map();
#endif

	l2tp_opt_debug = !l2tp_opt_debug;
}

static void l2tp_dmalloc_dump(void)
{
#ifdef L2TP_DMALLOC
	dmalloc_log_changed(l2tp_dmalloc_mark, 1, 0, 1);
	l2tp_dmalloc_mark = dmalloc_mark();
	dmalloc_message("DMALLOC MARK set to %lu\n", l2tp_dmalloc_mark);
#endif
}

/* Warn about features not yet supported from one place so they're
 * easier to find...
 */
void l2tp_warn_not_yet_supported(const char *what)
{
	l2tp_log(LOG_WARNING, "WARNING: %s not yet supported", what);
}

/* die - clean up state and exit with the specified status.
 */
static void l2tp_die(void)
{
	static int exiting = 0;

	if (!exiting) {
		exiting = 1;
		l2tp_log(LOG_INFO, "Exiting");
		exit(1);
	} else {
		_exit(1);
	}
}

/* hangup - handle SIGHUP signal. Called from main loop.
 */
static void l2tp_hangup(void)
{
	l2tp_load_config_file();
}

/* This function is registered as a signal notifier with USL.
 */
static void l2tp_signal_handler(void *arg, int sig)
{
	switch (sig) {
	case SIGUSR1:
		l2tp_toggle_debug();
		break;
	case SIGUSR2:
		l2tp_dmalloc_dump();
		break;
	case SIGTERM:
		/* This is handled in the main loop */
		break;
	case SIGHUP:
		/* This is handled in the main loop */
		break;
	default:
		break;
	}
}

void l2tp_make_random_vector(void *buf, int buf_len)
{
	size_t count;

	count = usl_fd_read(l2tp_rand_fd, buf, buf_len);
	if ((count < 0) && (errno != EAGAIN)) {
		l2tp_log(LOG_ERR, "ERROR: problem reading /dev/random: %s", strerror(errno));
		exit(1);
	}
}

uint16_t l2tp_make_random_id(void)
{
	return (uint16_t) random();
}

int l2tp_random(int min, int max)
{
#ifdef BRCM_L2TP_SUPPORT
	int scale = (max - min);
#else
	float scale = (float) (max - min);
#endif
	return min + (int) (scale * rand() / (RAND_MAX + scale));
}

void l2tp_vlog(int level, const char *fmt, va_list ap)
{
#if 0
	if (l2tp_opt_nodaemon) {
		vprintf(fmt, ap);
		printf("\n");
	} else {
		vsyslog(level, fmt, ap);
	}
	DMALLOC_VMESSAGE(fmt, ap);
#endif
}

void l2tp_log(int level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	l2tp_vlog(level, fmt, ap);
	va_end(ap);
}

static void l2tp_system_log(int level, const char *fmt, ...)
{
	if (l2tp_opt_trace_flags & L2TP_DEBUG_SYSTEM) {
		va_list ap;

		va_start(ap, fmt);
		l2tp_vlog(level, fmt, ap);
		va_end(ap);
	}
}

char *l2tp_system_time(void)
{
	time_t now;
	char *tstr;

	now = time(NULL);
	if (now == -1) {
		return NULL;
	}

	tstr = ctime(&now);
	if (tstr != NULL) {
		return strdup(tstr);
	}
	return NULL;
}

/*****************************************************************************
 * Management interface
 *****************************************************************************/

bool_t l2tp_app_info_get_1_svc(struct l2tp_api_app_msg_data *msg, struct svc_req *req)
{
	int patches_len;

	msg->build_date = strdup(__DATE__);
	msg->build_time = strdup(__TIME__);
	msg->major = L2TP_APP_MAJOR_VERSION;
	msg->minor = L2TP_APP_MINOR_VERSION;
	msg->cookie = L2TP_APP_COOKIE;

	msg->features = 0;
#ifdef L2TP_FEATURE_LOCAL_CONF_FILE
	msg->features |= L2TP_API_APP_FEATURE_LOCAL_CONF_FILE;
#endif
#ifdef L2TP_FEATURE_LOCAL_STAT_FILE
	msg->features |= L2TP_API_APP_FEATURE_LOCAL_STAT_FILE;
#endif
#ifdef L2TP_FEATURE_LAC_SUPPORT
	msg->features |= L2TP_API_APP_FEATURE_LAC_SUPPORT;
#endif
#ifdef L2TP_FEATURE_LNS_SUPPORT
	msg->features |= L2TP_API_APP_FEATURE_LNS_SUPPORT;
#endif
#ifdef L2TP_FEATURE_RPC_MANAGEMENT
	msg->features |= L2TP_API_APP_FEATURE_RPC_MANAGEMENT;
#endif
#ifdef L2TP_FEATURE_LAIC_SUPPORT
	msg->features |= L2TP_API_APP_FEATURE_LAIC_SUPPORT;
#endif
#ifdef L2TP_FEATURE_LAOC_SUPPORT
	msg->features |= L2TP_API_APP_FEATURE_LAOC_SUPPORT;
#endif
#ifdef L2TP_FEATURE_LNIC_SUPPORT
	msg->features |= L2TP_API_APP_FEATURE_LNIC_SUPPORT;
#endif
#ifdef L2TP_FEATURE_LNOC_SUPPORT
	msg->features |= L2TP_API_APP_FEATURE_LNOC_SUPPORT;
#endif

	patches_len = sizeof(l2tp_installed_patches) - sizeof(l2tp_installed_patches[0]);
	msg->patches.patches_len = patches_len / sizeof(l2tp_installed_patches[0]);
	if (msg->patches.patches_len > 0) {
		msg->patches.patches_val = malloc(patches_len);
		if (msg->patches.patches_val == NULL) {
			return FALSE;
		}
		memcpy(msg->patches.patches_val, &l2tp_installed_patches[0], patches_len);
	}

	if ((msg->build_date == NULL) || (msg->build_time == NULL)) {
		return FALSE;
	}

	return TRUE;
}

bool_t l2tp_system_get_1_svc(struct l2tp_api_system_msg_data *msg, struct svc_req *req)
{
	int type;
	struct l2tp_api_system_msg_stats *stat;

	memset(msg, 0, sizeof(*msg));

	msg->config.trace_flags = l2tp_opt_trace_flags;
	msg->config.udp_port = l2tp_opt_udp_port;
	msg->config.flags = l2tp_opt_flags;

	/* catch uncopied data errors */
	USL_POISON_MEMORY(&msg->status.stats, 0xaa, sizeof(msg->status.stats));

	msg->status.stats.total_sent_control_frames = l2tp_stats.total_sent_control_frames;
	msg->status.stats.total_rcvd_control_frames = l2tp_stats.total_rcvd_control_frames;
	msg->status.stats.total_control_frame_send_fails = l2tp_stats.total_control_frame_send_fails;
	msg->status.stats.total_retransmitted_control_frames = l2tp_stats.total_retransmitted_control_frames;
	msg->status.stats.event_queue_full_errors = l2tp_stats.event_queue_full_errors;
	msg->status.stats.short_frames = l2tp_stats.short_frames;
	msg->status.stats.wrong_version_frames = l2tp_stats.wrong_version_frames;
	msg->status.stats.unexpected_data_frames = l2tp_stats.unexpected_data_frames;
	msg->status.stats.bad_rcvd_frames = l2tp_stats.bad_rcvd_frames;
	msg->status.stats.no_control_frame_resources = l2tp_stats.no_control_frame_resources;
	msg->status.stats.no_peer_resources = l2tp_stats.no_peer_resources;
	msg->status.stats.no_tunnel_resources = l2tp_stats.no_tunnel_resources;
	msg->status.stats.no_session_resources = l2tp_stats.no_session_resources;
	msg->status.stats.no_ppp_resources = l2tp_stats.no_ppp_resources;
	msg->status.stats.auth_fails = l2tp_stats.auth_fails;
	msg->status.stats.no_matching_tunnel_id_discards = l2tp_stats.no_matching_tunnel_id_discards;
	msg->status.stats.no_matching_session_id_discards = l2tp_stats.no_matching_session_id_discards;
	msg->status.stats.mismatched_tunnel_ids = l2tp_stats.mismatched_tunnel_ids;
	msg->status.stats.mismatched_session_ids = l2tp_stats.mismatched_session_ids;
	msg->status.stats.encode_message_fails = l2tp_stats.encode_message_fails;
	msg->status.stats.ignored_avps = l2tp_stats.ignored_avps;
	msg->status.stats.vendor_avps = l2tp_stats.vendor_avps;
	msg->status.stats.illegal_messages = l2tp_stats.illegal_messages;
	msg->status.stats.unsupported_messages = l2tp_stats.unsupported_messages;
	msg->status.stats.messages.messages_len = L2TP_API_MSG_TYPE_COUNT;
	msg->status.stats.messages.messages_val = calloc(L2TP_API_MSG_TYPE_COUNT, sizeof(*msg->status.stats.messages.messages_val));
	if (msg->status.stats.messages.messages_val == NULL) {
		msg->status.stats.messages.messages_len = 0;
	}
	
	stat = msg->status.stats.messages.messages_val;
	for (type = 0; type < msg->status.stats.messages.messages_len; type++) {
		stat->rx = l2tp_stats.messages[type].rx;
		stat->tx = l2tp_stats.messages[type].tx;
		stat->rx_bad = l2tp_stats.messages[type].rx_bad;
		stat++;
	}
	
	msg->status.stats.too_many_tunnels = l2tp_stats.too_many_tunnels;
	msg->status.stats.too_many_sessions = l2tp_stats.too_many_sessions;
	msg->status.stats.tunnel_setup_failures = l2tp_stats.tunnel_setup_failures;
	msg->status.stats.session_setup_failures = l2tp_stats.session_setup_failures;

	l2tp_tunnel_globals_get(msg);
	l2tp_session_globals_get(msg);

	return TRUE;
}

bool_t l2tp_system_modify_1_svc(struct l2tp_api_system_msg_data msg, int *result, struct svc_req *req)
{
	if (msg.config.flags & L2TP_API_CONFIG_FLAG_TRACE_FLAGS) {
		if (msg.config.trace_flags_mask == 0) {
			msg.config.trace_flags_mask = 0xffffffff;
		}
		l2tp_opt_trace_flags &= ~(msg.config.trace_flags_mask);
		l2tp_opt_trace_flags |= (msg.config.trace_flags & msg.config.trace_flags_mask);
	}
	if (msg.config.flags & L2TP_API_CONFIG_FLAG_RESET_STATISTICS) {
		memset(&l2tp_stats, 0, sizeof(l2tp_stats));
		msg.config.flags &= ~L2TP_API_CONFIG_FLAG_RESET_STATISTICS;
	}
	l2tp_tunnel_globals_modify(&msg, result);
	l2tp_session_globals_modify(&msg, result);

	*result = 0;

	/* Remember all non-default parameters */
	l2tp_opt_flags |= msg.config.flags;
	
	return TRUE;
}

/*****************************************************************************
 * Init and cleanup
 *****************************************************************************/

/* Removes any profiles and resets the default profiles.
 */
void l2tp_restore_default_config(void)
{
	l2tp_opt_flags = 0;

	l2tp_ppp_reinit();
	l2tp_session_reinit();
	l2tp_tunnel_reinit();
	l2tp_peer_reinit();
}

/* Read local config file.
 */
void l2tp_load_config_file(void)
{
#ifdef BRCM_L2TP_SUPPORT	
	struct in_addr addr;
	bool_t status;
	int result;
	// ppp
	ppp_profile.flags2 |= L2TP_API_PPP_PROFILE_FLAG_AUTH_REFUSE_EAP;
	ppp_profile.auth_refuse_eap = -1;
	status = l2tp_ppp_profile_modify_1_svc(ppp_profile, &result, NULL);
	if ((status != TRUE) || (result < 0))
		l2tp_log(LOG_ERR, "ppp profile modify: command failed: rc=%d", l2tp_strerror(-result));
	memset(&ppp_profile, 0, sizeof(ppp_profile));
	
	// tunnel
	tunnel.flags |= L2TP_API_TUNNEL_FLAG_TUNNEL_NAME;
	OPTSTRING(tunnel.tunnel_name) = "default";
	tunnel.tunnel_name.valid = 1;
	tunnel.flags |= L2TP_API_TUNNEL_FLAG_PEER_ADDR;
	addr.s_addr = htonl(inet_addr(iremote));
	tunnel.peer_addr.s_addr = addr.s_addr;
	tunnel.flags |= L2TP_API_TUNNEL_FLAG_PERSIST;
	tunnel.persist = 1;
	tunnel.flags |= L2TP_API_TUNNEL_FLAG_OUR_UDP_PORT;
	tunnel.our_udp_port = 1701;
	l2tp_network_check(tunnel);
	status = l2tp_tunnel_create_1_svc(tunnel, &result, NULL);
	if ((status != TRUE) || ((result < 0) && (result != -L2TP_ERR_TUNNEL_ALREADY_EXISTS)))
		l2tp_log(LOG_ERR, "tunnel create: command failed: rc=%d", l2tp_strerror(-result));
	memset(&tunnel, 0, sizeof(tunnel));
	
	// session
	session.flags |= L2TP_API_SESSION_FLAG_TUNNEL_NAME;
	OPTSTRING(session.tunnel_name) = "default";
	session.tunnel_name.valid = 1;
	session.flags |= L2TP_API_SESSION_FLAG_SESSION_NAME;
	OPTSTRING(session.session_name) = "default";
	session.session_name.valid = 1;
	session.flags |= L2TP_API_SESSION_FLAG_USER_NAME;
	OPTSTRING(session.user_name) = strdup(iuserid);
	session.user_name.valid = 1;
	session.flags |= L2TP_API_SESSION_FLAG_USER_PASSWORD;
	OPTSTRING(session.user_password) = strdup(ipassword);
	session.user_password.valid = 1;
	status = l2tp_session_create_1_svc(session, &result, NULL);
	if ((status != TRUE) || ((result < 0) && (result != -L2TP_ERR_SESSION_ALREADY_EXISTS)))
		l2tp_log(LOG_ERR, "session create: command failed: rc=%d", l2tp_strerror(-result));
	memset(&session, 0, sizeof(session));
#endif

#ifdef L2TP_FEATURE_LOCAL_CONF_FILE
	FILE *f;

	if (l2tp_opt_config_filename != NULL) {
		f = fopen(l2tp_opt_config_filename, "r");
		if (f == NULL) {
			l2tp_log(LOG_ERR, "Unable to open %s", l2tp_opt_config_filename);
			exit(1);
		}
	} else {
		f = fopen(L2TP_CONFIG_FILENAME, "r");
	}
	if (f != NULL) {
		l2tp_log(LOG_INFO, "Using config file: %s", 
			 l2tp_opt_config_filename ? l2tp_opt_config_filename : L2TP_CONFIG_FILENAME);
		l2tp_restore_default_config();
		if (l2tp_config_parse(&f) < 0) {
			exit(1);
		}
		fclose(f);
	}
#endif
}

static void l2tp_init(void)
{
	unsigned int seed;

	l2tp_log(LOG_INFO, "OpenL2TP V%d.%d, %s %s",
		 L2TP_APP_MAJOR_VERSION, L2TP_APP_MINOR_VERSION,
		 L2TP_APP_COPYRIGHT_INFO, L2TP_APP_VENDOR_INFO);

#ifdef L2TP_DMALLOC
	/* dmalloc debug options are set in the environment. However,
	 * certain options cause problems to this application. We
	 * therefore ensure that the troublesome options are disabled,
	 * regardless of the user's settings.  The disabled options
	 * are: alloc-blank, free-blank, force-linear.  If these
	 * options are enabled, it causes strange problems in the
	 * generated RPC code.
	 */
	dmalloc_debug(dmalloc_debug_current() & 0xff5dffff);
	l2tp_dmalloc_mark = dmalloc_mark();
	if (getenv("DMALLOC_OPTIONS") != NULL) {
		l2tp_log(LOG_WARNING, "DMALLOC debugging enabled");
	}
#endif
	l2tp_my_pid = getpid();

	atexit(l2tp_cleanup);
	L2TP_DEBUG(L2TP_FUNC, "%s (%s %s): trace flags = %08x", __func__, __DATE__, __TIME__, l2tp_opt_trace_flags);
	usl_set_debug(l2tp_opt_debug, l2tp_system_log);

	usl_signal_terminate_hook = l2tp_die;
	usl_signal_hangup_hook = l2tp_hangup;
	usl_signal_init();
	usl_fd_init();
	usl_timer_init();
	usl_pid_init();
	l2tp_plugin_init();
	l2tp_net_init();

	l2tp_rand_fd = open("/dev/random", O_RDONLY);
	if (l2tp_rand_fd < 0) {
		fprintf(stderr, "No /dev/random device found. Exiting.\n");
		exit(1);
	}

	/* Set the random() seed. We use random() rather than
	 * l2tp_make_random_vector() to generate tunnel/session ids
	 * because random() is faster and we potentially allocate
	 * tunnels/sessions very often. But for better randomness, set
	 * the seed using a good random value.
	 */
	l2tp_make_random_vector(&seed, sizeof(seed));
	srandom(seed);

	usl_signal_notifier_add(l2tp_signal_handler, NULL);

	l2tp_avp_init();
	l2tp_peer_init();
#ifdef L2TP_FEATURE_RPC_MANAGEMENT
	l2tp_api_init();
#endif
	l2tp_event_init();
	l2tp_xprt_init();
	l2tp_tunnel_init();
	l2tp_session_init();
	l2tp_ppp_init();

#ifdef L2TP_FEATURE_LOCAL_STAT_FILE
	if (l2tp_opt_write_status_files) {
		l2tp_statusfile_init();
	}
#endif

	l2tp_load_config_file();
}

static void l2tp_cleanup(void)
{
	pid_t pid;

	pid = getpid();
	if (pid != l2tp_my_pid) {
		L2TP_DEBUG(L2TP_FUNC, "%s: not main pid so returning now", __func__);
		return;
	}

	l2tp_log(LOG_INFO, "Cleaning up before exiting");

	usl_signal_notifier_remove(l2tp_signal_handler, NULL);

	/* Cleanup all resources */
#ifdef L2TP_FEATURE_RPC_MANAGEMENT
	l2tp_api_cleanup();
#endif
	l2tp_event_cleanup();
	l2tp_net_cleanup();
	l2tp_avp_cleanup();
	l2tp_ppp_cleanup();
	l2tp_session_cleanup();
	l2tp_xprt_cleanup();
	l2tp_tunnel_cleanup();
	l2tp_peer_cleanup();
	l2tp_plugin_cleanup();

	usl_timer_cleanup();
	usl_fd_cleanup();
	usl_signal_cleanup();
	usl_pid_cleanup();

	if (l2tp_rand_fd != 0) {
		close(l2tp_rand_fd);
	}

#ifdef L2TP_FEATURE_LOCAL_STAT_FILE
	if (l2tp_opt_write_status_files) {
		l2tp_statusfile_cleanup();
	}
#endif

	L2TP_DEBUG(L2TP_FUNC, "%s: done", __func__);

#ifdef L2TP_DMALLOC
	dmalloc_log_changed(l2tp_dmalloc_mark, 1, 0, 1);
	// dmalloc_log_unfreed();
	// dmalloc_log_stats();
	// dmalloc_log_heap_map();
#endif

	/* Remove pid file */
	unlink(L2TP_PID_FILENAME);

#ifdef BRCM_L2TP_SUPPORT
    memset(devname, 0, sizeof(devname));
#endif
}

static void l2tp_network_check(struct l2tp_api_tunnel_msg_data msg) {
	struct sockaddr_in peer_addr;
	struct in_addr local_addr;

	memset(&peer_addr, 0, sizeof(peer_addr));
	peer_addr.sin_family = AF_INET;
	peer_addr.sin_addr.s_addr = msg.peer_addr.s_addr;
	peer_addr.sin_port = htons(msg.peer_udp_port);

    //printf("l2tp_network_check: ipaddr = %x port = %d\n", msg.peer_addr.s_addr, peer_addr.sin_port);	
	if (msg.our_addr.s_addr == INADDR_ANY) {
	    while ((l2tp_net_get_local_address_for_peer(&peer_addr, &local_addr)) < 0) {
	    sleep(3);
	    }
	}
}
