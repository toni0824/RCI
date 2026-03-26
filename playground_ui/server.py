#!/usr/bin/env python3
import json
import os
import shutil
import signal
import subprocess
import threading
import time
import uuid
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parent
PROJ_ROOT = ROOT.parent
STATIC_DIR = ROOT / "static"
RUNTIME_DIR = ROOT / "runtime"
LOG_DIR = RUNTIME_DIR / "logs"
CAPTURE_DIR = RUNTIME_DIR / "captures"
LOG_DIR.mkdir(parents=True, exist_ok=True)
CAPTURE_DIR.mkdir(parents=True, exist_ok=True)

SCENARIOS = {
    "local_direct": {
        "title": "Local Direct",
        "script": PROJ_ROOT / "runs/02_direct/local_direct_mac.sh",
        "args": [
            {"name": "ip", "label": "IP", "default": "127.0.0.1"},
        ],
        "kind": "local",
    },
    "local_intermediate": {
        "title": "Local Intermediate",
        "script": PROJ_ROOT / "runs/03_intermediate/local_intermediate_mac.sh",
        "args": [
            {"name": "ip", "label": "IP", "default": "127.0.0.1"},
        ],
        "kind": "local",
    },
    "local_message": {
        "title": "Local Message",
        "script": PROJ_ROOT / "runs/04_message/local_message_mac.sh",
        "args": [
            {"name": "ip", "label": "IP", "default": "127.0.0.1"},
        ],
        "kind": "local",
    },
    "local_final": {
        "title": "Local Final",
        "script": PROJ_ROOT / "runs/05_final/local_final_mac.sh",
        "args": [
            {"name": "ip", "label": "IP", "default": "127.0.0.1"},
        ],
        "kind": "local",
    },
    "tejo_basic": {
        "title": "Tejo Basic",
        "script": PROJ_ROOT / "runs/01_basic_tejo/tejo_basic_mac.sh",
        "args": [
            {"name": "local_ip", "label": "Local IP", "default": "10.19.233.157"},
            {"name": "local_tcp", "label": "Local TCP", "default": "58001"},
            {"name": "reg_udp", "label": "regUDP", "default": "58861"},
            {"name": "resident_id", "label": "Resident ID", "default": "10"},
            {"name": "resident_udp", "label": "Resident UDP", "default": "58862"},
            {"name": "group_id", "label": "Group", "default": "106"},
            {"name": "local_id", "label": "Local ID", "default": "01"},
            {"name": "session_code", "label": "Session", "default": "2600408"},
        ],
        "kind": "tejo",
    },
    "tejo_intermediate": {
        "title": "Tejo Intermediate",
        "script": PROJ_ROOT / "runs/03_intermediate/tejo_intermediate_mac.sh",
        "args": [
            {"name": "local_ip", "label": "Local IP", "default": "10.19.233.157"},
            {"name": "local_tcp", "label": "Local TCP", "default": "58001"},
            {"name": "reg_udp", "label": "regUDP", "default": "58861"},
            {"name": "resident_id", "label": "Resident ID", "default": "10"},
            {"name": "resident_udp", "label": "Resident UDP", "default": "58862"},
            {"name": "group_id", "label": "Group", "default": "106"},
            {"name": "local_id", "label": "Local ID", "default": "01"},
            {"name": "session_code", "label": "Session", "default": "2600408"},
        ],
        "kind": "tejo",
    },
    "tejo_message": {
        "title": "Tejo Message",
        "script": PROJ_ROOT / "runs/04_message/tejo_message_mac.sh",
        "args": [
            {"name": "local_ip", "label": "Local IP", "default": "10.19.233.157"},
            {"name": "local_tcp", "label": "Local TCP", "default": "58001"},
            {"name": "reg_udp", "label": "regUDP", "default": "58861"},
            {"name": "resident_id", "label": "Resident ID", "default": "10"},
            {"name": "resident_udp", "label": "Resident UDP", "default": "58862"},
            {"name": "group_id", "label": "Group", "default": "106"},
            {"name": "local_id", "label": "Local ID", "default": "01"},
            {"name": "session_code", "label": "Session", "default": "2600408"},
        ],
        "kind": "tejo",
    },
    "tejo_final": {
        "title": "Tejo Final",
        "script": PROJ_ROOT / "runs/05_final/tejo_final_mac.sh",
        "args": [
            {"name": "local_ip", "label": "Local IP", "default": "10.19.233.157"},
            {"name": "local_tcp", "label": "Local TCP", "default": "58004"},
            {"name": "reg_udp", "label": "regUDP", "default": "58861"},
            {"name": "group_id", "label": "Group", "default": "106"},
            {"name": "local_id", "label": "Local ID", "default": "40"},
            {"name": "id10", "label": "ID 10", "default": "10"},
            {"name": "udp10", "label": "UDP 10", "default": "58862"},
            {"name": "id20", "label": "ID 20", "default": "20"},
            {"name": "udp20", "label": "UDP 20", "default": "58863"},
            {"name": "id30", "label": "ID 30", "default": "30"},
            {"name": "udp30", "label": "UDP 30", "default": "58864"},
            {"name": "session_code", "label": "Session", "default": "2600408"},
        ],
        "kind": "tejo",
    },
}

