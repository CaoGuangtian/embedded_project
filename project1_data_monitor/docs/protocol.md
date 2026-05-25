# TCP Protocol

Each message is one UTF-8 JSON object followed by `\n`.

## Board to PC

Register:

```json
{"type":"register","device":"imx6ull-project1","version":"1.0"}
```

Status:

```json
{
  "type": "status",
  "ts": 1710000000,
  "mode": "normal",
  "interval_ms": 1000,
  "led": 1,
  "beep": 0,
  "env_ok": 1,
  "ir": 10,
  "als": 1200,
  "ps": 300,
  "imu_ok": 1,
  "accel_x": 1,
  "accel_y": 2,
  "accel_z": 3,
  "temp": 4,
  "gyro_x": 5,
  "gyro_y": 6,
  "gyro_z": 7
}
```

ACK:

```json
{"type":"ack","cmd":"set_led","ok":1,"msg":"ok"}
```

## PC to Board

```json
{"type":"command","cmd":"set_led","value":1}
{"type":"command","cmd":"set_beep","value":0}
{"type":"command","cmd":"set_interval","value":2000}
{"type":"command","cmd":"set_threshold","ps":1000}
{"type":"command","cmd":"set_threshold","als":60000}
{"type":"command","cmd":"set_mode","mode":"normal"}
{"type":"command","cmd":"set_mode","mode":"quiet"}
{"type":"command","cmd":"set_mode","mode":"alarm_only"}
{"type":"command","cmd":"shutdown"}
```

