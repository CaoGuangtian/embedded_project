#ifndef AGENT_LOG_H
#define AGENT_LOG_H

#include <stddef.h>

int agent_log_init(const char *path, int max_kb);
void agent_log_close(void);
void agent_log_info(const char *fmt, ...);
void agent_log_error(const char *fmt, ...);
void agent_log_debug(const char *fmt, ...);
int agent_log_tail(int lines, void (*cb)(int index, const char *line, void *arg),
		   void *arg);

#endif

