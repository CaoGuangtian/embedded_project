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
        self.device_id = None
        self.last_message = None
        self.lock = threading.Lock()

    def attach(self, conn, addr):
        with self.lock:
            old = self.conn
            self.conn = conn
            self.addr = addr
            self.device_id = None
            self.last_message = None

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

    def update(self, msg):
        with self.lock:
            self.last_message = msg
            if msg.get("device_id"):
                self.device_id = msg.get("device_id")

    def snapshot(self):
        with self.lock:
            return self.addr, self.device_id, self.last_message


def compact_json(obj):
    return json.dumps(obj, separators=(",", ":"))


def make_ack(msg, result="ok", text="ok"):
    return {
        "type": "ack",
        "device_id": msg.get("device_id", "unknown"),
        "seq": msg.get("seq", 0),
        "timestamp": int(time.time()),
        "payload": {
            "result": result,
            "msg": text,
        },
    }


def send_json_line(conn, obj):
    data = (compact_json(obj) + "\n").encode("utf-8")
    conn.sendall(data)


def describe_message(msg):
    msg_type = msg.get("type", "unknown")
    device_id = msg.get("device_id", "unknown")
    seq = msg.get("seq", 0)
    payload = msg.get("payload", {})

    if msg_type == "register":
        model = payload.get("model", "unknown")
        fw = payload.get("fw_version", "unknown")
        return f"register device={device_id} model={model} fw={fw} seq={seq}"

    if msg_type == "heartbeat":
        uptime = payload.get("uptime", "unknown")
        return f"heartbeat device={device_id} uptime={uptime} seq={seq}"

    if msg_type == "status_report":
        uptime = payload.get("uptime", "unknown")
        mem_total = payload.get("mem_total_kb", "unknown")
        mem_avail = payload.get("mem_available_kb", "unknown")
        rootfs = payload.get("rootfs_usage", "unknown")
        ifname = payload.get("net_ifname", "unknown")
        state = payload.get("net_state", "unknown")
        fw = payload.get("fw_version", "unknown")
        return (
            f"status device={device_id} uptime={uptime}s "
            f"mem={mem_avail}/{mem_total}KB rootfs={rootfs}% "
            f"{ifname}={state} fw={fw} seq={seq}"
        )

    return f"{msg_type} device={device_id} payload={payload} seq={seq}"


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
                    except json.JSONDecodeError as exc:
                        print(f"{addr}: bad json: {exc}: {line}")
                        continue

                    session.update(msg)
                    print(f"{addr}: {describe_message(msg)}")
                    send_json_line(conn, make_ack(msg))
    except OSError as exc:
        print(f"{addr}: socket error: {exc}")
    finally:
        print(f"device disconnected: {addr}")
        session.detach(conn)


def server_loop(host, port, session):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((host, port))
        server.listen(5)
        print(f"server listening on {host}:{port}")

        while True:
            conn, addr = server.accept()
            thread = threading.Thread(
                target=handle_client,
                args=(conn, addr, session),
                daemon=True,
            )
            thread.start()


def interactive_loop(session):
    print("commands: status, quit")

    while True:
        try:
            line = input("p2> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return

        if not line:
            continue
        if line in ("quit", "exit"):
            return
        if line == "status":
            addr, device_id, msg = session.snapshot()
            print(f"addr={addr} device_id={device_id} last={msg}")
            continue

        print("unknown command")


def main():
    parser = argparse.ArgumentParser(description="Project2 PC management server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", default=9000, type=int)
    args = parser.parse_args()

    session = DeviceSession()
    thread = threading.Thread(
        target=server_loop,
        args=(args.host, args.port, session),
        daemon=True,
    )
    thread.start()

    interactive_loop(session)


if __name__ == "__main__":
    main()
