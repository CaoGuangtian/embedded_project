#!/bin/sh

set -eu

for mod in p1_icm20608 p1_ap3216c p1_key p1_beep p1_led; do
	if lsmod | awk '{print $1}' | grep -qx "$mod"; then
		rmmod "$mod"
	fi
done

echo "project1 modules unloaded"

