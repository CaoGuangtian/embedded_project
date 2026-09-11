#ifndef DM_IIO_H
#define DM_IIO_H

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "dm_protocol.h"

static inline int dm_iio_read_attr(const char *name, const char *attr,
				   int *value)
{
	DIR *dir = opendir(DM_IIO_ROOT);
	struct dirent *entry;
	char name_path[256];
	char attr_path[300];
	char device_name[128];
	FILE *fp;
	int ret = -1;

	if (!dir)
		return -1;
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, "iio:device", 10))
			continue;
		snprintf(name_path, sizeof(name_path), "%s/%s/name",
			 DM_IIO_ROOT, entry->d_name);
		fp = fopen(name_path, "r");
		if (!fp)
			continue;
		if (!fgets(device_name, sizeof(device_name), fp)) {
			fclose(fp);
			continue;
		}
		fclose(fp);
		device_name[strcspn(device_name, "\r\n")] = '\0';
		if (strcmp(device_name, name))
			continue;
		snprintf(attr_path, sizeof(attr_path), "%s/%s/%s", DM_IIO_ROOT,
			 entry->d_name, attr);
		fp = fopen(attr_path, "r");
		if (fp && fscanf(fp, "%d", value) == 1)
			ret = 0;
		if (fp)
			fclose(fp);
		break;
	}
	closedir(dir);
	return ret;
}

#endif
