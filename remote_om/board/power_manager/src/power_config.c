#include "power_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_line(char *text)
{
	char *start = text;
	char *end;

	while (*start == ' ' || *start == '\t')
		start++;
	if (start != text)
		memmove(text, start, strlen(start) + 1);

	end = text + strlen(text);
	while (end > text &&
	       (end[-1] == '\n' || end[-1] == '\r' ||
		end[-1] == ' ' || end[-1] == '\t')) {
		end--;
		*end = '\0';
	}
}

static int parse_int(const char *text, int *out)
{
	char *end = NULL;
	long value;

	errno = 0;
	value = strtol(text, &end, 10);
	if (errno || end == text || *end != '\0')
		return -1;

	*out = (int)value;
	return 0;
}

void power_config_defaults(struct power_config *cfg)
{
	snprintf(cfg->config_path, sizeof(cfg->config_path), "%s",
		 POWER_DEFAULT_CONFIG_PATH);
	snprintf(cfg->state_path, sizeof(cfg->state_path), "%s",
		 POWER_DEFAULT_STATE_PATH);
	snprintf(cfg->log_path, sizeof(cfg->log_path), "%s",
		 POWER_DEFAULT_LOG_PATH);
	snprintf(cfg->default_mode, sizeof(cfg->default_mode), "%s",
		 POWER_DEFAULT_MODE);
	cfg->allow_suspend = 0;
	cfg->normal_heartbeat_interval = 5;
	cfg->low_power_heartbeat_interval = 60;
	snprintf(cfg->wakeup_source, sizeof(cfg->wakeup_source), "%s",
		 "rtc,key");
}

static void config_set_value(struct power_config *cfg, const char *key,
			     const char *value)
{
	int parsed;

	if (!strcmp(key, "default_mode")) {
		snprintf(cfg->default_mode, sizeof(cfg->default_mode), "%s",
			 value);
	} else if (!strcmp(key, "allow_suspend")) {
		if (!strcmp(value, "true") || !strcmp(value, "1"))
			cfg->allow_suspend = 1;
		else
			cfg->allow_suspend = 0;
	} else if (!strcmp(key, "normal_heartbeat_interval")) {
		if (parse_int(value, &parsed) == 0)
			cfg->normal_heartbeat_interval = parsed;
	} else if (!strcmp(key, "low_power_heartbeat_interval")) {
		if (parse_int(value, &parsed) == 0)
			cfg->low_power_heartbeat_interval = parsed;
	} else if (!strcmp(key, "wakeup_source")) {
		snprintf(cfg->wakeup_source, sizeof(cfg->wakeup_source), "%s",
			 value);
	} else if (!strcmp(key, "state_path")) {
		snprintf(cfg->state_path, sizeof(cfg->state_path), "%s", value);
	} else if (!strcmp(key, "log_path")) {
		snprintf(cfg->log_path, sizeof(cfg->log_path), "%s", value);
	}
}

int power_config_load(struct power_config *cfg, const char *path)
{
	FILE *fp;
	char line[256];

	fp = fopen(path, "r");
	if (!fp)
		return -1;

	while (fgets(line, sizeof(line), fp)) {
		char *eq;

		trim_line(line);
		if (!line[0] || line[0] == '#')
			continue;

		eq = strchr(line, '=');
		if (!eq)
			continue;

		*eq = '\0';
		trim_line(line);
		trim_line(eq + 1);
		config_set_value(cfg, line, eq + 1);
	}

	fclose(fp);
	return 0;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"usage: %s [-c config] [--get-mode] [--set-mode mode]\n",
		prog);
}

int power_config_parse_args(struct power_config *cfg, int argc, char **argv,
			    const char **set_mode, int *get_mode)
{
	int i;

	power_config_defaults(cfg);
	*set_mode = NULL;
	*get_mode = 0;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c") && i + 1 < argc) {
			snprintf(cfg->config_path, sizeof(cfg->config_path),
				 "%s", argv[i + 1]);
			break;
		}
	}

	power_config_load(cfg, cfg->config_path);

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c") && i + 1 < argc) {
			snprintf(cfg->config_path, sizeof(cfg->config_path),
				 "%s", argv[++i]);
		} else if (!strcmp(argv[i], "--get-mode")) {
			*get_mode = 1;
		} else if (!strcmp(argv[i], "--set-mode") && i + 1 < argc) {
			*set_mode = argv[++i];
		} else {
			usage(argv[0]);
			return -1;
		}
	}

	return 0;
}

