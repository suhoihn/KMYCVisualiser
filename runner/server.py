"""Private, fixed-policy job dispatcher. Only this service accesses Docker."""
import base64
import collections
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import socket
import signal
import subprocess
import threading
import time

from job import validate

active = threading.Semaphore(1)
capacity = threading.BoundedSemaphore(5)
rate_lock = threading.Lock()
requests = collections.OrderedDict()
container_lock = threading.Lock()
owned_containers = set()
MAX_BYTES = 8 * 1024 * 1024


class DockerConnection(http.client.HTTPConnection):
    def __init__(self):
        super().__init__("localhost", timeout=45)

    def connect(self):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(45)
        self.sock.connect("/var/run/docker.sock")


def docker(method, path, body=None):
    connection = DockerConnection()
    connection.request(method, path, json.dumps(body).encode() if body is not None else None,
                       {"Content-Type": "application/json"})
    response = connection.getresponse()
    data = response.read()
    connection.close()
    if response.status >= 300:
        raise RuntimeError("Sandbox engine: " + data.decode(errors="replace")[:300])
    return json.loads(data) if data else {}


def exact(stream, count):
    data = bytearray()
    while len(data) < count:
        chunk = stream.read(count - len(data))
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # Source code and program output never go into access logs.

    def do_GET(self):
        if self.path != "/health":
            self.send_error(404)
            return
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'{"status":"ok"}')

    def do_POST(self):
        if self.path != "/jobs":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", "0"))
        if not 0 < length <= 131072:
            self.send_error(413)
            return
        try:
            payload = json.loads(self.rfile.read(length))
            validate(payload)
        except (ValueError, TypeError) as error:
            self.send_error(400, str(error))
            return
        with rate_lock:
            key = self.headers.get("X-Client-IP", self.client_address[0])[:100]
            now = time.monotonic()
            history = requests.pop(key, [])
            history = [stamp for stamp in history if now - stamp < 60]
            if len(history) >= 10:
                self.send_error(429, "Submission limit reached; wait a minute")
                return
            requests[key] = history + [now]
            while len(requests) > 1024:
                requests.popitem(last=False)
        if not capacity.acquire(blocking=False):
            self.send_error(429, "The playground queue is full")
            return
        acquired = False
        self.send_response(200)
        self.send_header("Content-Type", "application/x-ndjson; charset=utf-8")
        self.send_header("Connection", "close")
        self.send_header("X-Accel-Buffering", "no")
        self.end_headers()
        self.close_connection = True
        self.bytes_sent = 0
        try:
            self.send_event({"type": "phase", "phase": "queued"})
            acquired = active.acquire(timeout=20)
            if not acquired:
                raise RuntimeError("Queue wait exceeded 20 seconds; try again")
            if os.environ.get("KMY_RUNNER_MODE", "docker") == "local":
                self.local(payload)
            else:
                self.sandbox(payload)
        except (BrokenPipeError, ConnectionResetError):
            pass
        except Exception as error:
            try:
                self.send_event({"type": "error", "message": str(error)[:1000]})
            except OSError:
                pass
        finally:
            if acquired:
                active.release()
            capacity.release()

    def send_event(self, event):
        self.send_data((json.dumps(event) + "\n").encode())

    def send_data(self, data):
        self.bytes_sent += len(data)
        if self.bytes_sent > MAX_BYTES:
            raise RuntimeError("Recording exceeded 8 MiB; use a smaller project")
        self.wfile.write(data)
        self.wfile.flush()

    def local(self, payload):
        # Explicit development mode, bound to loopback. Never enabled in Compose.
        script = Path(__file__).with_name("job.py")
        child = subprocess.Popen(["python3", "-u", str(script), "--stdin"], stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, env=os.environ.copy(), start_new_session=True)
        watchdog = threading.Timer(45, child.kill)
        watchdog.start()
        try:
            child.stdin.write(json.dumps(payload).encode())
            child.stdin.close()
            for line in child.stdout:
                self.send_data(line)
            child.wait()
            if child.returncode < 0:
                raise RuntimeError("Job exceeded the overall time limit")
        finally:
            watchdog.cancel()
            if child.poll() is None:
                child.kill()
            child.wait()

    def sandbox(self, payload):
        encoded = base64.b64encode(json.dumps(payload, ensure_ascii=False).encode()).decode()
        env = ["KMY_PAYLOAD_" + str(index // 32000) + "=" + encoded[index:index + 32000]
               for index in range(0, len(encoded), 32000)]
        env.extend(f"{name}={os.environ[name]}" for name in ("KMY_HEAP_BYTES", "KMY_STACK_BYTES", "KMY_EXECUTION_MODE")
                   if name in os.environ)
        configuration = {
            "Image": os.environ.get("KMY_JOB_IMAGE", "kmy-job:local"),
            "Env": env, "User": "65532:65532", "NetworkDisabled": True,
            "HostConfig": {
                "NetworkMode": "none", "Memory": 268435456, "MemorySwap": 268435456,
                "NanoCpus": 750000000, "PidsLimit": 32, "ReadonlyRootfs": True,
                "CapDrop": ["ALL"], "SecurityOpt": ["no-new-privileges:true"],
                "Tmpfs": {"/work": "rw,exec,nosuid,nodev,size=32m,mode=1777", "/tmp": "rw,noexec,nosuid,nodev,size=16m,mode=1777"},
                "LogConfig": {"Type": "json-file", "Config": {"max-size": "10m", "max-file": "1"}},
                "Ulimits": [{"Name": "core", "Soft": 0, "Hard": 0}, {"Name": "nofile", "Soft": 64, "Hard": 64}],
            },
        }
        runtime = os.environ.get("KMY_SANDBOX_RUNTIME", "")
        if runtime:
            configuration["HostConfig"]["Runtime"] = runtime
        identity = docker("POST", "/containers/create", configuration)["Id"]
        with container_lock:
            owned_containers.add(identity)
        def kill():
            try:
                docker("POST", "/containers/" + identity + "/kill")
            except Exception:
                pass
        watchdog = threading.Timer(45, kill)
        connection = None
        try:
            docker("POST", "/containers/" + identity + "/start")
            watchdog.start()
            connection = DockerConnection()
            connection.request("GET", "/containers/" + identity + "/logs?follow=1&stdout=1&stderr=0")
            response = connection.getresponse()
            if response.status != 200:
                raise RuntimeError("Could not read sandbox results")
            while True:
                header = exact(response, 8)
                if not header:
                    break
                if len(header) != 8:
                    raise RuntimeError("Sandbox log stream ended early")
                count = int.from_bytes(header[4:8], "big")
                if count > MAX_BYTES:
                    raise RuntimeError("Sandbox output limit exceeded")
                self.send_data(exact(response, count))
            state = docker("GET", "/containers/" + identity + "/json")["State"]
            if state.get("OOMKilled"):
                raise RuntimeError("Job exceeded its 256 MiB memory limit")
            if state.get("ExitCode") not in (0, 1):
                raise RuntimeError("Sandbox stopped unexpectedly or exceeded its time limit")
        finally:
            watchdog.cancel()
            if connection:
                connection.close()
            try:
                docker("DELETE", "/containers/" + identity + "?force=true")
            except Exception:
                pass
            with container_lock:
                owned_containers.discard(identity)


if __name__ == "__main__":
    def stop(_signum, _frame):
        with container_lock:
            identities = list(owned_containers)
        for identity in identities:
            try:
                docker("DELETE", "/containers/" + identity + "?force=true")
            except Exception:
                pass
        raise SystemExit(0)
    signal.signal(signal.SIGTERM, stop)
    local = os.environ.get("KMY_RUNNER_MODE") == "local"
    address = "127.0.0.1" if local else "0.0.0.0"
    ThreadingHTTPServer((address, int(os.environ.get("PORT", "8090"))), Handler).serve_forever()
