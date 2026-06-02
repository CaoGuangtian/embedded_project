# Build Notes

The first implementation can be developed on Windows as source code only.
Cross-compilation and board-side verification can be done later in a Linux
environment.

Planned board compiler:

```bash
arm-linux-gnueabihf-gcc
```

Build board-side `device_agent`:

```bash
cd board/device_agent
make
```

For local syntax experiments on a Linux PC, override `CC`:

```bash
make CC=gcc
```

Planned PC runtime:

```bash
python3 server/manage_server.py
```

Run flow:

```bash
# PC side
python3 server/manage_server.py --host 0.0.0.0 --port 9000

# board side
./device_agent -c ../scripts/device_agent.conf
```

The board-side agent keeps running after registration. It sends heartbeat
messages according to `heartbeat_interval` and reconnects after
`reconnect_interval` seconds when the socket is broken. It also sends
device status according to `status_interval`.

The PC server supports these first-stage interactive commands:

```text
status
get_status
set_heartbeat <seconds>
set_status <seconds>
get_log [lines]
service <status|start|stop|restart> <name>
config get
config set <key> <value>
config save
shutdown
quit
```
