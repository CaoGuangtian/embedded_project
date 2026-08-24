#include "status.h"

#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>

static void trim_line(char *text)
{
	char *end;

	end = text + strlen(text);
	while (end > text &&
	       (end[-1] == '\n' || end[-1] == '\r' ||
		end[-1] == ' ' || end[-1] == '\t')) {
		end--;
		*end = '\0';
	}
}

static long read_uptime_sec(void)
{
	FILE *fp;
	double uptime = 0.0;

	fp = fopen("/proc/uptime", "r");
	if (!fp)
		return -1;

	if (fscanf(fp, "%lf", &uptime) != 1)
		uptime = -1.0;

	fclose(fp);
	return uptime < 0.0 ? -1 : (long)uptime;
}

static void read_meminfo(long *total_kb, long *available_kb)
{
	FILE *fp;
	char key[64];
	long value;
	char unit[32];

	*total_kb = -1;
	*available_kb = -1;

	fp = fopen("/proc/meminfo", "r");
	if (!fp)
		return;

	while (fscanf(fp, "%63s %ld %31s", key, &value, unit) == 3) {
		if (!strcmp(key, "MemTotal:"))
			*total_kb = value;
		else if (!strcmp(key, "MemAvailable:"))
			*available_kb = value;
	}

	fclose(fp);
}

static int read_rootfs_usage_percent(void)
{
	struct statvfs vfs;
	unsigned long long total;
	unsigned long long available;
	unsigned long long used;

	if (statvfs("/", &vfs) < 0)
		return -1;

	total = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
	available = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
	if (total == 0)
		return -1;

	used = total - available;
	return (int)((used * 100) / total);
}

static void read_text_file(const char *path, char *buf, size_t len,
			   const char *fallback)
{
	FILE *fp;

	if (len == 0)
		return;

	snprintf(buf, len, "%s", fallback);
	fp = fopen(path, "r");
	if (!fp)
		return;

	if (fgets(buf, len, fp))
		trim_line(buf);
	else
		snprintf(buf, len, "%s", fallback);

	fclose(fp);
}

void status_collect(const struct agent_config *cfg, struct agent_status *st)
{
	char path[256];

	memset(st, 0, sizeof(*st));
	st->uptime_sec = read_uptime_sec();
	read_meminfo(&st->mem_total_kb, &st->mem_available_kb);
	st->rootfs_usage_percent = read_rootfs_usage_percent();

	snprintf(path, sizeof(path), "/sys/class/net/%s/operstate",
		 cfg->net_ifname);
	read_text_file(path, st->net_state, sizeof(st->net_state), "unknown");

	snprintf(path, sizeof(path), "/sys/class/net/%s/address",
		 cfg->net_ifname);
	read_text_file(path, st->mac_addr, sizeof(st->mac_addr), "unknown");
}

