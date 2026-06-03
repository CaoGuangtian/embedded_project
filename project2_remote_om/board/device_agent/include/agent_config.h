#ifndef AGENT_CONFIG_H
#define AGENT_CONFIG_H

#include <stddef.h>

#define P2_DEVICE_ID_MAX 64
#define P2_SERVER_IP_MAX 64
#define P2_FW_VERSION_MAX 32
#define P2_CONFIG_PATH_MAX 256
#define P2_NET_IFNAME_MAX 32
#define P2_LOG_PATH_MAX 256

#define P2_DEFAULT_DEVICE_ID "imx6ull-001"
#define P2_DEFAULT_SERVER_IP "192.168.10.100"
#define P2_DEFAULT_SERVER_PORT 9000
#define P2_DEFAULT_FW_VERSION "1.0.0"
#define P2_DEFAULT_CONFIG_PATH "/etc/device_agent/device_agent.conf"
#define P2_DEFAULT_HEARTBEAT_INTERVAL 5
#define P2_DEFAULT_STATUS_INTERVAL 10
#define P2_DEFAULT_RECONNECT_INTERVAL 3
#define P2_DEFAULT_NET_IFNAME "eth0"
#define P2_DEFAULT_LOG_PATH "/var/log/device_agent/device_agent.log"
#define P2_DEFAULT_MAX_LOG_KB 1024

struct agent_config {
	char device_id[P2_DEVICE_ID_MAX];
	char server_ip[P2_SERVER_IP_MAX];
	int server_port;
	int heartbeat_interval;
	int status_interval;
	int reconnect_interval;
	int max_log_kb;
	char net_ifname[P2_NET_IFNAME_MAX];
	char log_path[P2_LOG_PATH_MAX];
	char fw_version[P2_FW_VERSION_MAX];
	char config_path[P2_CONFIG_PATH_MAX];
};

void agent_config_defaults(struct agent_config *cfg);
int agent_config_load(struct agent_config *cfg, const char *path);
int agent_config_parse_args(struct agent_config *cfg, int argc, char **argv);
int agent_config_save(const struct agent_config *cfg);
int agent_config_update_value(struct agent_config *cfg, const char *key,
			      const char *value, char *msg, size_t msg_len);

#endif
