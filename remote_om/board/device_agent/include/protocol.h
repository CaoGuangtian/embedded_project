#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <time.h>

#include "agent_config.h"
#include "status.h"

#define OM_LINE_MAX 1024

struct protocol_context {
	unsigned int seq;
};

void protocol_init(struct protocol_context *ctx);
int protocol_build_register(struct protocol_context *ctx,
			    const struct agent_config *cfg,
			    char *buf, size_t len);
int protocol_build_heartbeat(struct protocol_context *ctx,
			     const struct agent_config *cfg,
			     long uptime_sec, char *buf, size_t len);
int protocol_build_status_report(struct protocol_context *ctx,
				 const struct agent_config *cfg,
				 const struct agent_status *st,
				 char *buf, size_t len);
int protocol_build_log_line(struct protocol_context *ctx,
			    const struct agent_config *cfg,
			    int index, const char *text, char *buf, size_t len);
int protocol_build_config_report(struct protocol_context *ctx,
				 const struct agent_config *cfg,
				 char *buf, size_t len);
int protocol_parse_ack(const char *line, char *result, size_t result_len,
		       char *msg, size_t msg_len);

#endif
