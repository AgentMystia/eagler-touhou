"""在浏览器用例中装载当前 React 组件，不复制组件实现。"""
from __future__ import annotations

import json
from pathlib import Path
import subprocess


def component_fixture(project: Path, name: str) -> dict:
    result = subprocess.run(
        ["node", str(project / "tests/support/component-fixture.mjs"), name],
        cwd=project, check=True, capture_output=True, text=True, encoding="utf-8",
    )
    return json.loads(result.stdout)
