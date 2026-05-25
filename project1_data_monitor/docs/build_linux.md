# Linux Build Notes

Run these commands in Linux or WSL2, not directly in Windows PowerShell.

## Kernel modules

Before building modules, merge `device_tree/project1-imx6ull-alientek-snippet.dts`
into your board DTS and rebuild the DTB. The snippet disables stock nodes that
would otherwise own the same LED, BEEP, KEY, AP3216C, and ICM20608 resources.

```bash
cd project1_data_monitor/kernel_modules
make KERNEL_DIR=/path/to/linux-imx-4.1.15-2.1.0-e48931b1-v2.8 \
     ARCH=arm \
     CROSS_COMPILE=arm-linux-gnueabihf-
```

Copy the generated `.ko` files to the board, then load them after booting with
the matching DTB:

```bash
insmod p1_led.ko
insmod p1_beep.ko
insmod p1_key.ko
insmod p1_ap3216c.ko
insmod p1_icm20608.ko
```

## Userspace tools

```bash
cd project1_data_monitor/userspace
make CC=arm-linux-gnueabihf-gcc
```

Example checks on the board:

```bash
./tools/p1_outctl /dev/p1_led 1
./tools/p1_outctl /dev/p1_beep 1
./tools/p1_keyread
./tools/p1_ap3216c_read
./tools/p1_icm20608_read
```

The collector links with pthread:

```bash
./collector/p1_collector -s 192.168.10.100 -p 9000 \
  -i 1000 -l /mnt/tf/project1_samples.csv
```

## PC server

Run this on the PC:

```bash
cd project1_data_monitor/pc
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
mode normal|quiet|alarm_only
shutdown
```
