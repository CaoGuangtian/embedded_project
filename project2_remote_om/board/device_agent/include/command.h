#ifndef COMMAND_H
#define COMMAND_H

#include <stddef.h>

enum command_action {
	CMD_ACTION_NONE = 0,
	CMD_ACTION_SEND_STATUS,
	CMD_ACTION_SEND_LOG,
	CMD_ACTION_SHUTDOWN,
};

struct command_result {
	enum command_action action;
	unsigned int seq;
	int lines;
	int ack_ok;
	char ack_cmd[64];
	char ack_msg[128];
};

struct agent_config;

int command_handle_line(const char *line, struct agent_config *cfg,
			struct command_result *result);
int command_build_ack(const struct agent_config *cfg,
		      const struct command_result *result,
		      char *buf, size_t len);

#endif
