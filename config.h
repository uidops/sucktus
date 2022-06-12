#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "v1.0"

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
#define IW "wlan0"
#define ET "enp1s0"
static char *vpn_interfaces[] = {"tun0", "proton0", NULL};

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


char            *date(char *, size_t);
char            *battery_status(void);
char            *battery(char *, size_t);
char            *volume(char *, size_t);
char            *layout(char *, size_t, Display *);
int              read_memory(struct meminfo *);
char            *memory(char *, size_t);
char            *temp(char *, size_t);
char            *cpu_prec(char *, size_t);
char            *wifi(char *, size_t);
char            *ethernet(char *, size_t);
char            *openvpn(char *, size_t);
static void      change_done(int);

#endif

