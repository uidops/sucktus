#include <dbus/dbus.h>
#include <pulse/pulseaudio.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/rtnetlink.h>
#include <linux/nl80211.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <stdio.h>
#include <X11/extensions/XKBrules.h>
#include <err.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"

int
main(int argc, char **argv)
{
	if (argc > 1) {
		if (!strcmp(*(argv+1), "-v")) {
			printf("sucktus %s\n", VERSION);
			exit(EXIT_SUCCESS);
		} else {
			printf("suckless %s\n\n", VERSION);
			printf("usage: %s [-vh]\n", *(argv));
			exit((strcmp(*(argv+1), "-h") ? EXIT_FAILURE : EXIT_SUCCESS));
		}
	}

	Display *dpy = XOpenDisplay(NULL);
	if (dpy == NULL)
		err(EXIT_FAILURE, "XOpenDisplay(%s)", getenv("DISPLAY"));

	char *text = calloc(sizeof(char), 256);
	if (text == NULL) {
		XCloseDisplay(dpy);
		err(EXIT_FAILURE, "calloc()");
	}

	signal(SIGINT, change_done);

	char date_text[11];
	char battery_text[11];
	char bluez_text[8];
	char volume_text[12];
	char layout_text[12];
	char memory_text[64];
	char temp_text[11];
	char cpu_prec_text[12];
	char wifi_text[IW_ESSID_MAX_SIZE+6];
	char ethernet_text[19];
	char openvpn_text[10];
	char tmonitor_text[15];

	struct timespec timeout = {INTERVAL/1000, (INTERVAL%1000)*1E6};

	uint64_t rx_bytes = get_rx_bytes();
	while (done) {
		snprintf(text, 256, "〱 %s%s%s%s%s%s%s%s%s%s%s%s  %s",
				unitconv(tmonitor_text, sizeof(tmonitor_text), (get_rx_bytes() - rx_bytes) / (float) INTERVAL * 1000.0),
				openvpn(openvpn_text, sizeof(ethernet_text)), ethernet(ethernet_text, sizeof(ethernet_text)),
				wifi(wifi_text, sizeof(wifi_text)),
				cpu_prec(cpu_prec_text, 12), temp(temp_text, sizeof(temp_text)),
				memory(memory_text, sizeof(memory_text)), layout(layout_text, sizeof(layout_text), dpy),
				volume(volume_text, sizeof(volume_text)), bluez_battery(bluez_text, sizeof(bluez_text)),
				battery(battery_text, sizeof(battery_text)), date(date_text, sizeof(date_text)), ICON);

		XStoreName(dpy, DefaultRootWindow(dpy), text);
		XSync(dpy, 0);

		rx_bytes = get_rx_bytes();
		nanosleep(&timeout, NULL);
	}

	free(text);
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}


static void
change_done(int sig_number)
{
	(void) sig_number;
	done = 0;

	return;
}


char *
date(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOWN;

	time_t clock = time(NULL);
	if (clock == (time_t)-1) {
		warn("time()");
		return UNKNOWN;
	}

	struct tm *tm = localtime(&clock);
	if (tm == NULL) {
		warn("localtime()");
		return UNKNOWN;
	}

	if (!strftime(text, len, "T %I:%M %p", tm)) {
		warn("strftime()");
		return UNKNOWN;
	}

	return text;
}


char *
battery(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOWN;

	int fd = open(BAT"/capacity", O_RDONLY);
	if (fd == -1) {
		warn("open(%s)", BAT"/capacity");
		return UNKNOWN;
	}

	char capacity[4];
	ssize_t d = read(fd, capacity, 3);
	if (d == -1) {
		warn("read(%s)", BAT"/capacity");
		if (close(fd) == -1)
			warn("close(%s)", TEMP);

		return UNKNOWN;
	}

	*(capacity+d + ((*(capacity+d-1) == '\n') ? -1 : 0)) = 0;

	if (close(fd) == -1)
		warn("close(%s)", TEMP);

	snprintf(text, len, "%s %s | ", battery_status(), atoi(capacity) < 100 ? capacity : "Full");

	return text;
}


