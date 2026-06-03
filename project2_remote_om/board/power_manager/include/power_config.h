#ifndef POWER_CONFIG_H
#define POWER_CONFIG_H

#define POWER_PATH_MAX 256
#define POWER_MODE_MAX 32

#define POWER_DEFAULT_CONFIG_PATH "/etc/power_manager/power_manager.conf"
#define POWER_DEFAULT_STATE_PATH "/var/run/power_manager.mode"
#define POWER_DEFAULT_LOG_PATH "/var/log/power_manager/power_manager.log"
#define POWER_DEFAULT_MODE "normal"

struct power_config {
	char config_path[POWER_PATH_MAX];
	char state_path[POWER_PATH_MAX];
	char log_path[POWER_PATH_MAX];
	char default_mode[POWER_MODE_MAX];
	int allow_suspend;
	int normal_heartbeat_interval;
	int low_power_heartbeat_interval;
	char wakeup_source[64];
};

void power_config_defaults(struct power_config *cfg);
int power_config_load(struct power_config *cfg, const char *path);
int power_config_parse_args(struct power_config *cfg, int argc, char **argv,
			    const char **set_mode, int *get_mode);

#endif

