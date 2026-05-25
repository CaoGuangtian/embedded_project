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
├── config/
│   └── project1.env
├── init.d/
│   └── S99project1
├── systemd/
│   └── project1.service
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
./bin/p1_collector -s 192.168.10.100 -p 9000 \
  -l /mnt/tf/project1_samples.csv --max-log-kb 1024
```

Pack on the build host:

```bash
cd project1_data_monitor
./scripts/package_release.sh
```

Install on the board after copying `project1_data_monitor/release/project1`:

```bash
cd /path/to/release/project1
./install_board.sh .
```

BusyBox init.d installation:

```bash
cp S99project1 /etc/init.d/
chmod +x /etc/init.d/S99project1
/etc/init.d/S99project1 start
```

systemd installation:

```bash
cp systemd/project1.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable project1
systemctl start project1
```

Optional runtime environment variables:

```bash
SERVER_IP=192.168.10.100
SERVER_PORT=9000
LOG_PATH=/mnt/tf/project1_samples.csv
INTERVAL_MS=1000
MAX_LOG_KB=1024
PS_THRESHOLD=1000
ALS_THRESHOLD=60000
```

Default config file:

```text
/etc/project1/project1.env
```

The collector writes CSV logs to `LOG_PATH`. When the file grows beyond
`MAX_LOG_KB`, it is rotated to `<LOG_PATH>.1`.
