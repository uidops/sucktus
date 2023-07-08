#ifndef CONFIG_H
#define CONFIG_H

#include <pulse/mainloop.h>
#include <X11/Xlib.h>
#include <stdio.h>

#define VERSION "v1.3"

#define ICON ""
#define UNKNOWN ""
#define INTERVAL 1000

#define BAT "/sys/class/power_supply/BAT1"
#define CARD "default"
#define SND_CARD "Master"
#define SND_INDEX 0
#define MEMINFO "/proc/meminfo"
#define TEMP "/sys/class/thermal/thermal_zone1/temp"
#define STAT "/proc/stat"
#define BLZDEV "/org/bluez/hci0/dev_00_00_00_00_00_00"

static char *IW[] = {"wlp2s0", NULL};
static char *ET[] = {"enp1s0", "enp0s20f0u3", NULL};
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
static void		 change_done(int);

#endif

