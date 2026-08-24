#ifndef AGENT_CONFIG_H
#define AGENT_CONFIG_H

#include <stddef.h>

#define OM_DEVICE_ID_MAX 64
#define OM_SERVER_IP_MAX 64
#define OM_FW_VERSION_MAX 32
#define OM_CONFIG_PATH_MAX 256
#define OM_NET_IFNAME_MAX 32
#define OM_LOG_PATH_MAX 256

#define OM_DEFAULT_DEVICE_ID "imx6ull-001"
#define OM_DEFAULT_SERVER_IP "192.168.10.100"
#define OM_DEFAULT_SERVER_PORT 9000
#define OM_DEFAULT_FW_VERSION "1.0.0"
#define OM_DEFAULT_CONFIG_PATH "/etc/device_agent/device_agent.conf"
#define OM_DEFAULT_HEARTBEAT_INTERVAL 5
#define OM_DEFAULT_STATUS_INTERVAL 10
#define OM_DEFAULT_RECONNECT_INTERVAL 3
#define OM_DEFAULT_NET_IFNAME "eth0"
#define OM_DEFAULT_LOG_PATH "/var/log/device_agent/device_agent.log"
#define OM_DEFAULT_MAX_LOG_KB 1024

struct agent_config {
	char device_id[OM_DEVICE_ID_MAX];
	char server_ip[OM_SERVER_IP_MAX];
	int server_port;
	int heartbeat_interval;
	int status_interval;
	int reconnect_interval;
	int max_log_kb;
	char net_ifname[OM_NET_IFNAME_MAX];
	char log_path[OM_LOG_PATH_MAX];
	char fw_version[OM_FW_VERSION_MAX];
	char config_path[OM_CONFIG_PATH_MAX];
};

void agent_config_defaults(struct agent_config *cfg);
int agent_config_load(struct agent_config *cfg, const char *path);
int agent_config_parse_args(struct agent_config *cfg, int argc, char **argv);
int agent_config_save(const struct agent_config *cfg);
int agent_config_update_value(struct agent_config *cfg, const char *key,
			      const char *value, char *msg, size_t msg_len);

#endif
