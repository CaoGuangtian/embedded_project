#ifndef POWER_CLIENT_H
#define POWER_CLIENT_H

#include <stddef.h>

int power_client_get_mode(char *msg, size_t msg_len);
int power_client_set_mode(const char *mode, char *msg, size_t msg_len);

#endif

