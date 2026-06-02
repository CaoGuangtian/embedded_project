# Project 2 Protocol

Each TCP message is planned as one UTF-8 JSON object followed by `\n`.

Common fields:

```json
{
  "type": "heartbeat",
  "device_id": "imx6ull-001",
  "seq": 1,
  "timestamp": 1710000000,
  "payload": {}
}
```

Planned message types:

- `register`
- `heartbeat`
- `status_report`
- `command`
- `ack`
- `log_query`
- `log_result`
- `service_ctrl`
- `ota_check`
- `ota_result`
- `power_mode`

## Stage 1 Messages

### Register

Board to PC:

```json
{
  "type": "register",
  "device_id": "imx6ull-001",
  "seq": 1,
  "timestamp": 1710000000,
  "payload": {
    "model": "i.MX6ULL",
    "fw_version": "1.0.0"
  }
}
```

### ACK

PC to board:

```json
{
  "type": "ack",
  "device_id": "imx6ull-001",
  "seq": 1,
  "timestamp": 1710000001,
  "payload": {
    "result": "ok",
    "msg": "ok"
  }
}
```

### Heartbeat

Board to PC:

```json
{
  "type": "heartbeat",
  "device_id": "imx6ull-001",
  "seq": 2,
  "timestamp": 1710000005,
  "payload": {
    "uptime": 5,
    "net": "online"
  }
}
```

The PC replies with the same ACK format. If the ACK is missing or invalid,
the board closes the socket and enters reconnect flow.

### Status Report

Board to PC:

```json
{
  "type": "status_report",
  "device_id": "imx6ull-001",
  "seq": 3,
  "timestamp": 1710000010,
  "payload": {
    "uptime": 3600,
    "mem_total_kb": 256000,
    "mem_available_kb": 120000,
    "rootfs_usage": 45,
    "net_ifname": "eth0",
    "net_state": "up",
    "mac": "00:11:22:33:44:55",
    "fw_version": "1.0.0"
  }
}
```

The current stage requires `register`, `heartbeat`, `status_report`, and
`ack`.

## Remote Commands

PC to board:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1001,
  "timestamp": 1710000020,
  "payload": {
    "cmd": "get_status",
    "args": {}
  }
}
```

Board command ACK:

```json
{
  "type": "ack",
  "device_id": "imx6ull-001",
  "seq": 1001,
  "timestamp": 1710000021,
  "payload": {
    "cmd": "get_status",
    "result": "ok",
    "msg": "status scheduled"
  }
}
```

Supported first-stage commands:

- `get_status`: immediately send one `status_report`
- `set_interval`: update runtime intervals
- `get_log`: send recent device-agent log lines
- `service_ctrl`: manage whitelisted board-side services
- `get_config`: send current runtime configuration
- `update_config`: update whitelisted runtime configuration
- `save_config`: persist current configuration to file
- `shutdown`: stop `device_agent`

Example `set_interval`:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1002,
  "timestamp": 1710000025,
  "payload": {
    "cmd": "set_interval",
    "args": {
      "heartbeat_interval": 2,
      "status_interval": 10
    }
  }
}
```

Example `get_log`:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1003,
  "timestamp": 1710000030,
  "payload": {
    "cmd": "get_log",
    "args": {
      "lines": 20
    }
  }
}
```

Board replies with zero or more log lines, followed by one command ACK:

```json
{
  "type": "log_line",
  "device_id": "imx6ull-001",
  "seq": 21,
  "timestamp": 1710000031,
  "payload": {
    "index": 0,
    "text": "[2026-06-02 10:30:21] [INFO] connected"
  }
}
```

Example `service_ctrl`:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1004,
  "timestamp": 1710000040,
  "payload": {
    "cmd": "service_ctrl",
    "args": {
      "service": "collector_demo",
      "action": "restart"
    }
  }
}
```

Supported actions:

- `status`
- `start`
- `stop`
- `restart`

Whitelisted services:

- `collector_demo`
- `power_manager`
- `network_monitor`
- `app_service`

The board rejects services and actions outside these lists.

Example `get_config`:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1005,
  "timestamp": 1710000050,
  "payload": {
    "cmd": "get_config",
    "args": {}
  }
}
```

Board replies with `config_report`, followed by one command ACK:

```json
{
  "type": "config_report",
  "device_id": "imx6ull-001",
  "seq": 22,
  "timestamp": 1710000051,
  "payload": {
    "heartbeat_interval": 5,
    "status_interval": 10,
    "reconnect_interval": 3,
    "net_ifname": "eth0",
    "log_path": "/var/log/device_agent/device_agent.log",
    "max_log_kb": 1024,
    "config_path": "/etc/device_agent/device_agent.conf"
  }
}
```

Example `update_config`:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1006,
  "timestamp": 1710000060,
  "payload": {
    "cmd": "update_config",
    "args": {
      "key": "heartbeat_interval",
      "value": "3"
    }
  }
}
```

Allowed remote config keys:

- `heartbeat_interval`
- `status_interval`
- `reconnect_interval`
- `net_ifname`
- `log_path`
- `max_log_kb`

The board rejects sensitive keys such as `server_ip`, `server_port`,
`device_id`, `fw_version`, and `config_path`.

Later stages will add OTA and power mode commands.
