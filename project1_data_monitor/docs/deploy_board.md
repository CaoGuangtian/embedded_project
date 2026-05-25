# Board Deployment Notes

Expected target layout:

```text
/opt/project1/
├── bin/
│   ├── p1_collector
│   ├── p1_outctl
│   ├── p1_keyread
│   ├── p1_ap3216c_read
│   └── p1_icm20608_read
├── modules/
│   ├── p1_led.ko
│   ├── p1_beep.ko
│   ├── p1_key.ko
│   ├── p1_ap3216c.ko
│   └── p1_icm20608.ko
├── load_modules.sh
├── unload_modules.sh
└── project1.log
```

Manual bring-up:

```bash
cd /opt/project1
./load_modules.sh ./modules
./bin/p1_outctl /dev/p1_led 1
./bin/p1_outctl /dev/p1_beep 1
./bin/p1_keyread
./bin/p1_ap3216c_read
./bin/p1_icm20608_read
./bin/p1_collector -s 192.168.10.100 -p 9000 -l /mnt/tf/project1_samples.csv
```

BusyBox init.d installation:

```bash
cp S99project1 /etc/init.d/
chmod +x /etc/init.d/S99project1
/etc/init.d/S99project1 start
```

Optional runtime environment variables:

```bash
SERVER_IP=192.168.10.100
SERVER_PORT=9000
LOG_PATH=/mnt/tf/project1_samples.csv
INTERVAL_MS=1000
```

