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
12. Enter `service status collector_demo` and confirm a `running` or `stopped` ACK is returned.
13. Enter `service status power_manager` and confirm `power_manager mode=<mode>` is returned.
14. Enter `service stop power_manager` and confirm `power_manager stop unsupported`.
15. Enter `service restart collector_demo` and confirm the script result is returned.
16. Enter `service stop unknown_service` and confirm `service not allowed`.
17. Enter `service bad_action collector_demo` and confirm `action not allowed`.
18. Enter `config get` and confirm a config summary is printed.
19. Enter `config set heartbeat_interval 2` and confirm heartbeat interval changes.
20. Enter `config set status_interval 5` and confirm status interval changes.
21. Enter `config save` and confirm config is persisted.
22. Enter `config set server_ip 1.2.3.4` and confirm `config key not allowed`.
23. Enter `ota upgrade unknown 1.1.0 http://server/pkg.tar.gz <valid_sha>` and confirm `target not allowed`.
24. Enter `ota upgrade device_agent 1.1.0 ftp://server/pkg.tar.gz <valid_sha>` and confirm `bad url`.
25. Enter `ota upgrade device_agent 1.1.0 http://server/pkg.tar.gz badsha` and confirm `bad sha256`.
26. Enter `ota upgrade device_agent 1.1.0 http://server/pkg.tar.gz <wrong_sha>` and confirm `sha256 mismatch` or download failure.
27. Use a tarball without `bin/device_agent` and confirm `missing target binary`.
28. Use a tarball without `version` and confirm `missing version`.
29. Use a tarball whose `version` does not match and confirm `version mismatch`.
30. Use a valid tarball and confirm `ota package prepared`.
31. Enter `ota install device_agent` and confirm `self upgrade not supported yet`.
32. Enter `ota install unknown` and confirm `target not allowed`.
33. Enter `ota install collector_demo` without a prepared package and confirm `missing target binary`.
34. Prepare a valid `collector_demo` package and confirm `install ok` or a rollback message if the service health check fails.
35. Enter `power get` and confirm current mode is returned.
36. Enter `power set low_power` and confirm mode changes.
37. Enter `power set sleep` with `allow_suspend=false` and confirm it logs but does not suspend.
38. Enter `power set bad_mode` and confirm `bad mode`.
39. Enter `shutdown` and confirm board-side `device_agent` exits cleanly.

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
config get
config set <key> <value>
config save
ota upgrade <target> <version> <url> <sha256>
ota install <target>
power get
power set <normal|idle|low_power|sleep|maintenance>
shutdown
quit
```

OTA package layout for tests:

```text
bin/
└── device_agent

version
```
