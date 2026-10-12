import argparse
from pathlib import Path
import zipfile

parser = argparse.ArgumentParser(description="打包目录为支持大文件的 ZIP")
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--prefix", default="")
args = parser.parse_args()
source = args.source.resolve()
output = args.output.resolve()
if not source.is_dir() or output == source or source in output.parents:
    parser.error("输出必须位于输入目录之外")
prefix = args.prefix.strip("/")
if "\\" in prefix or ":" in prefix or (prefix and any(part in ("", ".", "..") for part in prefix.split("/"))):
    parser.error("无效的压缩包前缀")
with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED, allowZip64=True, compresslevel=6) as archive:
    for path in sorted(source.rglob("*")):
        if path.is_symlink():
            raise ValueError("压缩包不接受符号链接：" + str(path))
        if path.is_file() or (path.is_dir() and not any(path.iterdir())):
            name = path.relative_to(source).as_posix()
            archive.write(path, (prefix + "/" if prefix else "") + name)
with zipfile.ZipFile(output) as archive:
    bad = archive.testzip()
    if bad is not None:
        raise ValueError("ZIP 校验失败：" + bad)
