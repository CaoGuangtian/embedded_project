#!/bin/sh

set -eu

SRC_DIR="${1:-.}"
APP_DIR="${APP_DIR:-/opt/project1}"
CONFIG_DIR="${CONFIG_DIR:-/etc/project1}"

if [ ! -d "$SRC_DIR/bin" ] || [ ! -d "$SRC_DIR/modules" ]; then
	echo "usage: $0 <staged-release-dir>" >&2
	echo "example: $0 /path/to/release/project1" >&2
	exit 1
fi

mkdir -p "$APP_DIR/bin" "$APP_DIR/modules" "$CONFIG_DIR"

cp "$SRC_DIR/bin/"* "$APP_DIR/bin/"
cp "$SRC_DIR/modules/"* "$APP_DIR/modules/"
cp "$SRC_DIR/load_modules.sh" "$APP_DIR/"
cp "$SRC_DIR/unload_modules.sh" "$APP_DIR/"
cp "$SRC_DIR/config/project1.env" "$CONFIG_DIR/project1.env"

chmod +x "$APP_DIR/bin/"* "$APP_DIR/load_modules.sh" "$APP_DIR/unload_modules.sh"

if [ -d /etc/init.d ] && [ -f "$SRC_DIR/init.d/S99project1" ]; then
	cp "$SRC_DIR/init.d/S99project1" /etc/init.d/S99project1
	chmod +x /etc/init.d/S99project1
	echo "installed BusyBox init script: /etc/init.d/S99project1"
fi

if command -v systemctl >/dev/null 2>&1 && [ -f "$SRC_DIR/systemd/project1.service" ]; then
	cp "$SRC_DIR/systemd/project1.service" /etc/systemd/system/project1.service
	systemctl daemon-reload
	echo "installed systemd service: project1.service"
fi

echo "installed Project1 to $APP_DIR"
