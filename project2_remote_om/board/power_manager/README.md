# Power Manager

Planned responsibilities:

- Manage work modes: `normal`, `idle`, `low_power`, `sleep`, `maintenance`
- Apply mode-specific service policies
- Enter Linux suspend when allowed
- Configure RTC/GPIO/key wakeup flows
- Restore state after wakeup
- Report mode and wakeup events to `device_agent`

Suggested source layout:

- `include/`: public headers for this program
- `src/`: C source files

