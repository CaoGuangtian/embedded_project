#include "power_mode.h"

#include "power_log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

const char *power_mode_name(enum power_mode mode)
{
	switch (mode) {
	case POWER_MODE_NORMAL:
		return "normal";
	case POWER_MODE_IDLE:
		return "idle";
	case POWER_MODE_LOW_POWER:
		return "low_power";
	case POWER_MODE_SLEEP:
		return "sleep";
	case POWER_MODE_MAINTENANCE:
		return "maintenance";
	default:
		return "unknown";
	}
}

int power_mode_parse(const char *name, enum power_mode *mode)
{
	if (!strcmp(name, "normal"))
		*mode = POWER_MODE_NORMAL;
	else if (!strcmp(name, "idle"))
		*mode = POWER_MODE_IDLE;
	else if (!strcmp(name, "low_power"))
		*mode = POWER_MODE_LOW_POWER;
	else if (!strcmp(name, "sleep"))
		*mode = POWER_MODE_SLEEP;
	else if (!strcmp(name, "maintenance"))
		*mode = POWER_MODE_MAINTENANCE;
	else
		return -1;

	return 0;
}

static int ensure_parent_dir(const char *path)
{
	char tmp[POWER_PATH_MAX];
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

int power_mode_save_state(const struct power_config *cfg, enum power_mode mode)
{
	FILE *fp;

	ensure_parent_dir(cfg->state_path);
	fp = fopen(cfg->state_path, "w");
	if (!fp)
		return -1;

	fprintf(fp, "%s\n", power_mode_name(mode));
	fclose(fp);
	return 0;
}

int power_mode_load_state(const struct power_config *cfg, enum power_mode *mode)
{
	FILE *fp;
	char line[POWER_MODE_MAX];
	size_t len;

	fp = fopen(cfg->state_path, "r");
	if (!fp)
		return power_mode_parse(cfg->default_mode, mode);

	if (!fgets(line, sizeof(line), fp)) {
		fclose(fp);
		return power_mode_parse(cfg->default_mode, mode);
	}
	fclose(fp);

	len = strlen(line);
	while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
			   line[len - 1] == ' ' || line[len - 1] == '\t')) {
		line[--len] = '\0';
	}

	if (power_mode_parse(line, mode))
		return power_mode_parse(cfg->default_mode, mode);

	return 0;
}

int power_mode_apply(const struct power_config *cfg, enum power_mode mode)
{
	power_log_info("apply mode=%s allow_suspend=%d wakeup=%s",
		       power_mode_name(mode), cfg->allow_suspend,
		       cfg->wakeup_source);

	switch (mode) {
	case POWER_MODE_NORMAL:
		power_log_info("normal mode: all services may run normally");
		break;
	case POWER_MODE_IDLE:
		power_log_info("idle mode: reduce periodic work in future stage");
		break;
	case POWER_MODE_LOW_POWER:
		power_log_info("low_power mode: stop non-essential services in future stage");
		break;
	case POWER_MODE_SLEEP:
		if (cfg->allow_suspend)
			power_log_info("sleep mode requested: suspend hook reserved");
		else
			power_log_info("sleep mode requested but allow_suspend=false");
		break;
	case POWER_MODE_MAINTENANCE:
		power_log_info("maintenance mode: keep network and OTA available");
		break;
	default:
		return -1;
	}

	return power_mode_save_state(cfg, mode);
}