char *
battery_status(void)
{
	int fd = open(BAT"/status", O_RDONLY);
	if (fd == -1) {
		warn("open(%s)", BAT"/status");
		return UNKNOWN;
	}

	char status[16];
	ssize_t d = read(fd, status, sizeof(status)/sizeof(*status));
	if (d == -1) {
		warn("read(%s)", BAT"/status");
		if (close(fd) == -1)
			warn("close(%s)", TEMP);

		return UNKNOWN;
	}

	*(status+d + ((*(status+d-1) == '\n') ? -1 : 0)) = 0;
	if (close(fd) == -1)
		warn("close(%s)", TEMP);

	if (!strncmp(status, "Full", 4))
		return "B";
	else if (!strncmp(status, "Discharging", 11))
		return "B-";
	else if (!strncmp(status, "Charging", 8))
		return "B+";
	else
		return "B!";
}


char *
bluez_battery(char *text, size_t len)
{
	DBusConnection *con = NULL;
	DBusMessage *msg = NULL;
	DBusError error = {0};
	DBusMessageIter args = {0}, var = {0};
	DBusPendingCall *pending = NULL;

	const char *device = "org.bluez.Battery1";
	const char *perc = "Percentage";
	uint8_t charge = 0xff;

	dbus_error_init(&error);
	con = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set(&error)) {
		warnx("dbus_bus_get(): %s", error.message);
		return UNKNOWN;
	}

	msg = dbus_message_new_method_call("org.bluez", BLZDEV, "org.freedesktop.DBus.Properties", "Get");
	dbus_message_append_args(msg, DBUS_TYPE_STRING, &device, DBUS_TYPE_STRING, &perc, DBUS_TYPE_INVALID);
	if (!dbus_connection_send_with_reply(con, msg, &pending, -1)) {
		warnx("dbus_connection_send_with_reply(): failed");
		return UNKNOWN;
	}

	dbus_connection_flush(con);
	dbus_pending_call_block(pending);

	msg = dbus_pending_call_steal_reply(pending);
	if (!msg)
		errx(EXIT_FAILURE, "dbus_pending_call_steal_reply(): failed");

	if (dbus_message_iter_init(msg, &args) && dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
		dbus_message_iter_recurse(&args, &var);
		if (dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_BYTE)
			dbus_message_iter_get_basic(&var, &charge);
	}

	dbus_pending_call_unref(pending);
	dbus_message_unref(msg);
	dbus_connection_unref(con);

	if (charge == 0xff)
		return UNKNOWN;

	snprintf(text, len, "H %u | ", charge);
	return text;
}


void
_sink_info_callback(pa_context *context, const pa_sink_info *sink_info, int eol, void *userdata)
{
	(void) context; (void) userdata;
	if (eol == 0)
		vol = (sink_info->mute == 1) ? 0xff : (pa_cvolume_avg(&sink_info->volume) * 100) / PA_VOLUME_NORM;

	pa_mainloop_quit(mainloop, 0);
}


void
_server_info_callback(pa_context *context, const pa_server_info *server_info, void *userdata)
{
	(void) context; (void) userdata;
	pa_operation_unref(pa_context_get_sink_info_by_name(context, server_info->default_sink_name, _sink_info_callback, NULL));
}


void
_context_state_callback(pa_context *context, void *userdata)
{
    switch (pa_context_get_state(context)) {
        case PA_CONTEXT_READY:
            pa_operation_unref(pa_context_get_server_info(context, _server_info_callback, userdata));
            break;

        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_mainloop_quit(mainloop, 0);
            break;

        default:
            break;
    }
}


char *
volume(char *text, size_t len)
{
	pa_context *context = NULL;
	pa_mainloop_api *api = NULL;

	mainloop = pa_mainloop_new();
	api = pa_mainloop_get_api(mainloop);
	context = pa_context_new(api, "SUCKTUS");

	pa_context_set_state_callback(context, _context_state_callback, NULL);
	if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
		warnx("pa_context_connect(): failed");
		return UNKNOWN;
	}

	pa_mainloop_run(mainloop, NULL);

	pa_context_disconnect(context);
	pa_context_unref(context);
	pa_mainloop_free(mainloop);

	if (vol == 0xff)
		snprintf(text, len, "V Mute | ");
	else
		snprintf(text, len, "V %u | ", vol);

	return text;
}


