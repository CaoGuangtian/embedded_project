#include "agent_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_line(char *text)
{
	char *start = text;
	char *end;

	while (*start == ' ' || *start == '\t')
		start++;
	if (start != text)
		memmove(text, start, strlen(start) + 1);

	end = text + strlen(text);
	while (end > text &&
	       (end[-1] == '\n' || end[-1] == '\r' ||
		end[-1] == ' ' || end[-1] == '\t')) {
		end--;
		*end = '\0';
	}
}

static int parse_int(const char *text, int *out)
{
	char *end = NULL;
	long value;

	errno = 0;
	value = strtol(text, &end, 10);
	if (errno || end == text || *end != '\0')
		return -1;
	if (value < 0 || value > 65535)
		return -1;

	*out = (int)value;
	return 0;
}

void agent_config_defaults(struct agent_config *cfg)
{
	snprintf(cfg->device_id, sizeof(cfg->device_id), "%s",
		 P2_DEFAULT_DEVICE_ID);
	snprintf(cfg->server_ip, sizeof(cfg->server_ip), "%s",
		 P2_DEFAULT_SERVER_IP);
	cfg->server_port = P2_DEFAULT_SERVER_PORT;
	cfg->heartbeat_interval = P2_DEFAULT_HEARTBEAT_INTERVAL;
	cfg->status_interval = P2_DEFAULT_STATUS_INTERVAL;
	cfg->reconnect_interval = P2_DEFAULT_RECONNECT_INTERVAL;
	cfg->max_log_kb = P2_DEFAULT_MAX_LOG_KB;
	snprintf(cfg->net_ifname, sizeof(cfg->net_ifname), "%s",
		 P2_DEFAULT_NET_IFNAME);
	snprintf(cfg->log_path, sizeof(cfg->log_path), "%s",
		 P2_DEFAULT_LOG_PATH);
	snprintf(cfg->fw_version, sizeof(cfg->fw_version), "%s",
		 P2_DEFAULT_FW_VERSION);
	snprintf(cfg->config_path, sizeof(cfg->config_path), "%s",
		 P2_DEFAULT_CONFIG_PATH);
}

static void config_set_value(struct agent_config *cfg, const char *key,
			     const char *value)
{
	int parsed;

	if (!strcmp(key, "device_id")) {
		snprintf(cfg->device_id, sizeof(cfg->device_id), "%s", value);
	} else if (!strcmp(key, "server_ip")) {
		snprintf(cfg->server_ip, sizeof(cfg->server_ip), "%s", value);
	} else if (!strcmp(key, "server_port")) {
		if (parse_int(value, &parsed) == 0)
			cfg->server_port = parsed;
	} else if (!strcmp(key, "heartbeat_interval")) {
		if (parse_int(value, &parsed) == 0)
			cfg->heartbeat_interval = parsed;
	} else if (!strcmp(key, "status_interval")) {
		if (parse_int(value, &parsed) == 0)
			cfg->status_interval = parsed;
	} else if (!strcmp(key, "reconnect_interval")) {
		if (parse_int(value, &parsed) == 0)
			cfg->reconnect_interval = parsed;
	} else if (!strcmp(key, "net_ifname")) {
		snprintf(cfg->net_ifname, sizeof(cfg->net_ifname), "%s", value);
	} else if (!strcmp(key, "log_path")) {
		snprintf(cfg->log_path, sizeof(cfg->log_path), "%s", value);
	} else if (!strcmp(key, "max_log_kb")) {
		if (parse_int(value, &parsed) == 0)
			cfg->max_log_kb = parsed;
	} else if (!strcmp(key, "fw_version")) {
		snprintf(cfg->fw_version, sizeof(cfg->fw_version), "%s", value);
	}
}

int agent_config_load(struct agent_config *cfg, const char *path)
{
	FILE *fp;
	char line[256];

	fp = fopen(path, "r");
	if (!fp)
		return -1;

	while (fgets(line, sizeof(line), fp)) {
		char *eq;

		trim_line(line);
		if (!line[0] || line[0] == '#')
			continue;

		eq = strchr(line, '=');
		if (!eq)
			continue;

		*eq = '\0';
		trim_line(line);
		trim_line(eq + 1);
		config_set_value(cfg, line, eq + 1);
	}

	fclose(fp);
	return 0;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"usage: %s [-c config] [-s server_ip] [-p port] "
		"[-d device_id] [-v fw_version] "
		"[--heartbeat seconds] [--status seconds] "
		"[--reconnect seconds] [--net-ifname name] "
		"[--log-path path] [--max-log-kb kb]\n",
		prog);
}

int agent_config_parse_args(struct agent_config *cfg, int argc, char **argv)
{
	int i;

	agent_config_defaults(cfg);

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c") && i + 1 < argc) {
			snprintf(cfg->config_path, sizeof(cfg->config_path),
				 "%s", argv[i + 1]);
			break;
		}
	}

	agent_config_load(cfg, cfg->config_path);

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-c") && i + 1 < argc) {
			snprintf(cfg->config_path, sizeof(cfg->config_path),
				 "%s", argv[++i]);
		} else if (!strcmp(argv[i], "-s") && i + 1 < argc) {
			snprintf(cfg->server_ip, sizeof(cfg->server_ip), "%s",
				 argv[++i]);
		} else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
			if (parse_int(argv[++i], &cfg->server_port))
				goto bad_args;
		} else if (!strcmp(argv[i], "-d") && i + 1 < argc) {
			snprintf(cfg->device_id, sizeof(cfg->device_id), "%s",
				 argv[++i]);
		} else if (!strcmp(argv[i], "-v") && i + 1 < argc) {
			snprintf(cfg->fw_version, sizeof(cfg->fw_version), "%s",
				 argv[++i]);
		} else if (!strcmp(argv[i], "--heartbeat") && i + 1 < argc) {
			if (parse_int(argv[++i], &cfg->heartbeat_interval))
				goto bad_args;
		} else if (!strcmp(argv[i], "--status") && i + 1 < argc) {
			if (parse_int(argv[++i], &cfg->status_interval))
				goto bad_args;
		} else if (!strcmp(argv[i], "--reconnect") && i + 1 < argc) {
			if (parse_int(argv[++i], &cfg->reconnect_interval))
				goto bad_args;
		} else if (!strcmp(argv[i], "--net-ifname") && i + 1 < argc) {
			snprintf(cfg->net_ifname, sizeof(cfg->net_ifname), "%s",
				 argv[++i]);
		} else if (!strcmp(argv[i], "--log-path") && i + 1 < argc) {
			snprintf(cfg->log_path, sizeof(cfg->log_path), "%s",
				 argv[++i]);
		} else if (!strcmp(argv[i], "--max-log-kb") && i + 1 < argc) {
			if (parse_int(argv[++i], &cfg->max_log_kb))
				goto bad_args;
		} else {
			goto bad_args;
		}
	}

	if (cfg->server_port <= 0 || cfg->server_port > 65535)
		goto bad_args;
	if (cfg->heartbeat_interval < 1)
		cfg->heartbeat_interval = 1;
	if (cfg->status_interval < 1)
		cfg->status_interval = 1;
	if (cfg->reconnect_interval < 1)
		cfg->reconnect_interval = 1;
	if (cfg->max_log_kb < 1)
		cfg->max_log_kb = 1;

	return 0;

bad_args:
	usage(argv[0]);
	return -1;
}
