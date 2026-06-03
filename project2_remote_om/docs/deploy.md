# Deployment Notes

Planned board layout:

```text
/opt/project2/
├── bin/
│   ├── device_agent
│   └── power_manager
├── config/
│   ├── device_agent.conf
│   └── power_manager.conf
└── scripts/
```

Planned config paths:

```text
/etc/device_agent/device_agent.conf
/etc/power_manager/power_manager.conf
```

Planned log paths:

```text
/var/log/device_agent/device_agent.log
/var/log/power_manager/power_manager.log
```

Current `device_agent` config options:

```text
log_path=/var/log/device_agent/device_agent.log
max_log_kb=1024
```

Remote config keys allowed in the current stage:

```text
heartbeat_interval
status_interval
reconnect_interval
net_ifname
log_path
max_log_kb
```

Current OTA framework:

```text
download path: /tmp/project2_ota_<target>.tar.gz
extract path: /tmp/project2_ota_<target>/
allowed targets: device_agent, power_manager, collector_demo, app_service
prepare action: download, SHA256 check, extract, package content check
install action: backup, replace, chmod, restart, health check, rollback
install paths:
  power_manager  -> /opt/project2/bin/power_manager
  collector_demo -> /opt/project2/bin/collector_demo
  app_service    -> /opt/project2/bin/app_service
```

Current service management whitelist:

```text
collector_demo  -> /etc/init.d/S99collector_demo
power_manager   -> /etc/init.d/S98power_manager
network_monitor -> /etc/init.d/S97network_monitor
app_service     -> /etc/init.d/S96app_service
```
