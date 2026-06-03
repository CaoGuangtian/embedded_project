#include "agent_config.h"
#include "agent_log.h"
#include "command.h"
#include "net_client.h"
#include "protocol.h"
#include "status.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop;

static void signal_handler(int signo)
{
	(void)signo;
	g_stop = 1;
}

static long monotonic_seconds(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
		return time(NULL);

	return ts.tv_sec;
}

static int send_message_only(int fd, const char *tag, const char *line)
{
	agent_log_debug("send %s: %s", tag, line);
	if (net_client_send_all(fd, line, strlen(line))) {
		agent_log_error("send %s failed: %s", tag, strerror(errno));
		return -1;
	}

	return 0;
}

static int send_message_wait_ack(int fd, const char *tag, const char *line)
{
	char reply[P2_LINE_MAX];
	char ack_result[32];
	char ack_msg[128];

	if (send_message_only(fd, tag, line))
		return -1;

	memset(reply, 0, sizeof(reply));
	if (net_client_recv_line(fd, reply, sizeof(reply), 5) <= 0) {
		agent_log_error("wait %s ack failed or timed out", tag);
		return -1;
	}

	agent_log_debug("recv %s ack: %s", tag, reply);
	if (protocol_parse_ack(reply, ack_result, sizeof(ack_result),
			       ack_msg, sizeof(ack_msg))) {
		agent_log_error("server reply for %s is not an ack", tag);
		return -1;
	}

	agent_log_info("%s ack result=%s msg=%s", tag, ack_result, ack_msg);
	return strcmp(ack_result, "ok") == 0 ? 0 : -1;
}

static int send_status_report(int fd, const struct agent_config *cfg,
			      struct protocol_context *proto)
{
	struct agent_status st;
	char line[P2_LINE_MAX];

	status_collect(cfg, &st);
	if (protocol_build_status_report(proto, cfg, &st, line,
					 sizeof(line))) {
		agent_log_error("build status_report message failed");
		return -1;
	}

	return send_message_only(fd, "status_report", line);
}

struct log_send_context {
	int fd;
	const struct agent_config *cfg;
	struct protocol_context *proto;
	int failed;
};

static void send_log_line_cb(int index, const char *text, void *arg)
{
	struct log_send_context *ctx = arg;
	char line[P2_LINE_MAX];

	if (ctx->failed)
		return;

	if (protocol_build_log_line(ctx->proto, ctx->cfg, index, text, line,
				    sizeof(line))) {
		ctx->failed = 1;
		return;
	}

	if (net_client_send_all(ctx->fd, line, strlen(line)))
		ctx->failed = 1;
}

static int send_log_tail(int fd, const struct agent_config *cfg,
			 struct protocol_context *proto, int lines)
{
	struct log_send_context ctx;

	ctx.fd = fd;
	ctx.cfg = cfg;
	ctx.proto = proto;
	ctx.failed = 0;

	if (agent_log_tail(lines, send_log_line_cb, &ctx)) {
		agent_log_error("log tail failed");
		return -1;
	}

	if (ctx.failed) {
		agent_log_error("send log tail failed");
		return -1;
	}

	agent_log_info("sent log tail lines=%d", lines);
	return 0;
}

static int send_config_report(int fd, const struct agent_config *cfg,
			      struct protocol_context *proto)
{
	char line[P2_LINE_MAX];

	if (protocol_build_config_report(proto, cfg, line, sizeof(line))) {
		agent_log_error("build config_report message failed");
		return -1;
	}

	if (net_client_send_all(fd, line, strlen(line))) {
		agent_log_error("send config_report failed: %s", strerror(errno));
		return -1;
	}

	agent_log_info("sent config_report");
	return 0;
}

static int send_command_ack(int fd, const struct agent_config *cfg,
			    const struct command_result *result)
{
	char line[P2_LINE_MAX];

	if (command_build_ack(cfg, result, line, sizeof(line))) {
		agent_log_error("build command ack failed");
		return -1;
	}

	agent_log_debug("send command ack: %s", line);
	return net_client_send_all(fd, line, strlen(line));
}

static int handle_server_line(int fd, struct agent_config *cfg,
			      struct protocol_context *proto, const char *line)
{
	struct command_result result;
	char ack_result[32];
	char ack_msg[128];

	if (command_handle_line(line, cfg, &result)) {
		if (protocol_parse_ack(line, ack_result, sizeof(ack_result),
				       ack_msg, sizeof(ack_msg)) == 0) {
			agent_log_debug("recv async ack result=%s msg=%s",
					ack_result, ack_msg);
		} else {
			agent_log_debug("ignore server line: %s", line);
		}
		return 0;
	}

	agent_log_info("recv command: %s", line);

