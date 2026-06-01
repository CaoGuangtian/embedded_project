#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include <stddef.h>

int net_client_connect(const char *ip, int port);
int net_client_send_all(int fd, const char *buf, size_t len);
int net_client_recv_line(int fd, char *buf, size_t len, int timeout_sec);

#endif

