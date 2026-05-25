#ifndef P1_PROTOCOL_H
#define P1_PROTOCOL_H

#include <stdint.h>

#define P1_DEV_LED "/dev/p1_led"
#define P1_DEV_BEEP "/dev/p1_beep"
#define P1_DEV_KEY "/dev/p1_key"
#define P1_DEV_AP3216C "/dev/p1_ap3216c"
#define P1_DEV_ICM20608 "/dev/p1_icm20608"

#define P1_DEFAULT_SERVER_IP "192.168.10.100"
#define P1_DEFAULT_SERVER_PORT 9000
#define P1_DEFAULT_INTERVAL_MS 1000
#define P1_DEFAULT_CONFIG_PATH "/etc/project1/project1.env"
#define P1_DEFAULT_LOG_PATH "/mnt/tf/project1_samples.csv"
#define P1_DEFAULT_MAX_LOG_KB 1024
#define P1_DEFAULT_PS_THRESHOLD 1000
#define P1_DEFAULT_ALS_THRESHOLD 60000
#define P1_DEFAULT_FILTER_ALPHA_PERCENT 35
#define P1_LOG_QUERY_MAX_LINES 20

enum p1_work_mode {
	P1_MODE_NORMAL = 0,
	P1_MODE_QUIET = 1,
	P1_MODE_ALARM_ONLY = 2,
};

struct p1_ap3216c_sample {
	uint16_t ir;
	uint16_t als;
	uint16_t ps;
};

struct p1_icm20608_sample {
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	int16_t temp;
	int16_t gyro_x;
	int16_t gyro_y;
	int16_t gyro_z;
};

static inline const char *p1_mode_name(enum p1_work_mode mode)
{
	switch (mode) {
	case P1_MODE_NORMAL:
		return "normal";
	case P1_MODE_QUIET:
		return "quiet";
	case P1_MODE_ALARM_ONLY:
		return "alarm_only";
	default:
		return "unknown";
	}
}

#endif
