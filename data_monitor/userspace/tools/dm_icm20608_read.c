#include <stdio.h>
#include "dm_iio.h"

int main(void)
{
	const char *attrs[] = { "in_accel_x_raw", "in_accel_y_raw",
		"in_accel_z_raw", "in_temp0_raw", "in_anglvel_x_raw",
		"in_anglvel_y_raw", "in_anglvel_z_raw" };
	int values[7], i;

	for (i = 0; i < 7; i++) {
		if (dm_iio_read_attr(DM_IIO_ICM20608, attrs[i], &values[i])) {
			perror("read ICM20608 IIO attributes");
			return 1;
		}
	}
	printf("icm20608: acc=(%d,%d,%d) temp=%d gyro=(%d,%d,%d)\n",
	       values[0], values[1], values[2], values[3], values[4],
	       values[5], values[6]);
	return 0;
}
