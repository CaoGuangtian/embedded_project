# i.MX6ULL Data Monitor

This repository area contains source code for the i.MX6ULL multi-peripheral
data collection and remote monitoring project.

The code is intentionally kept outside the Linux kernel source tree:

- `kernel_modules/`: external Linux kernel modules built with `make -C KERNEL_DIR M=$PWD modules`
- `device_tree/`: DTS snippets to merge into the board DTS file
- `userspace/`: board-side test tools and collector daemon
- `pc/`: simple PC-side TCP monitor for early testing
- `docs/`: build and bring-up notes

The first bring-up target is:

1. Apply the DTS snippet to the board DTS.
2. Build and load the kernel modules.
3. Verify the standard LED, input, IIO, and `/dev/dm_beep` interfaces.
4. Run the userspace tools one by one.

## Current Feature Set

- GPIO LED exposed through the kernel `gpio-leds` and LED class
- GPIO beep driver exposed as `/dev/dm_beep`
- GPIO key exposed through the kernel `gpio-keys` and input subsystem
- AP3216C I2C driver exposed through IIO
- ICM20608 SPI driver exposed through IIO
- Board collector with periodic sampling, alarm policy, local CSV logging,
  key-based mode switching, TCP status upload, and remote commands
- Engineering-unit conversion, simple exponential filtering, runtime config
  persistence, and remote log tail query
- PC management server with interactive commands
- Qt/CMake desktop monitor with live status and command controls
- BusyBox-style startup scripts
- systemd service, runtime env file, release packaging and board install script

## Recommended Development Order

1. Build and boot the DTB with the project DTS snippet.
2. Build and load kernel modules.
3. Test each `/dev/dm_*` node with the small tools.
4. Run `pc/manage_server.py` on the PC.
5. Optionally run `pc_qt/datamon_gui` as the GUI monitor.
6. Run `userspace/collector/dm_collector` on the board.
7. Add the init script once manual testing works.
8. Package a release with `scripts/package_release.sh` for repeatable install.
