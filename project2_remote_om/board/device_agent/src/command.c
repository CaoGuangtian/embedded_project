#include "command.h"

#include "agent_config.h"
#include "service_manager.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *json_find_value(const char *line, const char *key)
{
	char pattern[64];
	const char *p;

	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	p = strstr(line, pattern);
	if (!p)
		return NULL;

	p = strchr(p + strlen(pattern), ':');
	if (!p)
		return NULL;

	return p + 1;
}

static int json_get_string(const char *line, const char *key,
			   char *out, size_t out_len)
{
	const char *p = json_find_value(line, key);
	size_t used = 0;

	if (!p || out_len == 0)
		return -1;

	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != '"')
		return -1;
	p++;

	while (*p && *p != '"' && used + 1 < out_len) {
		if (*p == '\\' && p[1])
			p++;
		out[used++] = *p++;
	}

	if (*p != '"')
		return -1;

	out[used] = '\0';
	return 0;
}

static int json_get_int(const char *line, const char *key, int *out)
{
	const char *p = json_find_value(line, key);
	int sign = 1;
	int value = 0;
	int seen = 0;

	if (!p)
		return -1;

	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '-') {
		sign = -1;
		p++;
	}

	while (*p >= '0' && *p <= '9') {
		seen = 1;
		value = value * 10 + (*p - '0');
		p++;
	}

	if (!seen)
		return -1;

	*out = value * sign;
	return 0;
}

static void result_init(struct command_result *result)
{
	memset(result, 0, sizeof(*result));
	result->action = CMD_ACTION_NONE;
	result->ack_ok = 0;
	snprintf(result->ack_cmd, sizeof(result->ack_cmd), "unknown");
	snprintf(result->ack_msg, sizeof(result->ack_msg), "bad command");
}

static void result_set(struct command_result *result, const char *cmd,
		       int ok, const char *msg)
{
	result->ack_ok = ok;
	snprintf(result->ack_cmd, sizeof(result->ack_cmd), "%s", cmd);
	snprintf(result->ack_msg, sizeof(result->ack_msg), "%s", msg);
}

int command_handle_line(const char *line, struct agent_config *cfg,
			struct command_result *result)
{
	char type[32];
	char cmd[64];
	char service[64];
	char action[64];
	char key[64];
	char config_value[256];
	char msg[128];
	int value;
	int seq;

	result_init(result);
	if (json_get_int(line, "seq", &seq) == 0 && seq > 0)
		result->seq = (unsigned int)seq;

	if (json_get_string(line, "type", type, sizeof(type)) ||
	    strcmp(type, "command") != 0)
		return -1;

	if (json_get_string(line, "cmd", cmd, sizeof(cmd))) {
		result_set(result, "unknown", 0, "missing cmd");
		return 0;
	}

	if (!strcmp(cmd, "get_status")) {
		result->action = CMD_ACTION_SEND_STATUS;
		result_set(result, cmd, 1, "status scheduled");
	} else if (!strcmp(cmd, "get_log")) {
		result->action = CMD_ACTION_SEND_LOG;
		result->lines = 20;
		if (json_get_int(line, "lines", &value) == 0)
			result->lines = value;
		if (result->lines < 1)
			result->lines = 1;
		if (result->lines > 100)
			result->lines = 100;
		result_set(result, cmd, 1, "log scheduled");
	} else if (!strcmp(cmd, "service_ctrl")) {
		if (json_get_string(line, "service", service, sizeof(service)) ||
		    json_get_string(line, "action", action, sizeof(action))) {
			result_set(result, cmd, 0, "missing service or action");
			return 0;
		}

		if (service_manager_handle(service, action, msg, sizeof(msg)) == 0)
			result_set(result, cmd, 1, msg);
		else
			result_set(result, cmd, 0, msg);
	} else if (!strcmp(cmd, "get_config")) {
		result->action = CMD_ACTION_SEND_CONFIG;
		result_set(result, cmd, 1, "config scheduled");
	} else if (!strcmp(cmd, "update_config")) {
		if (json_get_string(line, "key", key, sizeof(key)) ||
		    json_get_string(line, "value", config_value,
				    sizeof(config_value))) {
			result_set(result, cmd, 0, "missing key or value");
			return 0;
		}

		if (agent_config_update_value(cfg, key, config_value, msg,
					      sizeof(msg)) == 0)
			result_set(result, cmd, 1, msg);
		else
			result_set(result, cmd, 0, msg);
	} else if (!strcmp(cmd, "save_config")) {
		if (agent_config_save(cfg) == 0)
			result_set(result, cmd, 1, "config saved");
		else
			result_set(result, cmd, 0, "config save failed");
	} else if (!strcmp(cmd, "set_interval")) {
		int changed = 0;

		if (json_get_int(line, "heartbeat_interval", &value) == 0) {
			if (value < 1)
				value = 1;
			cfg->heartbeat_interval = value;
			changed = 1;
		}

		if (json_get_int(line, "status_interval", &value) == 0) {
			if (value < 1)
				value = 1;
			cfg->status_interval = value;
			changed = 1;
		}

		if (changed)
			result_set(result, cmd, 1, "interval updated");
		else
			result_set(result, cmd, 0, "missing interval");
	} else if (!strcmp(cmd, "shutdown")) {
		result->action = CMD_ACTION_SHUTDOWN;
		result_set(result, cmd, 1, "stopping");
	} else {
		result_set(result, cmd, 0, "unsupported command");
	}

	return 0;
}

int command_build_ack(const struct agent_config *cfg,
		      const struct command_result *result,
		      char *buf, size_t len)
{
	int written;

	written = snprintf(buf, len,
			   "{\"type\":\"ack\",\"device_id\":\"%s\","
			   "\"seq\":%u,\"timestamp\":%ld,"
			   "\"payload\":{\"cmd\":\"%s\","
			   "\"result\":\"%s\",\"msg\":\"%s\"}}\n",
			   cfg->device_id, result->seq, (long)time(NULL),
			   result->ack_cmd, result->ack_ok ? "ok" : "error",
			   result->ack_msg);

	if (written < 0 || (size_t)written >= len)
		return -1;

	return 0;
}
