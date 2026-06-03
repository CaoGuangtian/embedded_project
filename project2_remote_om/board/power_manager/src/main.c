#include "power_config.h"
#include "power_log.h"
#include "power_mode.h"

#include <stdio.h>

int main(int argc, char **argv)
{
	struct power_config cfg;
	enum power_mode mode;
	const char *set_mode;
	int get_mode;
	int ret = 0;

	if (power_config_parse_args(&cfg, argc, argv, &set_mode, &get_mode))
		return 1;

	if (power_log_init(cfg.log_path))
		fprintf(stderr, "warning: log init failed: %s\n", cfg.log_path);

	if (set_mode) {
		if (power_mode_parse(set_mode, &mode)) {
			power_log_error("bad mode: %s", set_mode);
			ret = 1;
			goto out;
		}

		if (power_mode_apply(&cfg, mode)) {
			power_log_error("apply mode failed: %s", set_mode);
			ret = 1;
			goto out;
		}

		printf("%s\n", power_mode_name(mode));
		goto out;
	}

	if (get_mode) {
		if (power_mode_load_state(&cfg, &mode)) {
			power_log_error("load mode failed");
			ret = 1;
			goto out;
		}

		printf("%s\n", power_mode_name(mode));
		goto out;
	}

	if (power_mode_parse(cfg.default_mode, &mode))
		mode = POWER_MODE_NORMAL;
	ret = power_mode_apply(&cfg, mode);

out:
	power_log_close();
	return ret;
}

