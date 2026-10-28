#!/usr/bin/env python3
"""
电容触摸传感器实时监视器

读取 XIAO 串口输出的 "raw,smoothed" 数据,通过本地网页实时显示曲线。

用法:
    python3 touch_monitor.py                # 自动找 /dev/cu.usbmodem*
    python3 touch_monitor.py -p /dev/cu.usbmodem1101 --http-port 8765

浏览器打开 http://localhost:8765
"""

import argparse
import glob
import json
import os
import queue
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import serial

HTML_PATH = Path(__file__).with_name("touch_monitor.html")


class SerialReader(threading.Thread):
    def __init__(self, port: str, baud: int):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.ser = None
        self.connected = False
        self.lock = threading.Lock()
        self.subscribers: list[queue.Queue] = []
        self.latest = None

    def subscribe(self) -> queue.Queue:
        q: queue.Queue = queue.Queue(maxsize=500)
        with self.lock:
            self.subscribers.append(q)
        return q

    def unsubscribe(self, q: queue.Queue) -> None:
        with self.lock:
            if q in self.subscribers:
                self.subscribers.remove(q)

    def broadcast(self, msg: dict) -> None:
        with self.lock:
            subs = list(self.subscribers)
        for q in subs:
            try:
                q.put_nowait(msg)
            except queue.Full:
                pass

    def send_reset(self) -> bool:
        if self.ser and self.connected:
            try:
                self.ser.write(b"r")
                return True
            except serial.SerialException:
                return False
        return False

    def run(self) -> None:
        while True:
            try:
                if not os.path.exists(self.port):
                    found = find_port()
                    if not found:
                        time.sleep(1.5)
                        continue
                    self.port = found
                self.ser = serial.Serial(self.port, self.baud, timeout=1)
                self.connected = True
                self.broadcast({"type": "status", "connected": True, "port": self.port})
                print(f"[serial] 已连接 {self.port}", flush=True)
                self.ser.reset_input_buffer()
                while True:
                    line = self.ser.readline()
                    if not line:
                        continue
                    text = line.decode("ascii", errors="ignore").strip()
                    parts = text.split(",")
                    if len(parts) != 2:
                        continue
                    try:
                        raw = int(parts[0])
                        smooth = int(parts[1])
                    except ValueError:
                        continue
                    msg = {"type": "sample", "t": time.time(), "raw": raw, "smooth": smooth}
                    self.latest = msg
                    self.broadcast(msg)
            except serial.SerialException as exc:
                if self.connected:
                    print(f"[serial] 断开: {exc}", flush=True)
                self.connected = False
                self.broadcast({"type": "status", "connected": False, "port": self.port})
                try:
                    if self.ser:
                        self.ser.close()
                except Exception:
                    pass
                time.sleep(1.5)


def make_handler(reader: SerialReader):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):  # quiet
            pass

        def do_GET(self):
            if self.path in ("/", "/index.html"):
                body = HTML_PATH.read_bytes()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            elif self.path == "/stream":
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.send_header("X-Accel-Buffering", "no")
                self.end_headers()
                q = reader.subscribe()
                try:
                    status = {"type": "status", "connected": reader.connected, "port": reader.port}
                    self.wfile.write(f"data: {json.dumps(status)}\n\n".encode())
                    self.wfile.flush()
                    while True:
                        try:
                            msg = q.get(timeout=5)
                            self.wfile.write(f"data: {json.dumps(msg)}\n\n".encode())
                        except queue.Empty:
                            self.wfile.write(b": ping\n\n")
                        self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError, OSError):
                    pass
                finally:
                    reader.unsubscribe(q)
            else:
                self.send_error(404)

        def do_POST(self):
            if self.path == "/reset":
                ok = reader.send_reset()
                body = json.dumps({"ok": ok}).encode()
                self.send_response(200 if ok else 503)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            else:
                self.send_error(404)

    return Handler


def find_port() -> str | None:
    candidates = sorted(glob.glob("/dev/cu.usbmodem*")) + sorted(glob.glob("/dev/ttyACM*")) + sorted(
        glob.glob("/dev/ttyUSB*")
    )
    return candidates[0] if candidates else None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", help="串口设备,默认自动查找")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("--http-port", type=int, default=8765)
    args = ap.parse_args()

    port = args.port or find_port()
    if not port:
        print("找不到串口设备,请用 -p 指定", file=sys.stderr)
        return 1

    reader = SerialReader(port, args.baud)
    reader.start()

    server = ThreadingHTTPServer(("127.0.0.1", args.http_port), make_handler(reader))
    server.daemon_threads = True
    print(f"[http] 打开 http://localhost:{args.http_port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
