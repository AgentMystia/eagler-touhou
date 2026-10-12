"""读取当前 iframe 文档，适用于 location.replace 导航。"""
from pathlib import Path


def runtime_document_observation_script() -> str:
    return Path(__file__).with_name("runtime-document-observation.js").read_text(encoding="utf-8")


def install_runtime_document_observation(page) -> None:
    page.add_init_script(runtime_document_observation_script())