	if (result.action == CMD_ACTION_SEND_STATUS &&
	    send_status_report(fd, cfg, proto)) {
		result.ack_ok = 0;
		snprintf(result.ack_msg, sizeof(result.ack_msg),
			 "status failed");
	}

	if (result.action == CMD_ACTION_SEND_LOG &&
	    send_log_tail(fd, cfg, proto, result.lines)) {
		result.ack_ok = 0;
		snprintf(result.ack_msg, sizeof(result.ack_msg),
			 "log failed");
	}

	if (result.action == CMD_ACTION_SEND_CONFIG &&
	    send_config_report(fd, cfg, proto)) {
		result.ack_ok = 0;
		snprintf(result.ack_msg, sizeof(result.ack_msg),
			 "config failed");
	}

	if (send_command_ack(fd, cfg, &result))
		return -1;

	if (result.action == CMD_ACTION_SHUTDOWN)
		g_stop = 1;

	return 0;
}

static int poll_server_input(int fd, struct agent_config *cfg,
			     struct protocol_context *proto,
			     char *rx_line, size_t *rx_used)
{
	fd_set rfds;
	struct timeval tv;
	char buf[512];
	ssize_t n;
	ssize_t i;
	int ret;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	ret = select(fd + 1, &rfds, NULL, NULL, &tv);
	if (ret < 0) {
		if (errno == EINTR)
			return 0;
		return -1;
	}
	if (ret == 0)
		return 0;

	n = recv(fd, buf, sizeof(buf), 0);
	if (n <= 0)
		return -1;

	for (i = 0; i < n; i++) {
		if (buf[i] == '\n') {
			rx_line[*rx_used] = '\0';
			if (handle_server_line(fd, cfg, proto, rx_line))
				return -1;
			*rx_used = 0;
		} else if (*rx_used + 1 < P2_LINE_MAX) {
			rx_line[(*rx_used)++] = buf[i];
		} else {
			*rx_used = 0;
		}
	}

	return 0;
}

static int run_connected_session(struct agent_config *cfg,
				 struct protocol_context *proto, int fd)
{
	char line[P2_LINE_MAX];
	char rx_line[P2_LINE_MAX];
	size_t rx_used = 0;
	long connected_at = monotonic_seconds();
	long last_heartbeat = 0;
	long last_status = 0;

	if (protocol_build_register(proto, cfg, line, sizeof(line))) {
		agent_log_error("build register message failed");
		return -1;
	}

	if (send_message_wait_ack(fd, "register", line))
		return -1;

	while (!g_stop) {
		long now = monotonic_seconds();

		if (now - last_heartbeat >= cfg->heartbeat_interval) {
			long uptime_sec = now - connected_at;

			if (protocol_build_heartbeat(proto, cfg, uptime_sec,
						     line, sizeof(line))) {
				agent_log_error("build heartbeat message failed");
				return -1;
			}

			if (send_message_only(fd, "heartbeat", line))
				return -1;
			last_heartbeat = now;
		}

		if (now - last_status >= cfg->status_interval) {
			if (send_status_report(fd, cfg, proto))
				return -1;
			last_status = now;
		}

		if (poll_server_input(fd, cfg, proto, rx_line, &rx_used))
			return -1;

		sleep(1);
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct agent_config cfg;
	struct protocol_context proto;
	int fd;

	if (agent_config_parse_args(&cfg, argc, argv))
		return 1;

	protocol_init(&proto);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	if (agent_log_init(cfg.log_path, cfg.max_log_kb))
		fprintf(stderr, "log init failed: %s\n", cfg.log_path);

	agent_log_info("device_agent starting: id=%s server=%s:%d fw=%s "
		       "heartbeat=%ds status=%ds reconnect=%ds net=%s log=%s",
		       cfg.device_id, cfg.server_ip, cfg.server_port,
		       cfg.fw_version, cfg.heartbeat_interval,
		       cfg.status_interval, cfg.reconnect_interval,
		       cfg.net_ifname, cfg.log_path);

	while (!g_stop) {
		fd = net_client_connect(cfg.server_ip, cfg.server_port);
		if (fd < 0) {
			agent_log_error("connect %s:%d failed: %s",
					cfg.server_ip, cfg.server_port,
					strerror(errno));
			sleep((unsigned int)cfg.reconnect_interval);
			continue;
		}

		agent_log_info("connected to %s:%d", cfg.server_ip,
			       cfg.server_port);
		if (run_connected_session(&cfg, &proto, fd))
			agent_log_info("session ended, reconnect later");

		close(fd);
		if (!g_stop)
			sleep((unsigned int)cfg.reconnect_interval);
	}

	agent_log_info("device_agent stopping");
	agent_log_close();
	return 0;
}