RUNS = {}
RUN_LOCK = threading.Lock()
CAPTURE = {
    "proc": None,
    "id": None,
    "path": None,
    "tool": None,
    "filter": None,
    "started_at": None,
}


def json_bytes(payload):
    return json.dumps(payload, indent=2).encode("utf-8")


def detect_capture_tool():
    for tool in ("tshark", "tcpdump"):
        path = shutil.which(tool)
        if path:
            return tool, path
    return None, None


def run_to_dict(run_id, meta):
    proc = meta["proc"]
    return {
        "id": run_id,
        "scenario": meta["scenario"],
        "title": SCENARIOS[meta["scenario"]]["title"],
        "cmd": meta["cmd"],
        "log": str(meta["log"].relative_to(PROJ_ROOT)),
        "started_at": meta["started_at"],
        "ended_at": meta.get("ended_at"),
        "returncode": proc.poll(),
        "running": proc.poll() is None,
    }


def cleanup_finished_runs():
    with RUN_LOCK:
        for meta in RUNS.values():
            proc = meta["proc"]
            if proc.poll() is not None and meta.get("ended_at") is None:
                meta["ended_at"] = time.time()


def start_run(scenario_id, values):
    scenario = SCENARIOS[scenario_id]
    script = scenario["script"]
    if not script.exists():
        raise FileNotFoundError(f"script not found: {script}")

    args = []
    for field in scenario["args"]:
        value = str(values.get(field["name"], field.get("default", ""))).strip()
        if not value:
            raise ValueError(f"missing argument: {field['name']}")
        args.append(value)

    run_id = uuid.uuid4().hex[:10]
    log_path = LOG_DIR / f"{run_id}.log"
    log_fh = open(log_path, "w", encoding="utf-8")
    cmd = [str(script), *args]
    proc = subprocess.Popen(
        cmd,
        cwd=PROJ_ROOT,
        stdout=log_fh,
        stderr=subprocess.STDOUT,
        text=True,
        start_new_session=True,
    )
    meta = {
        "scenario": scenario_id,
        "proc": proc,
        "cmd": cmd,
        "log": log_path,
        "started_at": time.time(),
    }
    with RUN_LOCK:
        RUNS[run_id] = meta
    return run_id, meta


def stop_run(run_id):
    with RUN_LOCK:
        meta = RUNS.get(run_id)
    if not meta:
        return False
    proc = meta["proc"]
    if proc.poll() is not None:
        return True
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
    except ProcessLookupError:
        return True
    return True