char *
layout(char *text, size_t len, Display *dpy)
{
	if (text == NULL || (len == 0 || dpy == NULL))
		return UNKNOWN;

	XkbStateRec state;
	XkbGetState(dpy, XkbUseCoreKbd, &state);

	XkbDescPtr desc = XkbGetKeyboard(dpy, XkbAllComponentsMask, XkbUseCoreKbd);
	if (desc == NULL) {
		warn("XkbGetKeyboard()");
		return UNKNOWN;
	}

	XkbRF_VarDefsRec vd;
	XkbRF_GetNamesProp(dpy, NULL, &vd);

	char *layout = strtok(vd.layout, ",");
	for (int i = 0; i < state.group; i++) {
		layout = strtok(NULL, ",");
		if (layout == NULL)
			return UNKNOWN;
	}

	snprintf(text, len, "K %s | ", layout);
	XkbFreeKeyboard(desc, XkbAllComponentsMask, 1);

	return text;
}


int
read_meminfo(struct meminfo *mem)
{
	if (mem == NULL)
		return 0;

	FILE *fp = fopen(MEMINFO, "r");
	if (fp == NULL) {
		warn("fopen(%s)", MEMINFO);
		return 0;
	}

	char line[128];
	int flag = 0;

	while (fgets(line, 0x80, fp) != NULL) {
		if (!strncmp(line, "MemTotal:", 9) && ++flag)
			sscanf(line, "MemTotal: %lld", &(mem->memtotal));
		else if (!strncmp(line, "MemFree:",8) && ++flag)
			sscanf(line, "MemFree: %lld", &(mem->memfree));
		else if (!strncmp(line, "Buffers:", 8) && ++flag)
			sscanf(line, "Buffers: %lld", &(mem->buffers));
		else if (!strncmp(line, "Cached:", 7) && ++flag)
			sscanf(line, "Cached: %lld", &(mem->cached));
		else if (!strncmp(line, "SReclaimable:", 13) && ++flag)
			sscanf(line, "SReclaimable: %lld", &(mem->sreclaimable));
		else
			if (flag >= 5)
				break;
	}

	if (fclose(fp) == EOF)
		warn("fclose()");

	return 1;
}


char *
memory(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOWN;

	struct meminfo meminfo_new;
	char unit_used = 'M', unit_total = 'M';

	memset(&meminfo_new, 0, sizeof(struct meminfo));
	if (!read_meminfo(&meminfo_new))
		return UNKNOWN;

	long double used = (meminfo_new.memtotal - meminfo_new.memfree - \
		meminfo_new.buffers - meminfo_new.cached - meminfo_new.sreclaimable);
	long double total = meminfo_new.memtotal;

	if (used >= 1E6) {
		unit_used = 'G';
		used = used/1E6;
	} else
		used = used/1E3;

	if (total >= 1E6) {
		unit_total = 'G';
		total = total/1E6;
	} else
		total = total/1E3;

	snprintf(text, len, "M %.1Lf %ci/%.1Lf %ci | ", used, unit_used, total, unit_total);
	return text;
}


char *
temp(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOWN;

	int fd = open(TEMP, O_RDONLY);
	if (fd == -1) {
		warn("open(%s)", TEMP);
		return UNKNOWN;
	}

	char value[3];
	ssize_t d = read(fd, value, 3);
	if (d == -1) {
		if (close(fd) == -1)
			warn("close(%s)", TEMP);

		warn("read(%s)", TEMP);
		return UNKNOWN;
	}

	*(value+d-1) = 0;
	if (close(fd) == -1)
		warn("close(%s)", TEMP);

	snprintf(text, len, "T %s | ", value);
	return text;
}


