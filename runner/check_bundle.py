"""Verify shipped archives on Linux, including native execution after extraction."""
import io
import json
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
import tempfile
import zipfile

release = Path(__file__).resolve().parents[1] / "release"
with tempfile.TemporaryDirectory(prefix="kmy-release-check-") as directory:
    root = Path(directory)
    with tarfile.open(release / "kmyc-linux-x86_64.tar.gz") as archive:
        for member in archive.getmembers():
            path = PurePosixPath(member.name)
            assert not path.is_absolute() and ".." not in path.parts
            assert not member.issym() and not member.islnk()
        archive.extractall(root)
    compiler = root / "kmyc"
    compiler.chmod(0o755)
    assert compiler.read_bytes()[:4] == b"\x7fELF"
    source = root / "smoke.kmy"
    source.write_text("print(1 + 2);", encoding="utf-8")
    analysis = subprocess.run([str(compiler), str(source), "--frontend-json"], cwd=root,
                              text=True, capture_output=True, timeout=10, check=True)
    assert json.loads(analysis.stdout)["schemaVersion"] == 2
    executable = root / "program"
    subprocess.run([str(compiler), str(source), "--linux", "-o", str(executable)], cwd=root,
                   capture_output=True, timeout=15, check=True)
    executed = subprocess.run([str(executable)], text=True, capture_output=True, timeout=2, check=True)
    assert executed.stdout == "3\n"
    with tarfile.open(release / "kmy-playground-linux-x86_64.tar.gz") as archive:
        names = set(archive.getnames())
        for path in ("compose.yaml", "runner/server.py", "runner/job.py", "runner/heap_limit.c", "deploy/README.md",
                     "frontend/dist/index.html", "release/linux-x86_64/kmyc"):
            assert path in names, path
        assert archive.extractfile("release/linux-x86_64/kmyc").read() == compiler.read_bytes()
        project = release.parent
        for path in ("runner/job.py", "runner/server.py", "runner/heap_limit.c", "compose.yaml"):
            assert archive.extractfile(path).read() == (project / path).read_bytes(), "Stale packaged file: " + path
        jar = archive.extractfile("backend/build/libs/kmy-visualiser-backend-0.1.0.jar").read()
        with zipfile.ZipFile(io.BytesIO(jar)) as boot:
            bytecode = boot.read("BOOT-INF/classes/dev/kmy/visualiser/analysis/PlaygroundController.class")
            assert int.from_bytes(bytecode[6:8], "big") == 61, "The shipped backend must target Java 17"
print("PASS extracted Linux compiler: ELF, schema v2 recording, native stdout=3")
print("PASS upload bundle: frontend, Java 17 backend, runner, deployment files, and identical compiler")
