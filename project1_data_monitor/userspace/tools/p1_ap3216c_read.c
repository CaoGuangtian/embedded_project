#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "p1_protocol.h"

int main(void)
{
	struct p1_ap3216c_sample sample;
	int fd;

	fd = open(P1_DEV_AP3216C, O_RDONLY);
	if (fd < 0) {
		perror("open " P1_DEV_AP3216C);
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

