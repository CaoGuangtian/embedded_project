#!/bin/sh

set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/release/remote_om}"

mkdir -p "$OUT_DIR/bin" "$OUT_DIR/config" "$OUT_DIR/init.d" "$OUT_DIR/systemd"

copy_if_exists() {
	src="$1"
	dst="$2"

	if [ -e "$src" ]; then
		cp "$src" "$dst"
	else
		echo "missing: $src" >&2
	fi
}

copy_if_exists "$ROOT_DIR/board/device_agent/device_agent" "$OUT_DIR/bin/"
copy_if_exists "$ROOT_DIR/board/power_manager/power_manager" "$OUT_DIR/bin/"
copy_if_exists "$ROOT_DIR/board/scripts/device_agent.conf" "$OUT_DIR/config/"
copy_if_exists "$ROOT_DIR/board/scripts/power_manager.conf" "$OUT_DIR/config/"
copy_if_exists "$ROOT_DIR/board/scripts/S98power_manager" "$OUT_DIR/init.d/"
copy_if_exists "$ROOT_DIR/board/scripts/S99device_agent" "$OUT_DIR/init.d/"
copy_if_exists "$ROOT_DIR/board/scripts/power_manager.service" "$OUT_DIR/systemd/"
copy_if_exists "$ROOT_DIR/board/scripts/device_agent.service" "$OUT_DIR/systemd/"
copy_if_exists "$ROOT_DIR/board/scripts/install.sh" "$OUT_DIR/"

if [ -f "$OUT_DIR/install.sh" ]; then
	chmod +x "$OUT_DIR/install.sh"
fi
if [ -f "$OUT_DIR/init.d/S99device_agent" ]; then
	chmod +x "$OUT_DIR/init.d/S99device_agent"
fi
if [ -f "$OUT_DIR/init.d/S98power_manager" ]; then
	chmod +x "$OUT_DIR/init.d/S98power_manager"
fi
if [ -f "$OUT_DIR/bin/device_agent" ]; then
	chmod +x "$OUT_DIR/bin/device_agent"
else
	echo "warning: release has no device_agent binary yet" >&2
fi
if [ -f "$OUT_DIR/bin/power_manager" ]; then
	chmod +x "$OUT_DIR/bin/power_manager"
else
	echo "warning: release has no power_manager binary yet" >&2
fi

cat > "$OUT_DIR/README.txt" <<'EOF'
Remote OM board release

Install:

  ./install.sh .

Start with BusyBox init.d:

  /etc/init.d/S99device_agent start

Start with systemd:

  systemctl start device_agent
EOF

echo "release staged at $OUT_DIR"
