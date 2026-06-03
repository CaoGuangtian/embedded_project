#!/bin/sh

set -eu

SRC_DIR="${1:-.}"
APP_DIR="${APP_DIR:-/opt/project2}"
CONFIG_DIR="${CONFIG_DIR:-/etc/device_agent}"
POWER_CONFIG_DIR="${POWER_CONFIG_DIR:-/etc/power_manager}"
LOG_DIR="${LOG_DIR:-/var/log/device_agent}"
POWER_LOG_DIR="${POWER_LOG_DIR:-/var/log/power_manager}"

if [ ! -d "$SRC_DIR/bin" ] || [ ! -d "$SRC_DIR/config" ]; then
	echo "usage: $0 <release-dir>" >&2
	echo "example: $0 /tmp/project2" >&2
	exit 1
fi

mkdir -p "$APP_DIR/bin" "$APP_DIR/scripts" "$CONFIG_DIR" "$POWER_CONFIG_DIR" "$LOG_DIR" "$POWER_LOG_DIR"

if [ -f "$SRC_DIR/bin/device_agent" ]; then
	cp "$SRC_DIR/bin/device_agent" "$APP_DIR/bin/"
	chmod +x "$APP_DIR/bin/device_agent"
else
	echo "warning: missing binary: $SRC_DIR/bin/device_agent" >&2
fi

if [ -f "$SRC_DIR/bin/power_manager" ]; then
	cp "$SRC_DIR/bin/power_manager" "$APP_DIR/bin/"
	chmod +x "$APP_DIR/bin/power_manager"
else
	echo "warning: missing binary: $SRC_DIR/bin/power_manager" >&2
fi

cp "$SRC_DIR/config/device_agent.conf" "$CONFIG_DIR/device_agent.conf"
if [ -f "$SRC_DIR/config/power_manager.conf" ]; then
	cp "$SRC_DIR/config/power_manager.conf" "$POWER_CONFIG_DIR/power_manager.conf"
fi

if [ -d /etc/init.d ]; then
	if [ -f "$SRC_DIR/init.d/S98power_manager" ]; then
		cp "$SRC_DIR/init.d/S98power_manager" /etc/init.d/S98power_manager
		chmod +x /etc/init.d/S98power_manager
		echo "installed init.d script: /etc/init.d/S98power_manager"
	fi
	if [ -f "$SRC_DIR/init.d/S99device_agent" ]; then
		cp "$SRC_DIR/init.d/S99device_agent" /etc/init.d/S99device_agent
		chmod +x /etc/init.d/S99device_agent
		echo "installed init.d script: /etc/init.d/S99device_agent"
	fi
fi

if command -v systemctl >/dev/null 2>&1; then
	if [ -f "$SRC_DIR/systemd/power_manager.service" ]; then
		cp "$SRC_DIR/systemd/power_manager.service" /etc/systemd/system/power_manager.service
		echo "installed systemd service: power_manager.service"
	fi
	if [ -f "$SRC_DIR/systemd/device_agent.service" ]; then
		cp "$SRC_DIR/systemd/device_agent.service" /etc/systemd/system/device_agent.service
		echo "installed systemd service: device_agent.service"
	fi
	systemctl daemon-reload
fi

echo "installed Project2 device_agent"
echo "app:    $APP_DIR"
echo "config: $CONFIG_DIR/device_agent.conf"
echo "power:  $POWER_CONFIG_DIR/power_manager.conf"
echo "log:    $LOG_DIR/device_agent.log"
