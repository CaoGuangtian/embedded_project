#!/bin/sh

set -eu

MOD_DIR="${1:-/opt/datamon/modules}"

load_one() {
	name="$1"
	if lsmod | awk '{print $1}' | grep -qx "${name%.ko}"; then
		echo "$name already loaded"
		return 0
	fi
	insmod "$MOD_DIR/$name"
}

load_one dm_beep.ko
load_one dm_ap3216c_iio.ko
load_one dm_icm20608_iio.ko

echo "datamon modules loaded"
