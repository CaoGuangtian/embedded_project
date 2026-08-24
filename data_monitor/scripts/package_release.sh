#!/bin/sh

set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/release/datamon}"

mkdir -p "$OUT_DIR/bin" "$OUT_DIR/modules" "$OUT_DIR/config" "$OUT_DIR/init.d" "$OUT_DIR/systemd"

copy_if_exists() {
	src="$1"
	dst="$2"
	if [ -e "$src" ]; then
		cp "$src" "$dst"
	else
		echo "missing: $src" >&2
	fi
}

for app in dm_collector dm_outctl dm_keyread dm_ap3216c_read dm_icm20608_read; do
	if [ -e "$ROOT_DIR/userspace/collector/$app" ]; then
		cp "$ROOT_DIR/userspace/collector/$app" "$OUT_DIR/bin/"
	elif [ -e "$ROOT_DIR/userspace/tools/$app" ]; then
		cp "$ROOT_DIR/userspace/tools/$app" "$OUT_DIR/bin/"
	else
		echo "missing binary: $app" >&2
	fi
done

for mod in dm_led.ko dm_beep.ko dm_key.ko dm_ap3216c.ko dm_icm20608.ko; do
	copy_if_exists "$ROOT_DIR/kernel_modules/$mod" "$OUT_DIR/modules/"
done

cp "$ROOT_DIR/scripts/load_modules.sh" "$OUT_DIR/"
cp "$ROOT_DIR/scripts/unload_modules.sh" "$OUT_DIR/"
cp "$ROOT_DIR/scripts/install_board.sh" "$OUT_DIR/"
cp "$ROOT_DIR/scripts/datamon.env" "$OUT_DIR/config/"
cp "$ROOT_DIR/scripts/S99datamon" "$OUT_DIR/init.d/"
cp "$ROOT_DIR/scripts/datamon.service" "$OUT_DIR/systemd/"

chmod +x "$OUT_DIR/load_modules.sh" "$OUT_DIR/unload_modules.sh" "$OUT_DIR/install_board.sh" "$OUT_DIR/init.d/S99datamon"

echo "release staged at $OUT_DIR"
