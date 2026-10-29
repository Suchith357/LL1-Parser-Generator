#!/usr/bin/env python3
"""
web/server.py - ParseLab adapter (Python 3 standard library only).

One process does everything:
  * serves the static frontend from ./web  (ES modules, no build step)
  * POST /api/analyze    runs the C engine (ll1json) and returns its
                         JSON document
  * GET  /api/health     REALLY runs the engine on a built-in sample
                         before reporting the engine as connected

Security / robustness:
  * engine invoked with an argv ARRAY (never a shell string)
  * grammar written to a random temp file, always cleaned up (finally)
  * limits: grammar 64 KB, input 4 KB, 10 s engine timeout
  * binds to 127.0.0.1 by default (override host/port via env, see
    web/env.example), never opens the host network by default
  * structured JSON errors with stable codes

Environment variables (all optional, defaults shown):
  PARSELAB_HOST=127.0.0.1
  PARSELAB_PORT=8787
  PARSELAB_ENGINE=./ll1json(.exe auto-detected)
"""

import json
import os
import subprocess
import sys
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.abspath(__file__))   # web/
REPO = os.path.dirname(ROOT)

MAX_GRAMMAR_BYTES = 64 * 1024
MAX_INPUT_CHARS = 4096
ENGINE_TIMEOUT_S = 10

HOST = os.environ.get("PARSELAB_HOST", "127.0.0.1")
PORT = int(os.environ.get("PARSELAB_PORT", "8787"))


def engine_path():
    """ParseLab engine binary location (override with PARSELAB_ENGINE)."""
    env = os.environ.get("PARSELAB_ENGINE")
    if env:
        return env
    exe = ".exe" if os.name == "nt" else ""
    # Prefer repo root, then web/ (Windows-friendly layouts).
    for cand in (os.path.join(REPO, "ll1json" + exe),
                 os.path.join(ROOT, "ll1json" + exe)):
        if os.path.isfile(cand):
            return cand
    return os.path.join(REPO, "ll1json" + exe)


ENGINE_HELP = ("engine not built. Build it with:\n"
               "  make ll1json            # Linux/macOS with GNU make\n"
               "  gcc -std=c99 -Wall -Wextra -O2 -Iinclude "
               "src/grammar.c src/first.c src/follow.c src/parsing_table.c "
               "src/parser.c src/translation.c src/utils.c "
               "src/json_export.c tools/json_main.c -o ll1json\n"
               "(on Windows add .exe and .exe output name)")


# ----------------------------------------------------------------------
# Engine invocation
# ----------------------------------------------------------------------

class EngineResult:
    __slots__ = ("ok", "http_status", "payload")

    def __init__(self, ok, http_status, payload):
        self.ok = ok
        self.http_status = http_status
        self.payload = payload


def run_engine(grammar_text, input_text):
    """Runs ll1json on <grammar> with <input>; returns EngineResult."""
    engine = engine_path()
    if not os.path.isfile(engine):
        return EngineResult(False, 503, {
            "ok": False, "error": {"code": "ENGINE_NOT_BUILT",
                                   "message": ENGINE_HELP}})

    tmpdir = None
    try:
        tmpdir = tempfile.mkdtemp(prefix="parselab_")
        gram_path = os.path.join(tmpdir, "grammar.txt")
        with open(gram_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(grammar_text)

        argv = [os.path.abspath(engine), os.path.abspath(gram_path),
                input_text]
        try:
            proc = subprocess.run(argv, capture_output=True, text=True,
                                  timeout=ENGINE_TIMEOUT_S, cwd=REPO)
            out, ret = proc.stdout, proc.returncode
        except subprocess.TimeoutExpired:
            return EngineResult(False, 504, {
                "ok": False, "error": {"code": "ENGINE_TIMEOUT",
                                       "message":
                                       "engine did not finish within %d s"
                                       % ENGINE_TIMEOUT_S}})

        if not out.strip():
            return EngineResult(False, 500, {
                "ok": False, "error": {"code": "ENGINE_EMPTY_OUTPUT",
                                       "message": "engine produced no output"}})

        try:
            doc = json.loads(out)
        except json.JSONDecodeError:
            return EngineResult(False, 500, {
                "ok": False, "error": {"code": "ENGINE_BAD_OUTPUT",
                                       "message":
                                       "engine output was not valid JSON"}})

        if not isinstance(doc, dict):
            return EngineResult(False, 500, {
                "ok": False, "error": {"code": "ENGINE_BAD_OUTPUT",
                                       "message":
                                       "engine output was not a JSON object"}})

        if doc.get("ok") is not True:
            stage = doc.get("stage", "engine")
            message = doc.get("message") or doc.get("grammarError") or "engine failed"
            # Grammar validation failures are the USER's data problem: 200
            # with ok:false so the UI shows the engine's own diagnostics.
            return EngineResult(True, 200, {
                "ok": False,
                "stage": stage,
                "grammarError": message if stage == "grammar" else None,
                "error": None if stage == "grammar" else {
                    "code": "ENGINE_FAILED",
                    "message": message},
            })

        return EngineResult(True, 200, doc)

    finally:
        if tmpdir:
            for name in os.listdir(tmpdir):
                try:
                    os.unlink(os.path.join(tmpdir, name))
                except OSError:
                    pass
            try:
                os.rmdir(tmpdir)
            except OSError:
                pass


# Built-in sample used by /api/health: must succeed for "connected".
HEALTH_GRAMMAR = "E -> T Ep\nEp -> + T Ep | #\nT -> F Tp\nTp -> * F Tp | #\n" \
                 "F -> ( E ) | id\n"


def health_payload():
    engine = engine_path()
    if not os.path.isfile(engine):
        return 503, {"ok": True, "adapter": True, "engine": False,
                     "error": {"code": "ENGINE_NOT_BUILT",
                               "message": ENGINE_HELP}}
    res = run_engine(HEALTH_GRAMMAR, "id + id * id")
    if res.ok and res.payload.get("ok") and res.payload.get("parse", {}).get(
            "status") == "accepted":
        return 200, {"ok": True, "adapter": True, "engine": True,
                     "engineBinary": os.path.basename(engine),
                     "grammar": {"nonTerminals":
                                 res.payload["grammar"]["nonTerminals"],
                                 "terminals":
                                 res.payload["grammar"]["terminals"]}}
    return 503, {"ok": True, "adapter": True, "engine": False,
                 "error": {"code": "ENGINE_UNHEALTHY",
                           "message": "the engine binary ran but its "
                                      "self-check failed"}}


# ----------------------------------------------------------------------
# Static files
# ----------------------------------------------------------------------

MIME = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".mjs": "text/javascript; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".svg": "image/svg+xml",
    ".woff2": "font/woff2",
    ".woff": "font/woff",
    ".ico": "image/x-icon",
}


