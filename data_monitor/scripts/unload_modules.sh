#!/bin/sh

set -eu

for mod in dm_icm20608 dm_ap3216c dm_key dm_beep dm_led; do
	if lsmod | awk '{print $1}' | grep -qx "$mod"; then
		rmmod "$mod"
	fi
done

echo "datamon modules unloaded"
