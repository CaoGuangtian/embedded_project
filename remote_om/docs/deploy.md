# Deployment Notes

Planned board layout:

```text
/opt/remote_om/
├── bin/
│   ├── device_agent
│   ├── power_manager
│   └── ...
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

## Release Install

Build and package on the host:

```bash
cd remote_om/board/device_agent
make

cd ../power_manager
make

cd ../..
./board/scripts/package_release.sh
```

Copy `release/remote_om` to the board, then install:

```bash
cd /path/to/release/remote_om
./install.sh .
```

BusyBox init.d:

```bash
/etc/init.d/S99device_agent start
/etc/init.d/S98power_manager status
/etc/init.d/S99device_agent status
/etc/init.d/S99device_agent stop
```

systemd:

```bash
systemctl start device_agent
systemctl start power_manager
systemctl status device_agent
systemctl enable device_agent
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
download path: /tmp/om_ota_<target>.tar.gz
extract path: /tmp/om_ota_<target>/
allowed targets: device_agent, power_manager, collector_demo, app_service
prepare action: download, SHA256 check, extract, package content check
install action: backup, replace, chmod, restart, health check, rollback
install paths:
  power_manager  -> /opt/remote_om/bin/power_manager
  collector_demo -> /opt/remote_om/bin/collector_demo
  app_service    -> /opt/remote_om/bin/app_service
```

Current service management whitelist:

```text
collector_demo  -> /etc/init.d/S99collector_demo
power_manager   -> /etc/init.d/S98power_manager
network_monitor -> /etc/init.d/S97network_monitor
app_service     -> /etc/init.d/S96app_service
```

Power manager:

```text
config: /etc/power_manager/power_manager.conf
state:  /var/run/power_manager.mode
log:    /var/log/power_manager/power_manager.log
```
