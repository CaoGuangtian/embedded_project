#ifndef POWER_LOG_H
#define POWER_LOG_H

int power_log_init(const char *path);
void power_log_close(void);
void power_log_info(const char *fmt, ...);
void power_log_error(const char *fmt, ...);

#endif

