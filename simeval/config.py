"""Pipeline config loading: YAML pipeline + platform YAML + policy weights -> engine JSON."""

from __future__ import annotations

import json
import os
from pathlib import Path

import simcore
import yaml


def root() -> Path:
    return Path(os.environ.get("SIMEVAL_ROOT", Path.cwd()))


def load_config(config_path: str | Path, platform: str | None = None) -> dict:
    """Return the engine config dict for a pipeline YAML and platform name."""
    config_path = Path(config_path)
    cfg = yaml.safe_load(config_path.read_text())
    name = platform or cfg.get("platform", "car")
    plat_file = config_path.parent / "platforms" / f"{name}.yaml"
    cfg["platform"] = yaml.safe_load(plat_file.read_text())
    weights = cfg.pop("policy_weights_path", None)
    if cfg.get("components", {}).get("policy") == "mlp":
        if weights is None:
            raise ValueError("policy_weights_path is required for the mlp policy")
        cfg["policy_weights"] = json.loads((root() / weights).read_text())
    return cfg


def make_engine(cfg: dict) -> simcore.Engine:
    return simcore.Engine(json.dumps(cfg))


def parse_fault(text: str) -> tuple[str, int]:
    """'sensor_dropout@tick=40' -> ('sensor_dropout', 40)."""
    name, _, rest = text.partition("@tick=")
    if not rest:
        raise ValueError(f"fault must look like name@tick=N, got {text!r}")
    return name, int(rest)