char *
cpu_prec(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOWN;

	FILE *stream = fopen(STAT, "r");
	if (stream == NULL) {
		warn("fopen(%s)", STAT);
		return UNKNOWN;
	}

	char cpu[256];
	struct cpu a = {0, 0};
	static struct cpu b = {0, 0};

	if (fgets(cpu, 256, stream) == NULL) {
		warn("fgets(%s)", STAT);
		fclose(stream);
		return UNKNOWN;
	}

	*(cpu+strlen(cpu)-1) = 0;
	strncpy(cpu, cpu+5, strlen(cpu)-5);
	char *tok = strdup(cpu);
	if (tok == NULL) {
		warn("strdup()");
		fclose(stream);
		return UNKNOWN;
	}

	size_t n = 0;
	for (char *token = strtok(tok, " "); token != NULL; token = strtok(NULL, " ")) {
		a.sum += strtol(token, NULL, 10);
		if (n++ < 3)
			a.sum_3 += strtol(token, NULL, 10);;
	}

	if (b.sum == 0) {
		memcpy(&b, &a, sizeof(struct cpu));
		fclose(stream);
		free(tok);
		return UNKNOWN;
	}

	if (a.sum-b.sum == 0) {
		fclose(stream);
		free(tok);
		return UNKNOWN;
	}

	snprintf(text, len, "C %.f | ", round((double)((double)100*(double)(a.sum_3-b.sum_3)/(double)(a.sum-b.sum))));
	memcpy(&b, &a, sizeof(struct cpu));

	free(tok);
	fclose(stream);

	return text;
}


char *
wifi(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOWN;

    int fd = 0;
    uint8_t *buf = NULL;
    uint32_t family_id = 0;
	char *ssid = NULL;
    struct sockaddr_nl sa = {0};
    struct raw_netlink_generic_metadata req = {0};
    struct nlmsghdr *nlmh = NULL;
    struct nlattr *nla = NULL;

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK, NETLINK_GENERIC);
	if (fd == -1) {
		warn("socket()");
		return UNKNOWN;
	}

	sa.nl_family = AF_NETLINK;
	sa.nl_groups = 0;
	sa.nl_pad = 0;
	sa.nl_pid = getpid();

	if (bind(fd, (struct sockaddr *) &sa, sizeof(struct sockaddr_nl)) == -1) {
		warn("bind()");
		close(fd);
		return UNKNOWN;
	}

	req.nlmh.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	req.nlmh.nlmsg_type = GENL_ID_CTRL;
	req.nlmh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.nlmh.nlmsg_seq = 1;
	req.nlmh.nlmsg_pid = getpid();

	req.genlmh.cmd = CTRL_CMD_GETFAMILY;
	req.genlmh.version = 2;
	req.genlmh.reserved = 0;

	nla = (struct nlattr *) GENLMSG_DATA(&req);
	nla->nla_type = CTRL_ATTR_FAMILY_NAME;
	nla->nla_len = strlen(NL80211_GENL_NAME) + 1 + NLA_HDRLEN;
	strcpy(NLA_DATA(nla), NL80211_GENL_NAME);

	req.nlmh.nlmsg_len += NLMSG_ALIGN(nla->nla_len);

	buf = calloc(BUFLEN, sizeof(uint8_t));
	if (buf == NULL) {
		warn("calloc()");
		close(fd);
		return UNKNOWN;
	}

	if (send(fd, (void *)&req, req.nlmh.nlmsg_len, 0) == -1) {
		warn("send()");
		free(buf);
		close(fd);
		return UNKNOWN;
	}

	if (buf == NULL) {
		warn("calloc()");
		free(buf);
		close(fd);
		return UNKNOWN;
	}

	while (recv(fd, buf, BUFLEN, 0) > 0) {
		nlmh = (struct nlmsghdr *) buf;
		nla = GENLMSG_DATA(nlmh);
		while ((int64_t)((char *) nla - (char *) nlmh) < nlmh->nlmsg_len - NLA_HDRLEN) {
			if (nla->nla_type == CTRL_ATTR_FAMILY_ID)
				family_id = *((uint32_t *) NLA_DATA(nla));

			nla = (struct nlattr *) ((char *) nla + NLA_ALIGN(nla->nla_len));
		}
	}

	req.nlmh.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	req.nlmh.nlmsg_type = family_id;
	req.nlmh.nlmsg_seq = 2;
	req.nlmh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP;

	req.genlmh.cmd = NL80211_CMD_GET_INTERFACE;
	req.genlmh.version = 0;

	if (send(fd, (void *)&req, req.nlmh.nlmsg_len, 0) == -1) {
		warn("send()");
		free(buf);
		close(fd);
		return UNKNOWN;
	}

	while (recv(fd, buf, BUFLEN, 0) > 0) {
		nlmh = (struct nlmsghdr *) buf;
		nla = GENLMSG_DATA(nlmh);
		while ((int64_t)((char *) nla - (char *) nlmh) < nlmh->nlmsg_len - NLA_HDRLEN) {
			if (nla->nla_type == NL80211_ATTR_SSID)
				ssid = NLA_DATA(nla);

			nla = (struct nlattr *) ((char *) nla + NLA_ALIGN(nla->nla_len));
		}
	}

	if (ssid != NULL)
		snprintf(text, len, "W %s | ", ssid);

	else
		text = UNKNOWN;

	free(buf);
	close(fd);

	return text;
}


