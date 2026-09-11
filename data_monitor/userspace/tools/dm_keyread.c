#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <string.h>

#include "dm_protocol.h"

int main(void)
{
	int fd = -1, i;
	char path[64], name[128];
	struct input_event event;

	for (i = 0; i < 32; i++) {
		snprintf(path, sizeof(path), "%s%d", DM_DEV_KEY_PREFIX, i);
		fd = open(path, O_RDONLY);
		if (fd >= 0 && ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 &&
		    strstr(name, "datamon-key"))
			break;
		if (fd >= 0)
			close(fd);
		fd = -1;
	}
	if (fd < 0) {
		perror("open datamon input event");
		return 1;
	}

	printf("waiting for key events on %s...\n", path);
	while (1) {
		ssize_t n = read(fd, &event, sizeof(event));

		if (n < 0) {
			perror("read");
			close(fd);
			return 1;
		}

		if (n == sizeof(event) && event.type == EV_KEY)
			printf("key event: %s\n", event.value ? "press" : "release");
	}
}
