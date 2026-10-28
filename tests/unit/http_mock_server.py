#!/usr/bin/env python3
"""W2UCEM 断点续传单测 mock 服务器（纯 stdlib，loopback）。

用法: http_mock_server.py --file <path> [--mode normal|no-range|always-416|flaky]
                            [--flaky-bytes N] [--record <path>]

启动后首行打印 "PORT <n>"（测试端据此读取端口）。
模式:
  normal      支持 Range（206 + Content-Range）；无 Range 发 200 全量
  no-range    永远 200 全量（忽略 Range）——服务器不支持续传
  always-416  永远 416
  flaky       对 Range 请求发 206 但只写 flaky-bytes 后截断连接（模拟中断）；
              对无 Range 请求发 200 但同样截断
--record      以 206 应答过 Range 请求时写标记文件（断言"确实走了续传"）
"""
import argparse
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):  # 静默
        pass

    def _send_body(self, code, body, extra_headers=None, truncate=0):
        self.send_response(code)
        for key, value in (extra_headers or {}).items():
            self.send_header(key, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        chunk = body[:truncate] if truncate > 0 else body
        self.wfile.write(chunk)
        if truncate > 0:
            self.wfile.flush()
            self.close_connection = True  # 截断：写完即断

    def do_GET(self):
        server = self.server
        data = server.data
        mode = server.mode
        flaky = server.flaky_bytes

        if mode == "always-416":
            self.send_response(416)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return

        rng = self.headers.get("Range")
        if mode in ("normal", "flaky") and rng and rng.startswith("bytes="):
            start = int(rng[len("bytes="):].split("-")[0])
            if start >= len(data):
                self.send_response(416)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            chunk = data[start:]
            if server.record:
                with open(server.record, "w") as f:
                    f.write("206")
            self._send_body(206, chunk,
                            {"Content-Range": f"bytes {start}-{len(data)-1}/{len(data)}"},
                            truncate=flaky if mode == "flaky" else 0)
            return

        # 200 全量（no-range 模式 / flaky 无 Range / normal 无 Range）
        self._send_body(200, data, truncate=flaky if mode == "flaky" else 0)



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--file", required=True)
    parser.add_argument("--mode", default="normal")
    parser.add_argument("--flaky-bytes", type=int, default=0)
    parser.add_argument("--record", default="")
    args = parser.parse_args()

    with open(args.file, "rb") as f:
        data = f.read()

    class Server(ThreadingHTTPServer):
        daemon_threads = True

    server = Server(("127.0.0.1", 0), Handler)
    server.data = data
    server.mode = args.mode
    server.flaky_bytes = args.flaky_bytes
    server.record = args.record

    print(f"PORT {server.server_address[1]}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
