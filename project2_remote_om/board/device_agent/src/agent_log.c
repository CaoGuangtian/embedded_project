#include "agent_log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define LOG_LINE_MAX 512
#define LOG_TAIL_MAX_LINES 100

static FILE *g_log_fp;
static char g_log_path[256];
static int g_max_log_kb;

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

static void rotate_if_needed(void)
{
	long pos;
	char rotated[300];

	if (!g_log_fp || g_max_log_kb <= 0)
		return;

	pos = ftell(g_log_fp);
	if (pos < 0 || pos < (long)g_max_log_kb * 1024)
		return;

	fclose(g_log_fp);
	g_log_fp = NULL;

	snprintf(rotated, sizeof(rotated), "%s.1", g_log_path);
	rename(g_log_path, rotated);
	g_log_fp = fopen(g_log_path, "a");
}

int agent_log_init(const char *path, int max_kb)
{
	snprintf(g_log_path, sizeof(g_log_path), "%s", path);
	g_max_log_kb = max_kb;

	ensure_parent_dir(g_log_path);
	g_log_fp = fopen(g_log_path, "a");
	if (!g_log_fp)
		return -1;

	return 0;
}

void agent_log_close(void)
{
	if (g_log_fp) {
		fclose(g_log_fp);
		g_log_fp = NULL;
	}
}

static void log_write(const char *level, const char *fmt, va_list ap)
{
	char msg[LOG_LINE_MAX];
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
	fflush(stdout);

	if (g_log_fp) {
		fprintf(g_log_fp, "[%s] [%s] %s\n", ts, level, msg);
		fflush(g_log_fp);
		rotate_if_needed();
	}
}

void agent_log_info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_write("INFO", fmt, ap);
	va_end(ap);
}

void agent_log_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_write("ERROR", fmt, ap);
	va_end(ap);
}

void agent_log_debug(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_write("DEBUG", fmt, ap);
	va_end(ap);
}

int agent_log_tail(int lines, void (*cb)(int index, const char *line, void *arg),
		   void *arg)
{
	FILE *fp;
	char ring[LOG_TAIL_MAX_LINES][LOG_LINE_MAX];
	int count = 0;
	int start;
	int emit;
	char line[LOG_LINE_MAX];

	if (lines < 1)
		lines = 1;
	if (lines > LOG_TAIL_MAX_LINES)
		lines = LOG_TAIL_MAX_LINES;

	fp = fopen(g_log_path, "r");
	if (!fp)
		return -1;

	while (fgets(line, sizeof(line), fp)) {
		size_t len = strlen(line);

		while (len > 0 &&
		       (line[len - 1] == '\n' || line[len - 1] == '\r')) {
			line[--len] = '\0';
		}
		snprintf(ring[count % lines], sizeof(ring[0]), "%s", line);
		count++;
	}

	fclose(fp);

	start = count > lines ? count % lines : 0;
	emit = count > lines ? lines : count;
	for (int i = 0; i < emit; i++) {
		int pos = (start + i) % lines;
		cb(i, ring[pos], arg);
	}

	return 0;
}
