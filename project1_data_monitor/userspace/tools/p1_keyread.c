#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "p1_protocol.h"

int main(void)
{
	int fd;
	unsigned char event;

	fd = open(P1_DEV_KEY, O_RDONLY);
	if (fd < 0) {
		perror("open " P1_DEV_KEY);
		return 1;
	}

	printf("waiting for key events on %s...\n", P1_DEV_KEY);
	while (1) {
		ssize_t n = read(fd, &event, sizeof(event));

		if (n < 0) {
			perror("read");
			close(fd);
			return 1;
		}

		if (n == sizeof(event))
			printf("key event: %s\n", event ? "press" : "release");
	}
}

