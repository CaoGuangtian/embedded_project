# Linux Build Notes

Run these commands in Linux or WSL2, not directly in Windows PowerShell.

## Kernel modules

Before building modules, merge `device_tree/datamon-imx6ull-alientek-snippet.dts`
into your board DTS and rebuild the DTB. The snippet uses standard `gpio-leds`
and `gpio-keys` nodes and custom IIO nodes for the two sensors.

```bash
cd data_monitor/kernel_modules
make KERNEL_DIR=/path/to/linux-imx-4.1.15-2.1.0-e48931b1-v2.8 \
     ARCH=arm \
     CROSS_COMPILE=arm-linux-gnueabihf-
```

Copy the generated `.ko` files to the board, then load them after booting with
the matching DTB:

```bash
insmod dm_beep.ko
insmod dm_ap3216c_iio.ko
insmod dm_icm20608_iio.ko
```

## Userspace tools

```bash
cd data_monitor/userspace
make CC=arm-linux-gnueabihf-gcc
```

Example checks on the board:

```bash
./tools/dm_outctl /sys/class/leds/datamon:green/brightness 1
./tools/dm_outctl /dev/dm_beep 1
./tools/dm_keyread
./tools/dm_ap3216c_read
./tools/dm_icm20608_read
```

The collector links with pthread:

```bash
./collector/dm_collector -s 192.168.10.100 -p 9000 \
  -i 1000 -l /mnt/tf/datamon_samples.csv \
  --max-log-kb 1024 --ps 1000 --als 60000 \
  --filter-alpha 35 --config /etc/datamon/datamon.env
```

## Package release

After modules and userspace binaries are built:

```bash
cd data_monitor
./scripts/package_release.sh
```

This stages a release under `release/datamon/`.

## PC server

Run the Python management server on the PC:

```bash
cd data_monitor/pc
python3 manage_server.py --host 0.0.0.0 --port 9000
```

Interactive commands:

```text
status
led 0|1
beep 0|1
interval <ms>
threshold ps <value>
threshold als <value>
filter <alpha_percent>
mode normal|quiet|alarm_only
saveconfig
log [lines]
shutdown
```

For a GUI monitor, see `docs/build_qt.md`.
