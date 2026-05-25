#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "p1_protocol.h"

int main(void)
{
	struct p1_icm20608_sample sample;
	int fd;

	fd = open(P1_DEV_ICM20608, O_RDONLY);
	if (fd < 0) {
		perror("open " P1_DEV_ICM20608);
		return 1;
	}

	if (read(fd, &sample, sizeof(sample)) != sizeof(sample)) {
		perror("read");
		close(fd);
		return 1;
	}

	printf("icm20608: acc=(%d,%d,%d) temp=%d gyro=(%d,%d,%d)\n",
	       sample.accel_x, sample.accel_y, sample.accel_z, sample.temp,
	       sample.gyro_x, sample.gyro_y, sample.gyro_z);

	close(fd);
	return 0;
}

