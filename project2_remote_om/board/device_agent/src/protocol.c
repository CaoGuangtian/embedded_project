#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void json_escape(const char *src, char *dst, size_t dst_len)
{
	size_t used = 0;

	if (dst_len == 0)
		return;

	while (*src && used + 1 < dst_len) {
		char ch = *src++;

		if ((ch == '"' || ch == '\\') && used + 2 < dst_len) {
			dst[used++] = '\\';
			dst[used++] = ch;
		} else if (ch >= 0 && ch < 0x20) {
			dst[used++] = ' ';
		} else {
			dst[used++] = ch;
		}
	}

	dst[used] = '\0';
}

void protocol_init(struct protocol_context *ctx)
{
	ctx->seq = 0;
}

int protocol_build_register(struct protocol_context *ctx,
			    const struct agent_config *cfg,
			    char *buf, size_t len)
{
	char device_id[128];
	char fw_version[64];
	int written;

	json_escape(cfg->device_id, device_id, sizeof(device_id));
	json_escape(cfg->fw_version, fw_version, sizeof(fw_version));

	ctx->seq++;
	written = snprintf(buf, len,
			   "{\"type\":\"register\",\"device_id\":\"%s\","
			   "\"seq\":%u,\"timestamp\":%ld,"
			   "\"payload\":{\"model\":\"i.MX6ULL\","
			   "\"fw_version\":\"%s\"}}\n",
			   device_id, ctx->seq, (long)time(NULL), fw_version);

	if (written < 0 || (size_t)written >= len)
		return -1;

	return 0;
}

int protocol_build_heartbeat(struct protocol_context *ctx,
			     const struct agent_config *cfg,
			     long uptime_sec, char *buf, size_t len)
{
	char device_id[128];
	int written;

	json_escape(cfg->device_id, device_id, sizeof(device_id));

	ctx->seq++;
	written = snprintf(buf, len,
			   "{\"type\":\"heartbeat\",\"device_id\":\"%s\","
			   "\"seq\":%u,\"timestamp\":%ld,"
			   "\"payload\":{\"uptime\":%ld,"
			   "\"net\":\"online\"}}\n",
			   device_id, ctx->seq, (long)time(NULL), uptime_sec);

	if (written < 0 || (size_t)written >= len)
		return -1;

	return 0;
}

int protocol_build_status_report(struct protocol_context *ctx,
				 const struct agent_config *cfg,
				 const struct agent_status *st,
				 char *buf, size_t len)
{
	char device_id[128];
	char fw_version[64];
	char net_ifname[64];
	char net_state[64];
	char mac_addr[64];
	int written;

	json_escape(cfg->device_id, device_id, sizeof(device_id));
	json_escape(cfg->fw_version, fw_version, sizeof(fw_version));
	json_escape(cfg->net_ifname, net_ifname, sizeof(net_ifname));
	json_escape(st->net_state, net_state, sizeof(net_state));
	json_escape(st->mac_addr, mac_addr, sizeof(mac_addr));

	ctx->seq++;
	written = snprintf(buf, len,
			   "{\"type\":\"status_report\",\"device_id\":\"%s\","
			   "\"seq\":%u,\"timestamp\":%ld,"
			   "\"payload\":{\"uptime\":%ld,"
			   "\"mem_total_kb\":%ld,"
			   "\"mem_available_kb\":%ld,"
			   "\"rootfs_usage\":%d,"
			   "\"net_ifname\":\"%s\","
			   "\"net_state\":\"%s\","
			   "\"mac\":\"%s\","
			   "\"fw_version\":\"%s\"}}\n",
			   device_id, ctx->seq, (long)time(NULL),
			   st->uptime_sec, st->mem_total_kb,
			   st->mem_available_kb, st->rootfs_usage_percent,
			   net_ifname, net_state, mac_addr, fw_version);

	if (written < 0 || (size_t)written >= len)
		return -1;

	return 0;
}

int protocol_build_log_line(struct protocol_context *ctx,
			    const struct agent_config *cfg,
			    int index, const char *text, char *buf, size_t len)
{
	char device_id[128];
	char escaped[512];
	int written;

	json_escape(cfg->device_id, device_id, sizeof(device_id));
	json_escape(text, escaped, sizeof(escaped));

	ctx->seq++;
	written = snprintf(buf, len,
			   "{\"type\":\"log_line\",\"device_id\":\"%s\","
			   "\"seq\":%u,\"timestamp\":%ld,"
			   "\"payload\":{\"index\":%d,"
			   "\"text\":\"%s\"}}\n",
			   device_id, ctx->seq, (long)time(NULL), index,
			   escaped);

	if (written < 0 || (size_t)written >= len)
		return -1;

	return 0;
}

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

int protocol_parse_ack(const char *line, char *result, size_t result_len,
		       char *msg, size_t msg_len)
{
	char type[32];

	if (json_get_string(line, "type", type, sizeof(type)))
		return -1;
	if (strcmp(type, "ack"))
		return -1;

	if (json_get_string(line, "result", result, result_len))
		snprintf(result, result_len, "unknown");
	if (json_get_string(line, "msg", msg, msg_len))
		snprintf(msg, msg_len, "");

	return 0;
}