def start_capture(payload):
    if CAPTURE["proc"] and CAPTURE["proc"].poll() is None:
        raise RuntimeError("capture already running")
    tool, tool_path = detect_capture_tool()
    if not tool:
        raise RuntimeError("no capture tool found (need tshark or tcpdump)")

    capture_id = uuid.uuid4().hex[:10]
    pcap_path = CAPTURE_DIR / f"{capture_id}.pcap"
    filt = str(payload.get("filter", "tcp or udp")).strip() or "tcp or udp"
    iface = str(payload.get("interface", "any")).strip() or "any"

    if tool == "tshark":
        cmd = [tool_path, "-i", iface, "-w", str(pcap_path), "-f", filt]
    else:
        cmd = [tool_path, "-i", iface, "-w", str(pcap_path), filt]

    proc = subprocess.Popen(
        cmd,
        cwd=PROJ_ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    CAPTURE.update(
        {
            "proc": proc,
            "id": capture_id,
            "path": pcap_path,
            "tool": tool,
            "filter": filt,
            "started_at": time.time(),
            "interface": iface,
        }
    )
    return {
        "id": capture_id,
        "tool": tool,
        "path": str(pcap_path.relative_to(PROJ_ROOT)),
        "filter": filt,
        "interface": iface,
        "running": True,
    }


def stop_capture():
    proc = CAPTURE.get("proc")
    if not proc:
        return False
    if proc.poll() is None:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except ProcessLookupError:
            pass
    CAPTURE["proc"] = None
    return True


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(STATIC_DIR), **kwargs)

    def log_message(self, fmt, *args):
        return

    def send_json(self, payload, status=HTTPStatus.OK):
        body = json_bytes(payload)
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def read_json(self):
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length else b"{}"
        return json.loads(raw.decode("utf-8"))

    def do_GET(self):
        cleanup_finished_runs()
        parsed = urlparse(self.path)
        path = parsed.path
        if path == "/api/health":
            tool, _ = detect_capture_tool()
            self.send_json(
                {
                    "ok": True,
                    "project_root": str(PROJ_ROOT),
                    "capture_tool": tool,
                }
            )
            return
        if path == "/api/scenarios":
            payload = []
            for sid, meta in SCENARIOS.items():
                payload.append(
                    {
                        "id": sid,
                        "title": meta["title"],
                        "kind": meta["kind"],
                        "script": str(meta["script"].relative_to(PROJ_ROOT)),
                        "args": meta["args"],
                    }
                )
            self.send_json(payload)
            return
        if path == "/api/runs":
            with RUN_LOCK:
                payload = [run_to_dict(run_id, meta) for run_id, meta in sorted(RUNS.items(), reverse=True)]
            self.send_json(payload)
            return
        if path.startswith("/api/runs/") and path.endswith("/log"):
            run_id = path.split("/")[3]
            with RUN_LOCK:
                meta = RUNS.get(run_id)
            if not meta:
                self.send_json({"error": "run not found"}, status=HTTPStatus.NOT_FOUND)
                return
            try:
                text = meta["log"].read_text(encoding="utf-8", errors="replace")
            except FileNotFoundError:
                text = ""
            self.send_json({"id": run_id, "log": text})
            return
        if path == "/api/capture":
            proc = CAPTURE.get("proc")
            running = bool(proc and proc.poll() is None)
            self.send_json(
                {
                    "running": running,
                    "id": CAPTURE.get("id"),
                    "tool": CAPTURE.get("tool"),
                    "path": str(CAPTURE["path"].relative_to(PROJ_ROOT)) if CAPTURE.get("path") else None,
                    "filter": CAPTURE.get("filter"),
                    "interface": CAPTURE.get("interface"),
                    "started_at": CAPTURE.get("started_at"),
                }
            )
            return
        if path == "/api/files":
            files = []
            for p in sorted(CAPTURE_DIR.glob("*.pcap")):
                files.append({"name": p.name, "path": str(p.relative_to(PROJ_ROOT)), "size": p.stat().st_size})
            for p in sorted(LOG_DIR.glob("*.log")):
                files.append({"name": p.name, "path": str(p.relative_to(PROJ_ROOT)), "size": p.stat().st_size})
            self.send_json(files)
            return
        return super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path
        if path == "/api/run":
            try:
                payload = self.read_json()
                scenario_id = payload["scenario"]
                if scenario_id not in SCENARIOS:
                    raise KeyError("unknown scenario")
                run_id, meta = start_run(scenario_id, payload.get("args", {}))
                self.send_json(run_to_dict(run_id, meta), status=HTTPStatus.CREATED)
            except Exception as exc:
                self.send_json({"error": str(exc)}, status=HTTPStatus.BAD_REQUEST)
            return
        if path.startswith("/api/runs/") and path.endswith("/stop"):
            run_id = path.split("/")[3]
            if stop_run(run_id):
                self.send_json({"ok": True})
            else:
                self.send_json({"error": "run not found"}, status=HTTPStatus.NOT_FOUND)
            return
        if path == "/api/capture/start":
            try:
                payload = self.read_json()
                self.send_json(start_capture(payload), status=HTTPStatus.CREATED)
            except Exception as exc:
                self.send_json({"error": str(exc)}, status=HTTPStatus.BAD_REQUEST)
            return
        if path == "/api/capture/stop":
            stop_capture()
            self.send_json({"ok": True})
            return
        self.send_json({"error": "not found"}, status=HTTPStatus.NOT_FOUND)


def main():
    host = os.environ.get("PLAYGROUND_HOST", "127.0.0.1")
    port = int(os.environ.get("PLAYGROUND_PORT", "8787"))
    httpd = ThreadingHTTPServer((host, port), Handler)
    print(f"Playground UI on http://{host}:{port}")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stop_capture()
        httpd.server_close()


if __name__ == "__main__":
    main()
