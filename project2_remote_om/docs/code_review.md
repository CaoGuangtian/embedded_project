# Code Review Checklist

This checklist records items that should be verified in a Linux build
environment or on the i.MX6ULL board.

## Build Verification

- Build `board/device_agent` with `arm-linux-gnueabihf-gcc`.
- Check for missing POSIX feature macros or library dependencies.
- Verify `Makefile` source list stays in sync with added modules.
- Confirm Python server runs with the target PC Python version.

## Runtime Verification

- Confirm `device_agent` starts with `/etc/device_agent/device_agent.conf`.
- Confirm TCP register, heartbeat, status report, command ACK, and reconnect.
- Confirm log file creation under `/var/log/device_agent/`.
- Confirm log rotation behavior at `max_log_kb`.
- Confirm `get_log` does not interfere with heartbeat/status ACK flow.

## Board Tool Dependencies

- `pidof` for service status.
- `wget` for OTA download.
- `sha256sum` for OTA hash verification.
- `tar` for OTA extraction.
- `cp` and `chmod` for OTA install.
- BusyBox applet paths may differ from `/bin` and `/usr/bin`.

## Service Management

- Verify init scripts exist for whitelisted services.
- Confirm script names match:
  - `/etc/init.d/S99collector_demo`
  - `/etc/init.d/S98power_manager`
  - `/etc/init.d/S97network_monitor`
  - `/etc/init.d/S96app_service`
- Confirm service `status` result matches the process name used by `pidof`.

## OTA

- Verify package layout:

```text
bin/
└── <target>

version
```

- Confirm OTA prepare leaves files only under `/tmp/project2_ota_<target>/`.
- Confirm `ota install device_agent` is rejected.
- Confirm rollback restores `<target>.bak` after failed health check.
- Confirm target install paths exist under `/opt/project2/bin/`.

## Security Assumptions

- Remote config updates only allow whitelisted keys.
- Service commands only allow whitelisted services and actions.
- OTA targets are whitelisted.
- OTA local paths are generated on the board and not supplied by PC.
- `device_agent` self-upgrade requires a future external helper.

## Review Fixes Already Applied

- Command ACK strings are JSON-escaped before being sent.
- OTA SHA256 verification avoids shell command construction and uses
  `fork`, `pipe`, and `execl`.
- systemd service avoids `StandardOutput=append:...` for broader target
  compatibility.
- Config update API uses `size_t` for message buffer length.
- Log timestamp generation avoids `localtime_r` portability assumptions.

## Remaining First-Build Risks

- Confirm all POSIX calls are available in the target root filesystem.
- Confirm BusyBox provides `wget`, `sha256sum`, `tar`, `cp`, `chmod`, and
  `pidof` at expected paths or through compatible symlinks.
- Confirm service scripts exist and are executable.
- Confirm `device_agent` can write `/etc/device_agent/device_agent.conf`
  when `config save` is used.
- Confirm `device_agent` can write `/var/log/device_agent/device_agent.log`.
- Confirm OTA install paths exist under `/opt/project2/bin`.
