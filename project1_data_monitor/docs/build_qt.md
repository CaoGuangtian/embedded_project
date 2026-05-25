# Qt Monitor Build Notes

The Qt monitor is an optional PC-side GUI for Project1. It reuses the same
line-based JSON TCP protocol as `pc/manage_server.py`.

## Requirements

- Qt 5 or Qt 6
- CMake 3.16 or newer
- A desktop C++ compiler

## Build with CMake

```bash
cd project1_data_monitor/pc_qt
cmake -S . -B build
cmake --build build
```

On Windows with Qt Creator, open `pc_qt/CMakeLists.txt`, choose a Qt kit, then
build and run `project1_monitor`.

## Usage

1. Start the Qt monitor.
2. Listen on `0.0.0.0:9000`.
3. Start the board collector:

```bash
/opt/project1/bin/p1_collector -s <PC_IP> -p 9000
```

4. Watch status updates and use the controls:

- LED On / LED Off
- Beep On / Beep Off
- Apply Interval
- Apply Threshold
- Apply Mode
- Shutdown Collector

