"""从已选版本生成带 wheel 校验值的依赖锁，不自动升级依赖。"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]

def locked_package(item):
    name, version = item
    url = f"https://pypi.org/pypi/{name}/{version}/json"
    with urllib.request.urlopen(url, timeout=45) as response:
        release = json.load(response)
    wheels = [file for file in release["urls"] if file["filename"].endswith(".whl") and not file["yanked"]]
    if not wheels:
        raise ValueError(f"{name}=={version} 没有可用 wheel")
    hashes = sorted({file["digests"]["sha256"] for file in wheels})
    return f"# {url}\n{name}=={version} \\\n" + " \\\n".join(f"    --hash=sha256:{value}" for value in hashes)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", action="append", choices=["host", "ci", "browser"])
    args = parser.parse_args()
    config = json.loads((ROOT / "config/python-dependencies.json").read_text(encoding="utf-8"))
    for name in args.profile or config["profiles"]:
        profile = config["profiles"][name]
        with ThreadPoolExecutor(max_workers=6) as pool:
            entries = list(pool.map(locked_package, sorted(profile["packages"].items())))
        metadata = {"python": config["python"], "imports": profile["imports"]}
        text = "# 由 tools/lock-python.py 生成；版本选择见 config/python-dependencies.json。\n"
        text += "# environment: " + json.dumps(metadata, separators=(",", ":")) + "\n"
        text += "--require-hashes\n--only-binary=:all:\n\n" + "\n\n".join(entries) + "\n"
        target = (ROOT / profile["output"]).resolve()
        if not target.is_relative_to(ROOT):
            raise ValueError("依赖锁输出超出工程根目录")
        target.write_text(text, encoding="utf-8", newline="\n")
        print(f"{name}: {profile['output']} ({len(entries)} packages)", flush=True)

if __name__ == "__main__":
    main()
