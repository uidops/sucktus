#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <net/if.h>
#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>
#include <err.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
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
			exit((!(strcmp(*(argv+1), "-h")) ? EXIT_SUCCESS : EXIT_FAILURE));
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

	char date_text[9];
	char battery_text[11];
	char sound_text[11];
	char layout_text[12];
	char memory_text[64];
	char temp_text[11];
	char cpu_prec_text[12];
	char wifi_text[IW_ESSID_MAX_SIZE+6];
	char ethernet_text[19];
	char openvpn_text[10];

	struct timespec timeout;
	timeout.tv_sec = INTERVAL/1000;
	timeout.tv_nsec = (INTERVAL%1000)*1E6;

	while (done) {
		snprintf(text, 512, "〱 %s%s%s%s%s%s%s%s%s%s  %s",
				openvpn(openvpn_text, sizeof(ethernet_text)), ethernet(ethernet_text, sizeof(ethernet_text)),
				wifi(wifi_text, sizeof(wifi_text)),
				cpu_prec(cpu_prec_text, 12), temp(temp_text, sizeof(temp_text)),
				memory(memory_text, sizeof(memory_text)), layout(layout_text, sizeof(layout_text), dpy),
				volume(sound_text, sizeof(sound_text)), battery(battery_text, sizeof(battery_text)),
				date(date_text, sizeof(date_text)), ICON);

		XStoreName(dpy, DefaultRootWindow(dpy), text);
		XSync(dpy, 0);
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
		return UNKNOW;

	time_t clock = time(NULL);
	if (clock == (time_t)-1) {
		warn("time()");
		return UNKNOW;
	}

	struct tm *tm = localtime(&clock);
	if (tm == NULL) {
		warn("localtime()");
		return UNKNOW;
	}

	if (!strftime(text, len, "%I:%M %p", tm)) {
		warn("strftime()");
		return UNKNOW;
	}

	return text;
}

char *
battery(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOW;

	int fd = open(BAT"/capacity", O_RDONLY);
	if (fd == -1) {
		warn("open(%s)", BAT"/capacity");
		return UNKNOW;
	}

	char capacity[4];
	ssize_t d = read(fd, capacity, 3);
	if (d == -1) {
		warn("read(%s)", BAT"/capacity");
		return UNKNOW;
	}

	*(capacity+d + ((*(capacity+d-1) == '\n') ? -1 : 0)) = 0;
	close(fd);

	snprintf(text, len, "%s %s | ", battery_status(), capacity);

	return text;
}

char *
battery_status(void)
{
	int fd = open(BAT"/status", O_RDONLY);
	if (fd == -1) {
		warn("open(%s)", BAT"/status");
		return UNKNOW;
	}

	char status[16];
	ssize_t d = read(fd, status, sizeof(status)/sizeof(*status));
	if (d == -1) {
		warn("read(%s)", BAT"/status");
		return UNKNOW;
	}

	*(status+d + ((*(status+d-1) == '\n') ? -1 : 0)) = 0;
	close(fd);

	if (!strncmp(status, "Full", 4))
		return "";
	else if (!strncmp(status, "Discharging", 11))
		return "";
	else if (!strncmp(status, "Charging", 8))
		return "";
	else
		return "";
}

char *
volume(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOW;

	int psw = 0;
	long volume = 0;
	long min = 0;
	long max = 0;

	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, CARD);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);
	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, SND_INDEX);
	snd_mixer_selem_id_set_name(sid, SND_CARD);
	snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);
	snd_mixer_selem_get_playback_switch(elem, 0, &psw);

	if (!psw) {
		strncpy(text, "婢 Mute | ", len);
		return text;
	}

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_get_playback_volume(elem, 0, &volume);
	snd_mixer_close(handle);

	snprintf(text, len, "墳 %.f%% | ", round((double)100*((double)(volume)/(double)(max))));

	return text;
}

char *
layout(char *text, size_t len, Display *dpy)
{
	if (text == NULL || (len == 0 || dpy == NULL))
		return UNKNOW;

	XkbStateRec state;
	XkbGetState(dpy, XkbUseCoreKbd, &state);

	XkbDescPtr desc = XkbGetKeyboard(dpy, XkbAllComponentsMask, XkbUseCoreKbd);
	if (desc == NULL) {
		warn("XkbGetKeyboard()");
		return UNKNOW;
	}
	
	XkbRF_VarDefsRec vd;
	XkbRF_GetNamesProp(dpy, NULL, &vd);

	char *layout = strtok(vd.layout, ",");
	for (int i = 0; i < state.group; i++) {
		layout = strtok(NULL, ",");
		if (layout == NULL) {
			return UNKNOW;
		}
	}

	snprintf(text, len, " %s | ", layout);
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

	while (fgets(line, 512, fp) != NULL) {
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
			if (flag == 5)
				break;
			else
				continue;
	}

	if (fclose(fp) == EOF)
		warn("fclose()");

	return 1;
}

