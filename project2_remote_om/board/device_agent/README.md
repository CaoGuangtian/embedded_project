# Device Agent

Planned responsibilities:

- Load runtime configuration
- Connect to the PC/server side
- Send device registration messages
- Send periodic heartbeat messages
- Report system status
- Receive remote commands
- Return command ACK messages
- Query logs
- Trigger service management and OTA flows

Suggested source layout:

- `include/`: public headers for this program
- `src/`: C source files

