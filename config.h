#ifndef CONFIG_H
#define CONFIG_H

#include <pulse/mainloop.h>
#include <X11/Xlib.h>
#include <linux/genetlink.h>
#include <linux/rtnetlink.h>
#include <stdio.h>

#define VERSION "v2.0"

#define ICON ""
#define UNKNOWN ""
#define INTERVAL 1000

#define BAT "/sys/class/power_supply/BAT1"
#define CARD "default"
#define SND_CARD "Master"
#define SND_INDEX 0
#define MEMINFO "/proc/meminfo"
#define TEMP "/sys/class/thermal/thermal_zone2/temp"
#define STAT "/proc/stat"
#define BLZDEV "/org/bluez/hci0/dev_41_42_08_E1_8D_60"
#define TM "wlp3s0"

#define GENLMSG_DATA(gh) ((void *)(NLMSG_DATA(gh) + GENL_HDRLEN))
#define NLA_DATA(na) ((void *)((char*)(na) + NLA_HDRLEN))

#define BUFLEN (1 << 12)


static char *ET[] = {"enp1s0", "enp0s20f0u2", "enp2s0", NULL};
static char *VI[] = {"tun0", "proton0", NULL};


struct meminfo {
    long long int memtotal;
    long long int memfree;
    long long int buffers;
    long long int cached;
    long long int sreclaimable;
};

struct cpu {
    long long int sum;
    long long int sum_3;
};

struct raw_netlink_route_metadata {
	struct nlmsghdr nlmh;
	struct ifinfomsg ifmh;
	unsigned char attrs[256];
};

struct raw_netlink_generic_metadata {
	struct nlmsghdr nlmh;
	struct genlmsghdr genlmh;
	unsigned char attrs[256];
};


static int done = 1;
static pa_mainloop *mainloop = NULL;
static unsigned vol = 0xff;


char		*date(char *, size_t);
char		*battery_status(void);
char		*battery(char *, size_t);
char		*bluez_battery(char *, size_t);
char		*volume(char *, size_t);
char		*layout(char *, size_t, Display *);
int		 read_memory(struct meminfo *);
char		*memory(char *, size_t);
char		*temp(char *, size_t);
char		*cpu_prec(char *, size_t);
char		*wifi(char *, size_t);
char		*ethernet(char *, size_t);
char		*openvpn(char *, size_t);
char		*unitconv(char *, size_t, uint64_t);
uint64_t		 get_rx_bytes(void);
static void		 change_done(int);

#endif

