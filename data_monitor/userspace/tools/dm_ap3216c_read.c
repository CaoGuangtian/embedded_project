#include <stdio.h>
#include "dm_iio.h"

int main(void)
{
	int ir, als, ps;

	if (dm_iio_read_attr(DM_IIO_AP3216C, "in_intensity0_raw", &ir) ||
	    dm_iio_read_attr(DM_IIO_AP3216C, "in_illuminance0_raw", &als) ||
	    dm_iio_read_attr(DM_IIO_AP3216C, "in_proximity0_raw", &ps)) {
		perror("read AP3216C IIO attributes");
		return 1;
	}
	printf("ap3216c: ir=%d als=%d ps=%d\n", ir, als, ps);
	return 0;
}
