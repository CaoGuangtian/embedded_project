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
`ack`. Later stages will add `command` and remote operation commands.
