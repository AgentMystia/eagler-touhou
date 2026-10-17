"""读取工具解释器、锁定依赖和私有环境的实际文件身份。"""
from __future__ import annotations
import hashlib
import importlib
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import re
import sys
import sysconfig

def identity():
    return {
        "implementation": sys.implementation.name,
        "version": list(sys.version_info[:3]),
        "build": sys.version,
        "abi": sysconfig.get_config_var("SOABI"),
        "platform": sysconfig.get_platform(),
        "machine": platform.machine(),
        "baseExecutable": str(Path(getattr(sys, "_base_executable", sys.executable)).resolve()),
    }

def specification(lock):
    source = lock.read_text(encoding="utf-8")
    marker = "# environment: "
    metadata = next(json.loads(line[len(marker):]) for line in source.splitlines() if line.startswith(marker))
    expected = {}
    for line in source.splitlines():
        match = re.match(r"^([A-Za-z0-9_.-]+)==([^\s\\]+)", line)
        if match:
            name = re.sub(r"[-_.]+", "-", match[1]).lower()
            if name in expected:
                raise ValueError("duplicate dependency: " + name)
            expected[name] = match[2]
    if not expected:
        raise ValueError("empty dependency lock")
    return metadata, expected

def snapshot(lock):
    metadata, expected = specification(lock)
    runtime = identity()
    version = runtime["version"][:2]
    required = metadata["python"]
    if runtime["implementation"] != "cpython" or not required["min"] <= version < required["maxExclusive"]:
        raise ValueError("unsupported Python interpreter")
    if sys.prefix == sys.base_prefix:
        raise ValueError("dependency verification requires a private virtual environment")
    actual = {
        re.sub(r"[-_.]+", "-", distribution.metadata["Name"]).lower(): distribution.version
        for distribution in importlib.metadata.distributions()
    }
    if expected != actual:
        raise ValueError("installed distributions differ from the dependency lock: " + json.dumps(actual, sort_keys=True))
    for module in metadata["imports"]:
        importlib.import_module(module)
    prefix = Path(sys.prefix)
    hashes = []
    for directory, dirs, files in os.walk(prefix, followlinks=False):
        dirs[:] = sorted(name for name in dirs if name != "__pycache__")
        for name in sorted(files):
            path = Path(directory) / name
            relative = path.relative_to(prefix).as_posix()
            if relative == "environment.json" or name.endswith((".pyc", ".pyo")):
                continue
            with path.open("rb") as file:
                hashes.append([relative, hashlib.file_digest(file, "sha256").hexdigest()])
    fingerprint = hashlib.sha256(json.dumps(hashes, separators=(",", ":")).encode()).hexdigest()
    return {"runtime": runtime, "packages": actual, "files": fingerprint, "fileCount": len(hashes)}

def main():
    mode = sys.argv[1]
    if mode == "identity":
        print(json.dumps(identity()))
        return
    lock = Path(sys.argv[2])
    result = snapshot(lock)
    if mode == "verify":
        saved = json.loads(Path(sys.argv[3]).read_text(encoding="utf-8"))
        if result != saved:
            raise ValueError("private Python environment changed after installation")
    elif mode != "snapshot":
        raise ValueError("unknown environment inspection command")
    print(json.dumps(result, sort_keys=True))

if __name__ == "__main__":
    main()