def serve_static(handler, url_path):
    """Serves web/ with a safe, containment-checked join."""
    rel = url_path.lstrip("/")
    if rel in ("", "/"):
        rel = "index.html"
    base = os.path.abspath(ROOT)
    target = os.path.abspath(os.path.join(base, rel))
    if not target.startswith(base + os.sep) and target != base:
        return 403, b"forbidden"
    if os.path.isdir(target):
        target = os.path.join(target, "index.html")
    if not os.path.isfile(target):
        return 404, b"not found"
    ext = os.path.splitext(target)[1].lower()
    ctype = MIME.get(ext, "application/octet-stream")
    try:
        with open(target, "rb") as f:
            body = f.read()
    except OSError:
        return 500, b"read error"
    return 200, body, ctype


# ----------------------------------------------------------------------
# HTTP handler
# ----------------------------------------------------------------------

class ParseLabHandler(BaseHTTPRequestHandler):
    server_version = "ParseLabAdapter/1.0"

    def log_message(self, fmt, *args):  # quieter console
        sys.stderr.write("[%s] %s\n" % (self.command, fmt % args))

    # ---- helpers -----------------------------------------------------

    def send_json(self, status, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        # Restrict CORS: same-origin app, loopback adapter.
        self.send_header("Access-Control-Allow-Origin",
                         "http://127.0.0.1:%d http://localhost:%d"
                         % (PORT, PORT))
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()
        self.wfile.write(body)

    def read_json_body(self):
        length = int(self.headers.get("Content-Length") or 0)
        if length <= 0 or length > MAX_GRAMMAR_BYTES + 16 * 1024:
            return None
        raw = self.rfile.read(length)
        try:
            return json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return None

    def send_static(self, url_path):
        served = serve_static(self, url_path)
        if served is None:
            status, body = 404, b"not found"
            ctype = "text/plain; charset=utf-8"
        elif len(served) == 2:
            status, body = served
            ctype = "text/plain; charset=utf-8"
        else:
            status, body, ctype = served
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control",
                         "no-cache" if ctype.startswith("text/") else "max-age=3600")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    # ---- verbs -------------------------------------------------------

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin",
                         "http://127.0.0.1:%d http://localhost:%d"
                         % (PORT, PORT))
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path == "/api/health":
            status, payload = health_payload()
            self.send_json(status, payload)
            return
        self.send_static(path)

    def do_HEAD(self):
        self.do_GET()

    def do_POST(self):
        path = self.path.split("?", 1)[0]
        if path != "/api/analyze":
            self.send_json(404, {"ok": False,
                                 "error": {"code": "NOT_FOUND",
                                           "message": "unknown endpoint"}})
            return

        body = self.read_json_body()
        if not isinstance(body, dict):
            self.send_json(400, {"ok": False,
                                 "error": {"code": "BAD_REQUEST",
                                           "message": "body must be a JSON "
                                                      "object with 'grammar' "
                                                      "and optional 'input'"}})
            return

        grammar = body.get("grammar")
        user_input = body.get("input", "")
        if not isinstance(grammar, str) or not grammar.strip():
            self.send_json(400, {"ok": False,
                                 "error": {"code": "BAD_REQUEST",
                                           "message": "'grammar' must be a "
                                                      "non-empty string"}})
            return
        if not isinstance(user_input, str):
            self.send_json(400, {"ok": False,
                                 "error": {"code": "BAD_REQUEST",
                                           "message": "'input' must be a "
                                                      "string"}})
            return
        if len(grammar.encode("utf-8")) > MAX_GRAMMAR_BYTES:
            self.send_json(413, {"ok": False,
                                 "error": {"code": "GRAMMAR_TOO_LARGE",
                                           "message": "grammar exceeds %d KB"
                                                      % (MAX_GRAMMAR_BYTES //
                                                         1024)}})
            return
        if len(user_input) > MAX_INPUT_CHARS:
            self.send_json(413, {"ok": False,
                                 "error": {"code": "INPUT_TOO_LARGE",
                                           "message": "input exceeds %d "
                                                      "characters"
                                                      % MAX_INPUT_CHARS}})
            return

        res = run_engine(grammar, user_input)
        self.send_json(res.http_status, res.payload)


def main():
    server = ThreadingHTTPServer((HOST, PORT), ParseLabHandler)
    print("ParseLab adapter listening on http://%s:%d" % (HOST, PORT))
    print("Frontend:  http://%s:%d/" % (HOST, PORT))
    print("Health:    http://%s:%d/api/health" % (HOST, PORT))
    print("Engine:    %s" % engine_path())
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")


if __name__ == "__main__":
    main()
