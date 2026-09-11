#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *prog)
{
	fprintf(stderr, "usage: %s /sys/class/leds/datamon:green/brightness|/dev/dm_beep 0|1\n", prog);
}

int main(int argc, char **argv)
{
	int fd;
	char value;

	if (argc != 3) {
		usage(argv[0]);
		return 1;
	}

	if (strcmp(argv[2], "0") != 0 && strcmp(argv[2], "1") != 0) {
		usage(argv[0]);
		return 1;
	}

	value = argv[2][0];
	fd = open(argv[1], O_WRONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	if (write(fd, &value, 1) != 1) {
		perror("write");
		close(fd);
		return 1;
	}

	close(fd);
	return 0;
}
