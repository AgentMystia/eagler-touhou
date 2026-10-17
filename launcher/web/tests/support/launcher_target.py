"""Select a built Launcher target without changing a browser test scenario.

Unset EAGLER_LAUNCHER_TEST_ROOT retains the original development server.
An explicit root uses the strict UI-only static fixture server, with no legacy
fallback or Service Worker generation. Supply real required assets separately.
"""
from __future__ import annotations

import os
import subprocess
from pathlib import Path


def launcher_test_root(project: Path) -> Path | None:
    target = os.environ.get("EAGLER_LAUNCHER_TEST_ROOT")
    if not target:
        return None
    root = Path(target)
    if not root.is_absolute():
        root = project / root
    root = root.resolve()
    if not (root / "index.html").is_file():
        raise FileNotFoundError(f"Launcher test target has no index.html: {root}")
    return root


def launcher_server_command(project: Path, port: int) -> list[str]:
    command = ["node", "scripts/serve.mjs", str(port)]
    root = launcher_test_root(project)
    return ["node", "tests/support/launcher-static-server.mjs", str(port), str(root)] if root is not None else command


def launcher_static_server_command(project: Path, port: int, original: list[str]) -> list[str]:
    """Retarget an existing HTTP fixture without changing its default command.

    The original fixture may run from the workspace instead of PROJECT, so the
    selected server entry and root are absolute. No legacy file fallback occurs.
    """
    root = launcher_test_root(project)
    if root is None:
        return original
    return ["node", str(project.resolve() / "tests/support/launcher-static-server.mjs"), str(port), str(root)]


def launcher_test_file(project: Path, name: str) -> Path:
    """读取所选 Launcher 产物，默认由正式的前端产物解析器决定路径。"""
    root = launcher_test_root(project)
    if root is not None:
        path = (root / name).resolve()
        if not path.is_relative_to(root):
            raise ValueError("Launcher test file must stay inside its target")
    else:
        result = subprocess.run(
            [
                "node",
                "--input-type=module",
                "--eval",
                "import {resolveFrontendPackageSource} from './lib/frontend-manifest.mjs';"
                "process.stdout.write(resolveFrontendPackageSource(process.argv[1]));",
                name,
            ],
            cwd=project,
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        path = Path(result.stdout)
    if not path.is_file():
        raise FileNotFoundError(f"Launcher test artifact is missing: {path}")
    return path


def runtime_frame_url(frame) -> str:
    """读取实际 iframe 文档；location.replace 不会同步修改 src 属性。"""
    from .runtime_document_observation import runtime_document_observation_script

    return frame.evaluate(
        "(frame) => {"
        "if (!globalThis.__runtimeDocumentObservation) {"
        + runtime_document_observation_script()
        + "} return globalThis.__runtimeDocumentObservation.url(frame); }"
    )