char *
memory(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOW;

	struct meminfo meminfo_new;
	char unit_used = 0;
	char unit_total = 0;

	memset(&meminfo_new, 0, sizeof(struct meminfo));

	if (!read_meminfo(&meminfo_new))
		return UNKNOW;

	long double used = (meminfo_new.memtotal - meminfo_new.memfree - \
		meminfo_new.buffers - meminfo_new.cached - meminfo_new.sreclaimable);
	long double total = meminfo_new.memtotal;

	if (used >= 1000000) {
		unit_used = 'G';
		used = used/(1000*1000);
	} else {
		unit_used = 'M';
		used = used/(1000);
	}

	if (total >= 1000000) {
		unit_total = 'G';
		total = total/(1000*1000);
	} else {
		unit_total = 'M';
		total = total/(1000);
	}

	snprintf(text, len, " %.1Lf %ci/%.1Lf %ci | ", used, unit_used, total, unit_total);
	return text;
}

char *
temp(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOW;

	int fd = open(TEMP, O_RDONLY);
	if (fd == -1) {
		warn("open(%s)", TEMP);
		return UNKNOW;
	}

	char value[3];
	ssize_t d = read(fd, value, 3);
	if (d == -1) {
		warn("read(%s)", TEMP);
		return UNKNOW;
	}
	*(value+d-1) = 0;

	if (close(fd) == -1)
		warn("close(%s)", TEMP);

	snprintf(text, len, " %s | ", value);
	return text;
}

char *
cpu_prec(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOW;

	FILE *stream = fopen(STAT, "r");
	if (stream == NULL) {
		warn("fopen(%s)", STAT);
		return UNKNOW;
	}

	char cpu[256];

	struct cpu a = {0, 0};
	static struct cpu b = {0, 0};

	if (fgets(cpu, 256, stream) == NULL) {
		warn("fgets(%s)", STAT);
		return UNKNOW;
	}

	*(cpu+strlen(cpu)-1) = 0;
	strncpy(cpu, cpu+5, strlen(cpu)-5);
	char *tok = strdup(cpu);
	if (tok == NULL) {
		warn("strdup()");
		return UNKNOW;
	}

	size_t n = 0;
	for (char *token = strtok(tok, " "); token != NULL; token = strtok(NULL, " ")) {
		a.sum += strtol(token, NULL, 10);
		if (n++ < 3)
			a.sum_3 += strtol(token, NULL, 10);;
	}

	if (b.sum == 0) {
		memcpy(&b, &a, sizeof(struct cpu));
		return UNKNOW;
	}

	if (a.sum-b.sum == 0)
		return UNKNOW;

	snprintf(text, len, " %.f%% | ", round((double)((double)100*(double)(a.sum_3-b.sum_3)/(double)(a.sum-b.sum))));
	memcpy(&b, &a, sizeof(struct cpu));

	free(tok);
	fclose(stream);

	return text;
}

char *
wifi(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOW;

	int sockfd;
	struct iwreq wreq;
	char *id = calloc(sizeof(char), IW_ESSID_MAX_SIZE+1);

	memset(&wreq, 0, sizeof(struct iwreq));
	wreq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
	strncpy(wreq.ifr_name, IW, sizeof(wreq.ifr_name));

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		warn("socket()");
		return UNKNOW;
	}

	wreq.u.essid.pointer = id;
	if (ioctl(sockfd, SIOCGIWESSID, &wreq) == -1) {
		warn("ioctl()");
		close(sockfd);
		return UNKNOW;
	}

	if (close(sockfd) == -1)
		warn("close()");

	if (*id == 0) {
		free(id);
		return UNKNOW;
	}

	snprintf(text, len, " %s | ", id);
	free(id);

	return text;
}

char *
ethernet(char *text, size_t len)
{
	struct ifreq ifr;
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		warn("socket()");
		return UNKNOW;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, ET, sizeof(ifr.ifr_name));
	if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == -1) {
		warn("ioctl()");
		if(close(sockfd) == -1)
			warn("close()");
		return UNKNOW;
	}

	if (close(sockfd) == -1)
		warn("close()");

	if (ifr.ifr_flags&IFF_RUNNING) {
		strncpy(text, " Connected | ", len);
		return text;
	}

	return UNKNOW;
}

char *
openvpn(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOW;

	struct if_nameindex *if_ni;
	struct if_nameindex *i;

	if_ni = if_nameindex();
	if (if_ni == NULL) {
		warn("if_nameindex()");
		return UNKNOW;
	}

	for (i = if_ni; i->if_index != 0 && i->if_name != NULL; i++)
		if (!strncmp(i->if_name, TN, strlen(TN))) {
			strncpy(text, " VPN | ", len);
			if_freenameindex(if_ni);
			return text;
		}

	if_freenameindex(if_ni);
	return UNKNOW;
}
