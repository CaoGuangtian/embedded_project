#ifndef DM_PROTOCOL_H
#define DM_PROTOCOL_H

#include <stdint.h>

#define DM_DEV_LED "/sys/class/leds/datamon:green/brightness"
#define DM_DEV_BEEP "/dev/dm_beep"
#define DM_DEV_KEY_PREFIX "/dev/input/event"
#define DM_IIO_ROOT "/sys/bus/iio/devices"
#define DM_IIO_AP3216C "dm-ap3216c"
#define DM_IIO_ICM20608 "dm-icm20608"

#define DM_DEFAULT_SERVER_IP "192.168.10.100"
#define DM_DEFAULT_SERVER_PORT 9000
#define DM_DEFAULT_INTERVAL_MS 1000
#define DM_DEFAULT_CONFIG_PATH "/etc/datamon/datamon.env"
#define DM_DEFAULT_LOG_PATH "/mnt/tf/datamon_samples.csv"
#define DM_DEFAULT_MAX_LOG_KB 1024
#define DM_DEFAULT_PS_THRESHOLD 1000
#define DM_DEFAULT_ALS_THRESHOLD 60000
#define DM_DEFAULT_FILTER_ALPHA_PERCENT 35
#define DM_LOG_QUERY_MAX_LINES 20

enum dm_work_mode {
	DM_MODE_NORMAL = 0,
	DM_MODE_QUIET = 1,
	DM_MODE_ALARM_ONLY = 2,
};

struct dm_ap3216c_sample {
	uint16_t ir;
	uint16_t als;
	uint16_t ps;
};

struct dm_icm20608_sample {
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	int16_t temp;
	int16_t gyro_x;
	int16_t gyro_y;
	int16_t gyro_z;
};

static inline const char *dm_mode_name(enum dm_work_mode mode)
{
	switch (mode) {
	case DM_MODE_NORMAL:
		return "normal";
	case DM_MODE_QUIET:
		return "quiet";
	case DM_MODE_ALARM_ONLY:
		return "alarm_only";
	default:
		return "unknown";
	}
}

#endif
