#!/bin/sh

set -eu

MOD_DIR="${1:-/opt/project1/modules}"

load_one() {
	name="$1"
	if lsmod | awk '{print $1}' | grep -qx "${name%.ko}"; then
		echo "$name already loaded"
		return 0
	fi
	insmod "$MOD_DIR/$name"
}

load_one p1_led.ko
load_one p1_beep.ko
load_one p1_key.ko
load_one p1_ap3216c.ko
load_one p1_icm20608.ko

echo "project1 modules loaded"

