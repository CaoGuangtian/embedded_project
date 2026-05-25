#!/usr/bin/env python3

import argparse
import json
import socket
import threading
import time


class DeviceSession:
    def __init__(self):
        self.conn = None
        self.addr = None
        self.last_status = None
        self.lock = threading.Lock()

    def attach(self, conn, addr):
        with self.lock:
            old = self.conn
            self.conn = conn
            self.addr = addr
            self.last_status = None
        if old:
            try:
                old.close()
            except OSError:
                pass

    def detach(self, conn):
        with self.lock:
            if self.conn is conn:
                self.conn = None
                self.addr = None

    def send(self, obj):
        data = (json.dumps(obj, separators=(",", ":")) + "\n").encode()
        with self.lock:
            conn = self.conn
        if not conn:
            print("no device connected")
            return False
        try:
            conn.sendall(data)
            return True
        except OSError as exc:
            print(f"send failed: {exc}")
            return False

    def update_status(self, msg):
        with self.lock:
            self.last_status = msg

    def snapshot(self):
        with self.lock:
            return self.addr, self.last_status


def format_status(status):
    if not status:
        return "no status yet"

    env = "env=ERR"
    if status.get("env_ok"):
        env = (
            f"ir={status.get('ir_filtered', status.get('ir'))} "
            f"lux={status.get('als_lux', status.get('als'))} "
            f"ps={status.get('ps_filtered', status.get('ps'))}"
        )

    imu = "imu=ERR"
    if status.get("imu_ok"):
        imu = (
            f"acc_g=({status.get('accel_x_g', status.get('accel_x'))},"
            f"{status.get('accel_y_g', status.get('accel_y'))},"
            f"{status.get('accel_z_g', status.get('accel_z'))}) "
            f"temp_c={status.get('temp_c', status.get('temp'))} "
            f"gyro_dps=({status.get('gyro_x_dps', status.get('gyro_x'))},"
            f"{status.get('gyro_y_dps', status.get('gyro_y'))},"
            f"{status.get('gyro_z_dps', status.get('gyro_z'))})"
        )

    return (
        f"mode={status.get('mode')} interval={status.get('interval_ms')}ms "
        f"led={status.get('led')} beep={status.get('beep')} {env} {imu}"
    )


def handle_client(conn, addr, session):
    print(f"device connected: {addr}")
    session.attach(conn, addr)
    buffer = ""

    try:
        with conn:
            while True:
                data = conn.recv(4096)
                if not data:
                    break

                buffer += data.decode("utf-8", errors="replace")
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue

                    try:
                        msg = json.loads(line)
                    except json.JSONDecodeError:
                        print(f"{addr}: raw: {line}")
                        continue

                    msg_type = msg.get("type")
                    if msg_type == "status":
                        session.update_status(msg)
                        print(f"{addr}: {format_status(msg)}")
                    elif msg_type == "ack":
                        print(
                            f"{addr}: ack cmd={msg.get('cmd')} "
                            f"ok={msg.get('ok')} msg={msg.get('msg')}"
                        )
                    elif msg_type == "log_line":
                        print(f"{addr}: log[{msg.get('index')}]: {msg.get('text')}")
                    elif msg_type == "register":
                        print(
                            f"{addr}: register device={msg.get('device')} "
                            f"version={msg.get('version')}"
                        )
                    else:
                        print(f"{addr}: {msg}")
    finally:
        print(f"device disconnected: {addr}")
        session.detach(conn)


def server_thread(host, port, session):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((host, port))
        server.listen(5)
        print(f"listening on {host}:{port}")

        while True:
            conn, addr = server.accept()
            thread = threading.Thread(
                target=handle_client, args=(conn, addr, session), daemon=True
            )
            thread.start()


def print_help():
    print(
        "commands:\n"
        "  status\n"
        "  led 0|1\n"
        "  beep 0|1\n"
        "  interval <ms>\n"
        "  filter <alpha_percent>\n"
        "  threshold ps <value>\n"
        "  threshold als <value>\n"
        "  mode normal|quiet|alarm_only\n"
        "  saveconfig\n"
        "  log [lines]\n"
        "  shutdown\n"
        "  help\n"
        "  quit"
    )


def interactive_loop(session):
    print_help()
    while True:
        try:
            line = input("p1> ").strip()
        except EOFError:
            print()
            return
        except KeyboardInterrupt:
            print()
            return

        if not line:
            continue

        parts = line.split()
        cmd = parts[0]

        if cmd in ("quit", "exit"):
            return
        if cmd == "help":
            print_help()
            continue
        if cmd == "status":
            addr, status = session.snapshot()
            print(f"device={addr} {format_status(status)}")
            continue

        obj = {"type": "command"}
        try:
            if cmd == "led" and len(parts) == 2:
                obj.update({"cmd": "set_led", "value": int(parts[1])})
            elif cmd == "beep" and len(parts) == 2:
                obj.update({"cmd": "set_beep", "value": int(parts[1])})
            elif cmd == "interval" and len(parts) == 2:
                obj.update({"cmd": "set_interval", "value": int(parts[1])})
            elif cmd == "filter" and len(parts) == 2:
                obj.update({"cmd": "set_filter", "alpha": int(parts[1])})
            elif cmd == "threshold" and len(parts) == 3 and parts[1] in ("ps", "als"):
                obj.update({"cmd": "set_threshold", parts[1]: int(parts[2])})
            elif cmd == "mode" and len(parts) == 2:
                obj.update({"cmd": "set_mode", "mode": parts[1]})
            elif cmd == "saveconfig":
                obj.update({"cmd": "save_config"})
            elif cmd == "log":
                obj.update({"cmd": "get_log", "lines": int(parts[1]) if len(parts) > 1 else 10})
            elif cmd == "shutdown":
                obj.update({"cmd": "shutdown"})
            else:
                print("bad command, type help")
                continue
        except ValueError:
            print("numeric argument expected")
            continue

        session.send(obj)


def main():
    parser = argparse.ArgumentParser(description="Project1 PC management server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", default=9000, type=int)
    args = parser.parse_args()

    session = DeviceSession()
    thread = threading.Thread(
        target=server_thread, args=(args.host, args.port, session), daemon=True
    )
    thread.start()

    time.sleep(0.1)
    interactive_loop(session)


if __name__ == "__main__":
    main()
