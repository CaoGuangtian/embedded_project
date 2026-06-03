#ifndef POWER_MODE_H
#define POWER_MODE_H

#include "power_config.h"

enum power_mode {
	POWER_MODE_NORMAL = 0,
	POWER_MODE_IDLE,
	POWER_MODE_LOW_POWER,
	POWER_MODE_SLEEP,
	POWER_MODE_MAINTENANCE,
};

const char *power_mode_name(enum power_mode mode);
int power_mode_parse(const char *name, enum power_mode *mode);
int power_mode_apply(const struct power_config *cfg, enum power_mode mode);
int power_mode_save_state(const struct power_config *cfg, enum power_mode mode);
int power_mode_load_state(const struct power_config *cfg, enum power_mode *mode);

#endif

