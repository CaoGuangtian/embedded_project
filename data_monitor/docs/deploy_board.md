# Board Deployment Notes

Expected target layout:

```text
/opt/datamon/
├── bin/
│   ├── dm_collector
│   ├── dm_outctl
│   ├── dm_keyread
│   ├── dm_ap3216c_read
│   └── dm_icm20608_read
├── modules/
│   ├── dm_led.ko
│   ├── dm_beep.ko
│   ├── dm_key.ko
│   ├── dm_ap3216c.ko
│   └── dm_icm20608.ko
├── load_modules.sh
├── unload_modules.sh
├── config/
│   └── datamon.env
├── init.d/
│   └── S99datamon
├── systemd/
│   └── datamon.service
└── datamon.log
```

Manual bring-up:

```bash
cd /opt/datamon
./load_modules.sh ./modules
./bin/dm_outctl /dev/dm_led 1
./bin/dm_outctl /dev/dm_beep 1
./bin/dm_keyread
./bin/dm_ap3216c_read
./bin/dm_icm20608_read
./bin/dm_collector -s 192.168.10.100 -p 9000 \
  -l /mnt/tf/datamon_samples.csv --max-log-kb 1024
```

Pack on the build host:

```bash
cd data_monitor
./scripts/package_release.sh
```

Install on the board after copying `data_monitor/release/datamon`:

```bash
cd /path/to/release/datamon
./install_board.sh .
```

BusyBox init.d installation:

```bash
cp S99datamon /etc/init.d/
chmod +x /etc/init.d/S99datamon
/etc/init.d/S99datamon start
```

systemd installation:

```bash
cp systemd/datamon.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable datamon
systemctl start datamon
```

Optional runtime environment variables:

```bash
SERVER_IP=192.168.10.100
SERVER_PORT=9000
LOG_PATH=/mnt/tf/datamon_samples.csv
INTERVAL_MS=1000
MAX_LOG_KB=1024
PS_THRESHOLD=1000
ALS_THRESHOLD=60000
FILTER_ALPHA_PERCENT=35
```

Default config file:

```text
/etc/datamon/datamon.env
```

The collector writes CSV logs to `LOG_PATH`. When the file grows beyond
`MAX_LOG_KB`, it is rotated to `<LOG_PATH>.1`.

Runtime changes sent from the PC can be persisted with the `save_config`
command. The collector writes them back to `/etc/datamon/datamon.env`.
