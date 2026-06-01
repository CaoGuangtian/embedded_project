#include "net_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int net_client_connect(const char *ip, int port)
{
	struct sockaddr_in addr;
	int fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
		close(fd);
		errno = EINVAL;
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

int net_client_send_all(int fd, const char *buf, size_t len)
{
	size_t sent = 0;

	while (sent < len) {
		ssize_t n = send(fd, buf + sent, len - sent, 0);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (n == 0)
			return -1;

		sent += (size_t)n;
	}

	return 0;
}

int net_client_recv_line(int fd, char *buf, size_t len, int timeout_sec)
{
	size_t used = 0;

	if (len == 0)
		return -1;

	while (used + 1 < len) {
		fd_set rfds;
		struct timeval tv;
		char ch;
		ssize_t n;
		int ret;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = timeout_sec;
		tv.tv_usec = 0;

		ret = select(fd + 1, &rfds, NULL, NULL,
			     timeout_sec >= 0 ? &tv : NULL);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (ret == 0)
			return 0;

		n = recv(fd, &ch, 1, 0);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (n == 0)
			return -1;

		if (ch == '\n') {
			buf[used] = '\0';
			return (int)used;
		}

		buf[used++] = ch;
	}

	buf[used] = '\0';
	return (int)used;
}

