#!/bin/sh

set -eu

for mod in dm_icm20608_iio dm_ap3216c_iio dm_beep; do
	if lsmod | awk '{print $1}' | grep -qx "$mod"; then
		rmmod "$mod"
	fi
done

echo "datamon modules unloaded"
