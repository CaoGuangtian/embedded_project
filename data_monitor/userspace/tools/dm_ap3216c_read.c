#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "dm_protocol.h"

int main(void)
{
	struct dm_ap3216c_sample sample;
	int fd;

	fd = open(DM_DEV_AP3216C, O_RDONLY);
	if (fd < 0) {
		perror("open " DM_DEV_AP3216C);
		return 1;
	}

	if (read(fd, &sample, sizeof(sample)) != sizeof(sample)) {
		perror("read");
		close(fd);
		return 1;
	}

	printf("ap3216c: ir=%u als=%u ps=%u\n",
	       sample.ir, sample.als, sample.ps);

	close(fd);
	return 0;
}
