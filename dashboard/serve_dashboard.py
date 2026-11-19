from __future__ import annotations

import argparse
import os
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parent


class DashboardHandler(SimpleHTTPRequestHandler):
    def end_headers(self) -> None:
        self.send_header("Cache-Control", "no-store, max-age=0")
        self.send_header("Pragma", "no-cache")
        super().end_headers()

    def log_message(self, format_string: str, *args: object) -> None:
        print(f"[网页] {self.address_string()} - {format_string % args}")


def find_supported_browser() -> Path | None:
    candidates = [
        Path(os.environ.get("PROGRAMFILES(X86)", "")) / "Microsoft/Edge/Application/msedge.exe",
        Path(os.environ.get("PROGRAMFILES", "")) / "Microsoft/Edge/Application/msedge.exe",
        Path(os.environ.get("PROGRAMFILES", "")) / "Google/Chrome/Application/chrome.exe",
        Path(os.environ.get("PROGRAMFILES(X86)", "")) / "Google/Chrome/Application/chrome.exe",
        Path(os.environ.get("LOCALAPPDATA", "")) / "Google/Chrome/Application/chrome.exe",
    ]
    return next((path for path in candidates if path.is_file()), None)


def open_browser(url: str) -> None:
    time.sleep(0.7)
    browser = find_supported_browser()
    if browser:
        subprocess.Popen([str(browser), url])
    else:
        webbrowser.open(url)


def main() -> int:
    parser = argparse.ArgumentParser(description="启动SEN0639 LoRa风速监测网页")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--no-browser", action="store_true")
    parser.add_argument("--demo", action="store_true", help="启动后自动显示模拟数据")
    args = parser.parse_args()

    os.chdir(ROOT)
    server = ThreadingHTTPServer((args.host, args.port), DashboardHandler)
    suffix = "?demo=1" if args.demo else ""
    url = f"http://localhost:{args.port}/{suffix}"

    print("=" * 62)
    print("SEN0639 EoRa 风速遥测监测平台")
    print(f"页面地址：{url}")
    print("保持本窗口打开；按 Ctrl+C 停止网页服务。")
    print("=" * 62)

    if not args.no_browser:
        threading.Thread(target=open_browser, args=(url,), daemon=True).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n监测页面已停止。")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
