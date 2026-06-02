#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stddef.h>

int ota_manager_prepare_package(const char *target, const char *version,
				const char *url, const char *sha256,
				char *msg, size_t msg_len);

#endif
