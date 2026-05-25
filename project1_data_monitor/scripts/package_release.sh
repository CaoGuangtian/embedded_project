#!/bin/sh

set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/release/project1}"

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

for app in p1_collector p1_outctl p1_keyread p1_ap3216c_read p1_icm20608_read; do
	if [ -e "$ROOT_DIR/userspace/collector/$app" ]; then
		cp "$ROOT_DIR/userspace/collector/$app" "$OUT_DIR/bin/"
	elif [ -e "$ROOT_DIR/userspace/tools/$app" ]; then
		cp "$ROOT_DIR/userspace/tools/$app" "$OUT_DIR/bin/"
	else
		echo "missing binary: $app" >&2
	fi
done

for mod in p1_led.ko p1_beep.ko p1_key.ko p1_ap3216c.ko p1_icm20608.ko; do
	copy_if_exists "$ROOT_DIR/kernel_modules/$mod" "$OUT_DIR/modules/"
done

cp "$ROOT_DIR/scripts/load_modules.sh" "$OUT_DIR/"
cp "$ROOT_DIR/scripts/unload_modules.sh" "$OUT_DIR/"
cp "$ROOT_DIR/scripts/install_board.sh" "$OUT_DIR/"
cp "$ROOT_DIR/scripts/project1.env" "$OUT_DIR/config/"
cp "$ROOT_DIR/scripts/S99project1" "$OUT_DIR/init.d/"
cp "$ROOT_DIR/scripts/project1.service" "$OUT_DIR/systemd/"

chmod +x "$OUT_DIR/load_modules.sh" "$OUT_DIR/unload_modules.sh" "$OUT_DIR/install_board.sh" "$OUT_DIR/init.d/S99project1"

echo "release staged at $OUT_DIR"