char *
ethernet(char *text, size_t len)
{
	int fd = 0, n = 0;
	uint8_t *buf = NULL;
	struct sockaddr_nl sa = {0};
	struct raw_netlink_route_metadata req = {0};
	struct nlmsghdr *nlmh = NULL;
	struct ifinfomsg *ifi = NULL;

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK, NETLINK_ROUTE);
	if (fd == -1) {
		warn("socket()");
		return UNKNOWN;
	}

	buf = calloc(BUFLEN, sizeof(uint8_t));
	if (buf == NULL) {
		warn("calloc()");
		return UNKNOWN;
	}

	for (int i = 0; ET[i] != NULL; i++) {
		sa.nl_family = AF_NETLINK;
		sa.nl_groups = 0;
		sa.nl_pad = 0;
		sa.nl_pid = getpid();

		if (bind(fd, (struct sockaddr *) &sa, sizeof(struct sockaddr_nl)) == -1) {
			warn("bind()");
			continue;
		}

		req.nlmh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
		req.nlmh.nlmsg_type = RTM_GETLINK;
		req.nlmh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
		req.nlmh.nlmsg_pid = getpid();
		req.nlmh.nlmsg_seq = i;

		req.ifmh.ifi_family = AF_UNSPEC;
		req.ifmh.ifi_type = 0;
		req.ifmh.ifi_index = if_nametoindex(ET[i]);
		if (req.ifmh.ifi_index == 0)
			continue;

		req.ifmh.ifi_flags = 0;
		req.ifmh.ifi_change = 0xffffffff;

		if (send(fd, &req, req.nlmh.nlmsg_len, 0) == -1) {
			warn("send()");
			continue;
		}

        while (recv(fd, buf, BUFLEN, 0) > 0) {
			nlmh = (struct nlmsghdr *) buf;
			if (nlmh->nlmsg_type == RTM_NEWLINK) {
				ifi = (struct ifinfomsg *) NLMSG_DATA(nlmh);
				if (ifi->ifi_flags & IFF_RUNNING) {
					n = 1;
					break;
				}
			}
        }

		if (n) break;
	}

	if (n)
		strncpy(text, "ETH Connected | ", len);

	else
		text = UNKNOWN;

	free(buf);
	close(fd);

	return text;
}


char *
openvpn(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOWN;

	struct if_nameindex *if_ni = if_nameindex();
	if (if_ni == NULL) {
		warn("if_nameindex()");
		return UNKNOWN;
	}

	for (struct if_nameindex *i = if_ni; i->if_index != 0 && i->if_name != NULL; i++)
		for (size_t j = 0; VI[j] != NULL; j++)
			if (!strncmp(i->if_name, VI[j], strlen(VI[j]))) {
				strncpy(text, "VPN ", len);
				if_freenameindex(if_ni);
				return text;
			}

	if_freenameindex(if_ni);
	return UNKNOWN;
}


