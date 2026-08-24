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
the board records the asynchronous ACK when it is received. The board does
not block waiting for heartbeat ACKs, so PC commands can be received at any
time.

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
`ack`. `register` uses a synchronous ACK during connection setup.
`heartbeat` and `status_report` use asynchronous ACK handling in the main
receive loop.

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
- `ota_upgrade`: download an OTA package and verify SHA256
- `ota_install`: install a prepared OTA package
- `power_mode`: get or set board power mode
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

The board rejects services and actions outside these lists. Daemon services
return `running` or `stopped` for `status`. `power_manager` is a
command-style helper in this stage, so `status` returns its recorded mode,
for example `power_manager mode=low_power`; `stop` is unsupported.

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

Example `ota_upgrade`:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1100,
  "timestamp": 1710000100,
  "payload": {
    "cmd": "ota_upgrade",
    "args": {
      "target": "device_agent",
      "version": "1.1.0",
      "url": "http://192.168.10.100:8000/ota/device_agent_v1.1.0.tar.gz",
      "sha256": "abcdef123456abcdef123456abcdef123456abcdef123456abcdef123456abcd"
    }
  }
}
```

Current OTA stage downloads to a fixed `/tmp/om_ota_<target>.tar.gz`
path, checks SHA256, rejects unsafe archive paths, extracts the package to
`/tmp/om_ota_<target>/`, and validates package contents. It does not
replace binaries, restart services, run health checks, or roll back.

Expected package layout after extraction:

```text
bin/
└── <target>

version
```

The `version` file must match the command `version` argument.

Allowed OTA targets:

- `device_agent`
- `power_manager`
- `collector_demo`
- `app_service`

Security limits:

- `target` must be whitelisted
- `version` and `target` must be simple tokens
- `url` must start with `http://` or `https://`
- `sha256` must be 64 hexadecimal characters
- PC cannot choose the local download path
- archive entries must be relative paths and must not contain `..` path
  segments

The next command installs a package that has already been prepared by
`ota_upgrade`.

Example `ota_install`:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1101,
  "timestamp": 1710000120,
  "payload": {
    "cmd": "ota_install",
    "args": {
      "target": "collector_demo"
    }
  }
}
```

Current install paths:

```text
power_manager  -> /opt/remote_om/bin/power_manager
collector_demo -> /opt/remote_om/bin/collector_demo
app_service    -> /opt/remote_om/bin/app_service
```

`device_agent` self-upgrade is not supported in this stage. Install flow:

```text
check prepared package
backup old binary to <target>.bak
copy prepared binary into /opt/remote_om/bin/
chmod +x
restart service
check service status
rollback from .bak on failure
```

Example `power_mode` get:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1200,
  "timestamp": 1710000200,
  "payload": {
    "cmd": "power_mode",
    "args": {
      "action": "get"
    }
  }
}
```

Example `power_mode` set:

```json
{
  "type": "command",
  "device_id": "imx6ull-001",
  "seq": 1201,
  "timestamp": 1710000205,
  "payload": {
    "cmd": "power_mode",
    "args": {
      "action": "set",
      "mode": "low_power"
    }
  }
}
```

Allowed power modes:

- `normal`
- `idle`
- `low_power`
- `sleep`
- `maintenance`

Current `power_manager` stage records and logs mode changes. With
`allow_suspend=false`, `sleep` does not actually suspend the board.

Later stages will add device-agent self-upgrade and real suspend/resume hooks.
