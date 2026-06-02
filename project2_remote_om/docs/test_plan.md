# Test Plan

Planned test groups:

- TCP connection and registration
- Heartbeat and reconnect
- Status reporting
- Remote command ACK
- Log query
- Service management
- OTA success and rollback
- Low-power mode switching
- Suspend/resume and wakeup recovery

## Stage 1 Test

1. Start `server/manage_server.py` on the PC.
2. Start `board/device_agent/device_agent` on the board.
3. Confirm the PC prints a `register` message.
4. Confirm the board prints an ACK reply.
5. Confirm the PC keeps receiving `heartbeat` messages.
6. Confirm the PC keeps receiving `status_report` messages.
7. Stop the PC server, then start it again and confirm reconnect.
8. Enter `get_status` in the PC server and confirm an immediate status report.
9. Enter `set_heartbeat 2` and confirm heartbeat interval changes.
10. Enter `set_status 5` and confirm status interval changes.
11. Enter `get_log 20` and confirm recent board-side logs are printed.
12. Enter `service status collector_demo` and confirm an ACK is returned.
13. Enter `service restart collector_demo` and confirm the script result is returned.
14. Enter `service stop unknown_service` and confirm `service not allowed`.
15. Enter `service bad_action collector_demo` and confirm `action not allowed`.
16. Enter `shutdown` and confirm board-side `device_agent` exits cleanly.

Expected board output:

```text
device_agent starting: id=imx6ull-001 server=192.168.10.100:9000 fw=1.0.0
connected to 192.168.10.100:9000
send register: {"type":"register",...}
register ack result=ok msg=ok
send heartbeat: {"type":"heartbeat",...}
heartbeat ack result=ok msg=ok
send status_report: {"type":"status_report",...}
status_report ack result=ok msg=ok
```

Expected PC status summary:

```text
status device=imx6ull-001 uptime=3600s mem=120000/256000KB rootfs=45% eth0=up fw=1.0.0 seq=3
```

PC-side interactive commands:

```text
status
get_status
set_heartbeat <seconds>
set_status <seconds>
get_log [lines]
service <status|start|stop|restart> <name>
shutdown
quit
```
