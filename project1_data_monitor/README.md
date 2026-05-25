# Project 1: i.MX6ULL Data Monitor

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
3. Verify `/dev/p1_led`, `/dev/p1_beep`, `/dev/p1_key`,
   `/dev/p1_ap3216c`, and `/dev/p1_icm20608`.
4. Run the userspace tools one by one.

## Current Feature Set

- GPIO LED driver exposed as `/dev/p1_led`
- GPIO beep driver exposed as `/dev/p1_beep`
- GPIO interrupt key driver exposed as `/dev/p1_key`
- AP3216C I2C driver exposed as `/dev/p1_ap3216c`
- ICM20608 SPI driver exposed as `/dev/p1_icm20608`
- Board collector with periodic sampling, alarm policy, local CSV logging,
  key-based mode switching, TCP status upload, and remote commands
- PC management server with interactive commands
- BusyBox-style startup scripts

## Recommended Development Order

1. Build and boot the DTB with the project DTS snippet.
2. Build and load kernel modules.
3. Test each `/dev/p1_*` node with the small tools.
4. Run `pc/manage_server.py` on the PC.
5. Run `userspace/collector/p1_collector` on the board.
6. Add the init script once manual testing works.
