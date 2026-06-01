#ifndef STATUS_H
#define STATUS_H

#include "agent_config.h"

#define P2_NET_STATE_MAX 32
#define P2_MAC_ADDR_MAX 32

struct agent_status {
	long uptime_sec;
	long mem_total_kb;
	long mem_available_kb;
	int rootfs_usage_percent;
	char net_state[P2_NET_STATE_MAX];
	char mac_addr[P2_MAC_ADDR_MAX];
};

void status_collect(const struct agent_config *cfg, struct agent_status *st);

#endif

