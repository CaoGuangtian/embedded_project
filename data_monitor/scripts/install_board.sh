#!/bin/sh

set -eu

SRC_DIR="${1:-.}"
APP_DIR="${APP_DIR:-/opt/datamon}"
CONFIG_DIR="${CONFIG_DIR:-/etc/datamon}"

if [ ! -d "$SRC_DIR/bin" ] || [ ! -d "$SRC_DIR/modules" ]; then
	echo "usage: $0 <staged-release-dir>" >&2
	echo "example: $0 /path/to/release/datamon" >&2
	exit 1
fi

mkdir -p "$APP_DIR/bin" "$APP_DIR/modules" "$CONFIG_DIR"

cp "$SRC_DIR/bin/"* "$APP_DIR/bin/"
cp "$SRC_DIR/modules/"* "$APP_DIR/modules/"
cp "$SRC_DIR/load_modules.sh" "$APP_DIR/"
cp "$SRC_DIR/unload_modules.sh" "$APP_DIR/"
cp "$SRC_DIR/config/datamon.env" "$CONFIG_DIR/datamon.env"

chmod +x "$APP_DIR/bin/"* "$APP_DIR/load_modules.sh" "$APP_DIR/unload_modules.sh"

if [ -d /etc/init.d ] && [ -f "$SRC_DIR/init.d/S99datamon" ]; then
	cp "$SRC_DIR/init.d/S99datamon" /etc/init.d/S99datamon
	chmod +x /etc/init.d/S99datamon
	echo "installed BusyBox init script: /etc/init.d/S99datamon"
fi

if command -v systemctl >/dev/null 2>&1 && [ -f "$SRC_DIR/systemd/datamon.service" ]; then
	cp "$SRC_DIR/systemd/datamon.service" /etc/systemd/system/datamon.service
	systemctl daemon-reload
	echo "installed systemd service: datamon.service"
fi

echo "installed DataMonitor to $APP_DIR"
