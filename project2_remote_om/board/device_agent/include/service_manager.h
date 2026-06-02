#ifndef SERVICE_MANAGER_H
#define SERVICE_MANAGER_H

#include <stddef.h>

int service_manager_handle(const char *service, const char *action,
			   char *msg, size_t msg_len);

#endif

