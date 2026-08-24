#include "power_log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static FILE *g_log_fp;
static char g_log_path[256];

static int ensure_parent_dir(const char *path)
{
	char tmp[256];
	char *slash;

	snprintf(tmp, sizeof(tmp), "%s", path);
	slash = strrchr(tmp, '/');
	if (!slash || slash == tmp)
		return 0;

	*slash = '\0';
	if (mkdir(tmp, 0755) == 0 || errno == EEXIST)
		return 0;

	return -1;
}

int power_log_init(const char *path)
{
	snprintf(g_log_path, sizeof(g_log_path), "%s", path);
	ensure_parent_dir(g_log_path);
	g_log_fp = fopen(g_log_path, "a");
	return g_log_fp ? 0 : -1;
}

void power_log_close(void)
{
	if (g_log_fp) {
		fclose(g_log_fp);
		g_log_fp = NULL;
	}
}

static void log_write(const char *level, const char *fmt, va_list ap)
{
	char msg[512];
	char ts[32];
	time_t now = time(NULL);
	struct tm tm_now;
	struct tm *tm_ptr;
	va_list copy;

	tm_ptr = localtime(&now);
	if (tm_ptr)
		tm_now = *tm_ptr;
	else
		memset(&tm_now, 0, sizeof(tm_now));
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);

	va_copy(copy, ap);
	vsnprintf(msg, sizeof(msg), fmt, copy);
	va_end(copy);

	printf("[%s] [%s] %s\n", ts, level, msg);
	if (g_log_fp) {
		fprintf(g_log_fp, "[%s] [%s] %s\n", ts, level, msg);
		fflush(g_log_fp);
	}
}

void power_log_info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_write("INFO", fmt, ap);
	va_end(ap);
}

void power_log_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_write("ERROR", fmt, ap);
	va_end(ap);
}

