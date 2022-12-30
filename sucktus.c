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
	char volume_text[12];
	char layout_text[12];
	char memory_text[64];
	char temp_text[11];
	char cpu_prec_text[12];
	char wifi_text[IW_ESSID_MAX_SIZE+6];
	char ethernet_text[19];
	char openvpn_text[10];

	struct timespec timeout = {INTERVAL/1000, (INTERVAL%1000)*1E6};

	while (done) {
		snprintf(text, 256, "〱 %s%s%s%s%s%s%s%s%s%s  %s",
				openvpn(openvpn_text, sizeof(ethernet_text)), ethernet(ethernet_text, sizeof(ethernet_text)),
				wifi(wifi_text, sizeof(wifi_text)),
				cpu_prec(cpu_prec_text, 12), temp(temp_text, sizeof(temp_text)),
				memory(memory_text, sizeof(memory_text)), layout(layout_text, sizeof(layout_text), dpy),
				volume(volume_text, sizeof(volume_text)), battery(battery_text, sizeof(battery_text)),
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

	snprintf(text, len, "%s %s | ", battery_status(), atoi(capacity)%100 ? capacity : "Full");

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
volume(char *text, size_t len)
{
	if (text == NULL || len == 0)
		return UNKNOWN;

	int psw = 0;
	long volume = 0, min = 0, max = 0;

	snd_mixer_t *handle = NULL;
	snd_mixer_selem_id_t *sid = NULL;
	snd_mixer_elem_t *elem = NULL;

	if (snd_mixer_open(&handle, 0) < 0)
		return UNKNOWN;
	else if (snd_mixer_attach(handle, CARD) < 0)
		goto error;
	else if (snd_mixer_selem_register(handle, NULL, NULL) < 0)
		goto error;
	else if (snd_mixer_load(handle) < 0)
		goto error;

	snd_mixer_selem_id_alloca(&sid);
	if (sid == NULL)
		goto error;

	snd_mixer_selem_id_set_index(sid, SND_INDEX);
	snd_mixer_selem_id_set_name(sid, SND_CARD);
	elem = snd_mixer_find_selem(handle, sid);
	if (elem == NULL)
		goto error;
	else if (snd_mixer_selem_get_playback_switch(elem, 0, &psw) < 0)
		goto error;

	if (!psw) {
		strncpy(text, "V Mute | ", len);
		goto end;
	}

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_get_playback_volume(elem, 0, &volume);

	snprintf(text, len, "V %.f | ", round((double)100*((double)(volume)/(double)(max))));

	end:
		snd_mixer_close(handle);
		return text;

	error:
		if (handle != NULL)
			snd_mixer_close(handle);

		return UNKNOWN;
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

	while (fgets(line, 0x200, fp) != NULL) {
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

	int sockfd = 0;
	struct iwreq wreq;
	char *id = calloc(sizeof(char), IW_ESSID_MAX_SIZE+1);

	for (int i = 0; IW[i] != NULL; i++) {
		memset(&wreq, 0, sizeof(struct iwreq));
		wreq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
		strncpy(wreq.ifr_name, IW[i], sizeof(wreq.ifr_name));

		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sockfd == -1) {
			warn("socket()");
			free(id);
			return UNKNOWN;
		}

		wreq.u.essid.pointer = id;
		if (ioctl(sockfd, SIOCGIWESSID, &wreq) == -1) {
			if (close(sockfd) == -1)
				warn("close()");

			continue;
		}

		if (close(sockfd) == -1)
			warn("close()");

		break;
	}

	if (*id == 0) {
		free(id);
		return UNKNOWN;
	}

	snprintf(text, len, "W %s | ", id);
	free(id);

	return text;
}


char *
ethernet(char *text, size_t len)
{
	int sockfd = 0;
	struct ifreq ifr;

	for (int i = 0; ET[i] != NULL; i++) {
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sockfd == -1) {
			warn("socket()");
			return UNKNOWN;
		}

		memset(&ifr, 0, sizeof(struct ifreq));
		strncpy(ifr.ifr_name, ET[i], sizeof(ifr.ifr_name));
		if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == -1) {
			if (close(sockfd) == -1)
				warn("close()");

			continue;
		}

		if (close(sockfd) == -1)
			warn("close()");

		if (ifr.ifr_flags&IFF_RUNNING) {
			strncpy(text, "ETH Connected | ", len);
			return text;
		}
	}

	return UNKNOWN;
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
