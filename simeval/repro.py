"""Reproducibility check: the same scenario, config and seed must give an identical digest."""

from __future__ import annotations

import json
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

from . import batch
from .config import load_config
from .log import emit


def _digest(path: str) -> str:
    return batch._ENGINE.run(Path(path).read_text())["digest"]


def repro_check(
    scenario_path: str | Path,
    config_path: str | Path,
    repeats: int,
    workers_list: list[int],
    results_dir: str | Path,
    platform: str | None = None,
) -> dict:
    cfg = load_config(config_path, platform)
    rows = []
    for w in workers_list:
        with ProcessPoolExecutor(
            max_workers=w,
            initializer=batch._init_worker,
            initargs=(json.dumps(cfg), cfg["platform"]["name"], cfg["mode"]),
        ) as pool:
            digests = list(pool.map(_digest, [str(scenario_path)] * repeats))
        rows.extend({"workers": w, "repeat": i + 1, "digest": d} for i, d in enumerate(digests))
    reference = rows[0]["digest"]
    for r in rows:
        r["matches"] = r["digest"] == reference
    sid = json.loads(Path(scenario_path).read_text())["id"]
    out = {
        "scenario_id": sid,
        "mode": cfg["mode"],
        "platform": cfg["platform"]["name"],
        "repeats": repeats,
        "workers": workers_list,
        "reference_digest": reference,
        "rows": rows,
        "reproducible": all(r["matches"] for r in rows),
    }
    Path(results_dir).mkdir(parents=True, exist_ok=True)
    (Path(results_dir) / "repro.json").write_text(json.dumps(out, indent=2))
    emit("repro_check", scenario=sid, reproducible=out["reproducible"], runs=len(rows),
         digest=reference)
    return out
