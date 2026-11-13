#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SmartMate 2000 端到端联调用的假服务器（OpenAI 兼容 chat/completions）。

作用：不花钱、不联网，验证主程序的
  * 请求 JSON 是否正确（系统提示词、历史、工具定义）
  * 响应解析（content / tool_calls）
  * 工具调用循环（工具执行后把结果回传）
  * 中文 GBK <-> UTF-8 转换

用法: py mockserver.py [--port 18080] [--log mock.log]
第一轮返回 tool_calls（调用 get_system_info），第二轮返回文字答案。
"""
import argparse
import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

CALLS = {"n": 0}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(n)
        try:
            req = json.loads(raw.decode("utf-8"))
        except Exception as e:
            self.send_error(400, "bad json")
            return

        CALLS["n"] += 1
        msgs = req.get("messages", [])
        tools = req.get("tools", [])
        with open(LOG, "a", encoding="utf-8") as f:
            f.write("\n===== request #%d  uri=%s\n" % (CALLS["n"], self.path))
            f.write("model=%s  temperature=%s  max_tokens=%s  tools=%d  messages=%d\n"
                    % (req.get("model"), req.get("temperature"),
                       req.get("max_tokens"), len(tools), len(msgs)))
            for t in tools:
                f.write("  tool: %s\n" % t.get("function", {}).get("name"))
            for m in msgs:
                role = m.get("role")
                c = m.get("content")
                if isinstance(c, str):
                    show = c[:120].replace("\n", " ")
                else:
                    show = str(c)[:80]
                extra = " tool_calls=%d" % len(m["tool_calls"]) if m.get("tool_calls") else ""
                f.write("  [%s]%s %s\n" % (role, extra, show))

        last = msgs[-1] if msgs else {}
        asked_tool = any(m.get("role") == "tool" for m in msgs)

        if not asked_tool and len(tools) > 0:
            body = {
                "id": "chatcmpl-mock-1",
                "object": "chat.completion",
                "model": req.get("model", "mock"),
                "choices": [{
                    "index": 0,
                    "message": {
                        "role": "assistant",
                        "content": "我先看一下这台电脑的情况。",
                        "tool_calls": [{
                            "id": "call_mock_1",
                            "type": "function",
                            "function": {"name": "get_system_info", "arguments": "{}"},
                        }],
                    },
                    "finish_reason": "tool_calls",
                }],
                "usage": {"prompt_tokens": 100, "completion_tokens": 20, "total_tokens": 120},
            }
        else:
            tool_text = ""
            for m in msgs:
                if m.get("role") == "tool":
                    tool_text = (m.get("content") or "")
                    break
            summary = tool_text.split("\r\n")[:6]
            text = ("好的，我已经查到这台机器的情况了：\r\n\r\n"
                    + "\r\n".join("  " + s for s in summary if s.strip())
                    + "\r\n\r\n需要我接着做点什么吗？例如整理桌面文件、读一个日志，"
                      "或者写个批处理。")
            body = {
                "id": "chatcmpl-mock-2",
                "object": "chat.completion",
                "model": req.get("model", "mock"),
                "choices": [{
                    "index": 0,
                    "message": {"role": "assistant", "content": text},
                    "finish_reason": "stop",
                }],
                "usage": {"prompt_tokens": 300, "completion_tokens": 80, "total_tokens": 380},
            }

        data = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=18080)
    ap.add_argument("--host", default="127.0.0.1", help="bind address")
    ap.add_argument("--log", default="mock.log")
    args = ap.parse_args()
    LOG = args.log
    open(LOG, "w", encoding="utf-8").write("SmartMate 2000 mock server log\n")
    srv = HTTPServer((getattr(args, "host", "127.0.0.1"), args.port), Handler)
    print("mock server on http://127.0.0.1:%d/chat/completions" % args.port)
    sys.stdout.flush()
    srv.serve_forever()
