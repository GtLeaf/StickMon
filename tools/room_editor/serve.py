#!/usr/bin/env python3
"""Serve the editor and a read-only view of current project resources."""
from __future__ import annotations

import argparse
import json
import webbrowser
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

try:
    from project_scene import ROOT, build_project_scene
except ModuleNotFoundError as error:
    if error.name == "PIL":
        raise SystemExit("Pillow is required. Install it with: python3 -m pip install pillow") from error
    raise

TOOL_DIR = Path(__file__).resolve().parent


class EditorHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, root=ROOT, layout=None, furniture_dir=None, **kwargs):
        self.project_root = root
        self.layout = layout
        self.furniture_dir = furniture_dir
        super().__init__(*args, directory=str(TOOL_DIR), **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def list_directory(self, path):
        self.send_error(404)
        return None

    def do_GET(self):
        host = self.headers.get("Host", "").split(":", 1)[0]
        if host not in ("localhost", "127.0.0.1"):
            self.send_error(403)
            return
        if urlsplit(self.path).path == "/api/project-scene":
            try:
                data = build_project_scene(self.project_root, self.layout, self.furniture_dir)
                payload = json.dumps(data, ensure_ascii=False).encode("utf-8")
                status = 200
            except (OSError, ValueError, RuntimeError) as error:
                self.log_error("Project scene: %s", error)
                payload = json.dumps({"error": str(error)}, ensure_ascii=False).encode("utf-8")
                status = 422
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        target = Path(self.translate_path(self.path)).resolve()
        if not target.is_relative_to(TOOL_DIR.resolve()) or target.suffix in (".py", ".pyc"):
            self.send_error(404)
            return
        super().do_GET()


def create_server(port=8765, root=ROOT, layout=None, furniture_dir=None):
    handler = partial(EditorHandler, root=root, layout=layout, furniture_dir=furniture_dir)
    return ThreadingHTTPServer(("127.0.0.1", port), handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--layout", type=Path, help="Editable room source override, as in prepare_room_background.py")
    parser.add_argument("--furniture-dir", type=Path)
    parser.add_argument("--no-open", action="store_true", help="Do not open the browser automatically")
    args = parser.parse_args()
    try:
        server = create_server(args.port, layout=args.layout, furniture_dir=args.furniture_dir)
    except OSError as error:
        parser.exit(1, f"Cannot start room editor: {error}\nTry a different --port.\n")
    url = f"http://127.0.0.1:{server.server_port}/"
    print(f"StickMon room editor: {url}", flush=True)
    print("Reads active project resources on load/reload. Press Ctrl+C to stop.", flush=True)
    if not args.no_open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
