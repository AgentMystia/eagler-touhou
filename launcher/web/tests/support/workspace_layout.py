"""Python adapter for the repository-owned workspace topology.

The JSON file is the only topology declaration. Python tooling consumes it so
Node and Python workspace tests cannot silently invent different sibling names.
"""

from __future__ import annotations

import json
import os
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[2]
CONFIG = json.loads((PROJECT / "config" / "workspace.json").read_text(encoding="utf-8"))
if CONFIG.get("schema") != "eagler-touhou/workspace-layout/1" or not isinstance(CONFIG.get("repositories"), dict):
    raise RuntimeError("invalid config/workspace.json")

REPOSITORIES = CONFIG["repositories"]
WORKSPACE_ROOT = Path(os.environ.get("EAGLER_WORKSPACE_ROOT", PROJECT.parent)).resolve()


def workspace_path(repository: str, *segments: str) -> Path:
    directory = REPOSITORIES.get(repository)
    if not isinstance(directory, str) or not directory:
        raise KeyError(f"unknown workspace repository: {repository}")
    return WORKSPACE_ROOT.joinpath(directory, *segments)