uint64_t
get_rx_bytes(void)
{
	int fd = 0, rc = 0, len = 0;
	uint8_t *buf = NULL;
	uint64_t rx_bytes = 0;
	struct sockaddr_nl recv_addr = {0};
	struct raw_netlink_route_metadata req = {0};
	struct nlmsghdr *recv_hdr = NULL;
	struct ifinfomsg *infomsg = NULL;
	struct rtattr *rta = NULL;
	struct rtnl_link_stats64 *stats64 = NULL;

	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK, NETLINK_ROUTE);
	if (fd == -1) {
		warn("socket()");
		return 0;
	}

	recv_addr.nl_family = AF_NETLINK;
	recv_addr.nl_pad = 0;
	recv_addr.nl_pid = getpid();
	recv_addr.nl_groups = 0;

	rc = bind(fd, (struct sockaddr *) &recv_addr, sizeof(struct sockaddr_nl));
	if (rc == -1) {
		warn("bind()");
		close(fd);
		return 0;
	}

	req.nlmh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.nlmh.nlmsg_type = RTM_GETLINK;
	req.nlmh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.nlmh.nlmsg_pid = getpid();
	req.nlmh.nlmsg_seq = 1;

	req.ifmh.ifi_family = AF_UNSPEC;
	req.ifmh.ifi_type = 0;
	req.ifmh.ifi_index = if_nametoindex(TM);
	if (req.ifmh.ifi_index == 0) {
		warn("if_nametoindex(%s)", TM);
		close(fd);
		return 0;
	}

	req.ifmh.ifi_flags = 0;
	req.ifmh.ifi_change = 0xffffffff;

	rta = IFLA_RTA(&req.ifmh);
	rta->rta_type = IFLA_STATS;
	rta->rta_len = RTA_LENGTH(0);
 
	req.nlmh.nlmsg_len += RTA_ALIGN(rta->rta_len);

	rc = send(fd, &req, sizeof(struct raw_netlink_route_metadata), 0);
	if (rc == -1) {
		warn("send()");
		close(fd);
		return 0;
	}

	buf = calloc(BUFLEN, sizeof(uint8_t));
	if (buf == NULL) {
		warn("calloc()");
		close(fd);
		return 0;
	}

	while (recv(fd, buf, BUFLEN, 0) > 0) {
		recv_hdr = (struct nlmsghdr *) buf;
		infomsg = NLMSG_DATA(recv_hdr);
		rta = IFLA_RTA(infomsg);

		len = recv_hdr->nlmsg_len;
		while (RTA_OK(rta, len)) {
			if (rta->rta_type == IFLA_STATS64) {
				stats64 = RTA_DATA(rta);
				rx_bytes = stats64->rx_bytes;
				break;
			}

			rta = RTA_NEXT(rta, len);
		}
	}

	free(buf);
	close(fd);

	return rx_bytes;
}


char *
unitconv(char *retmsg, size_t len, uint64_t bps)
{
    int ndigits = 0;
    if (bps == 0)
        return UNKNOWN;

    ndigits = log10(bps);
    if (ndigits - 3 < 0)
        snprintf(retmsg, len, "%zu B/s | ", bps);
    else if (ndigits - 6 < 0)
        snprintf(retmsg, len, "%.1f KB/s | ", bps/1E3);
    else if (ndigits - 9 < 0)
        snprintf(retmsg, len, "%.1f MB/s | ", bps/1E6);
    else if (ndigits - 9 < 0)
        snprintf(retmsg, len, "%.1f GB/s | ", bps/1E9);
    else if (ndigits - 9 < 0)
        snprintf(retmsg, len, "%.1f TB/s | ", bps/1E12);
    else
        snprintf(retmsg, len, "%.1f PB/s | ", bps/1E15);

    return retmsg;
}
