"""Open-loop batch evaluation: ProcessPoolExecutor fan-out, aggregation, results contract."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import tempfile
import time
from concurrent.futures import ProcessPoolExecutor
from datetime import UTC, datetime
from pathlib import Path

import numpy as np

from .config import load_config, make_engine
from .log import emit

_ENGINE = None
_PLATFORM = "car"
_MODE = "open_loop"

SKIP_SUFFIXES = (".diff.json", ".profile.json", ".trace.json")
SKIP_NAMES = {"index.json", "scaling.json"}


def _init_worker(cfg_json: str, platform: str, mode: str) -> None:
    global _ENGINE, _PLATFORM, _MODE
    import simcore

    _ENGINE = simcore.Engine(cfg_json)
    _PLATFORM, _MODE = platform, mode


def run_scenario_file(engine, path: str | Path, platform: str, run_id: str | None = None) -> dict:
    text = Path(path).read_text()
    res = engine.run(text)
    res["run_id"] = run_id or res["scenario_id"]
    res["platform"] = platform
    return res


def summarize(res: dict) -> dict:
    return {
        "run_id": res["run_id"],
        "scenario_id": res["scenario_id"],
        "platform": res["platform"],
        "mode": res["mode"],
        "ticks": res["ticks"],
        "metrics": res["metrics"],
        "digest": res["digest"],
    }


def _task(args: tuple[str, str]) -> dict:
    path, out_dir = args
    res = run_scenario_file(_ENGINE, path, _PLATFORM)
    (Path(out_dir) / f"{res['run_id']}.json").write_text(json.dumps(res, separators=(",", ":")))
    return summarize(res)


def aggregate(runs: list[dict], thresholds: dict) -> dict:
    err = np.array([r["metrics"]["action_l2_error"] for r in runs], dtype=float)
    thr = thresholds.get("action_l2_error", 0.5)
    passed = [
        r for r in runs
        if r["metrics"]["action_l2_error"] <= thr and r["metrics"]["collisions"] == 0
    ]
    return {
        "n_runs": len(runs),
        "mean_action_l2_error": float(err.mean()) if len(err) else 0.0,
        "p95_action_l2_error": float(np.percentile(err, 95)) if len(err) else 0.0,
        "mean_route_completion_pct": float(
            np.mean([r["metrics"]["route_completion_pct"] for r in runs]) if runs else 0.0
        ),
        "total_collisions": int(sum(r["metrics"]["collisions"] for r in runs)),
        "thresholds": thresholds,
        "passed": len(passed),
        "failed": len(runs) - len(passed),
    }


def build_index(batch_id: str, runs: list[dict], thresholds: dict, throughput: float, **extra) -> dict:
    runs = sorted(runs, key=lambda r: r["run_id"])
    return {
        "batch_id": batch_id,
        "created_at": datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "runs": runs,
        "aggregate": aggregate(runs, thresholds),
        "throughput_rps": throughput,
        **extra,
    }


def combined_digest(runs: list[dict]) -> str:
    h = hashlib.sha256()
    for r in sorted(runs, key=lambda r: r["run_id"]):
        h.update(f"{r['run_id']}:{r['digest']}\n".encode())
    return h.hexdigest()


def run_batch(
    scenarios_dir: str | Path,
    config_path: str | Path,
    workers: int,
    batch_id: str,
    results_dir: str | Path,
    platform: str | None = None,
) -> dict:
    cfg = load_config(config_path, platform)
    paths = sorted(str(p) for p in Path(scenarios_dir).glob("s*.json"))
    out_dir = Path(results_dir) / batch_id
    out_dir.mkdir(parents=True, exist_ok=True)
    emit("batch_start", batch_id=batch_id, scenarios=len(paths), workers=workers, mode=cfg["mode"])
    t0 = time.perf_counter()
    with ProcessPoolExecutor(
        max_workers=workers,
        initializer=_init_worker,
        initargs=(json.dumps(cfg), cfg["platform"]["name"], cfg["mode"]),
    ) as pool:
        runs = list(pool.map(_task, [(p, str(out_dir)) for p in paths], chunksize=4))
    wall = time.perf_counter() - t0
    throughput = len(runs) / wall if wall > 0 else 0.0
    index = build_index(
        batch_id, runs, cfg.get("thresholds", {}), throughput,
        workers=workers, mode=cfg["mode"], platform=cfg["platform"]["name"],
        wall_s=wall, combined_digest=combined_digest(runs),
    )
    (out_dir / "index.json").write_text(json.dumps(index, separators=(",", ":")))
    emit(
        "batch_complete", batch_id=batch_id, runs=len(runs), workers=workers,
        wall_s=round(wall, 3), throughput_rps=round(throughput, 2),
        mean_action_l2_error=index["aggregate"]["mean_action_l2_error"],
        p95_action_l2_error=index["aggregate"]["p95_action_l2_error"],
        passed=index["aggregate"]["passed"], failed=index["aggregate"]["failed"],
    )
    update_manifest(results_dir)
    return index


def bench_scaling(
    scenarios_dir: str | Path,
    config_path: str | Path,
    batch_id: str,
    results_dir: str | Path,
    workers_list: list[int],
) -> dict:
    """Same batch at several worker counts; the combined digest must not change."""
    points = []
    with tempfile.TemporaryDirectory() as tmp:
        # local copy: keeps bind-mount/network file latency out of the worker-count comparison
        local = Path(tmp) / "scenarios"
        shutil.copytree(scenarios_dir, local)
        for w in workers_list:
            idx = run_batch(local, config_path, w, f"{batch_id}-w{w}", Path(tmp) / "out")
            points.append(
                {
                    "workers": w,
                    "wall_s": idx["wall_s"],
                    "runs_per_sec": idx["throughput_rps"],
                    "combined_digest": idx["combined_digest"],
                }
            )
    base = points[0]["wall_s"]
    for p in points:
        p["speedup"] = base / p["wall_s"] if p["wall_s"] else 0.0
    out = {
        "batch_id": batch_id,
        "n_runs": idx["aggregate"]["n_runs"],
        "cpu_count": os.cpu_count(),
        "digests_identical": len({p["combined_digest"] for p in points}) == 1,
        "points": points,
    }
    out_dir = Path(results_dir) / batch_id
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "scaling.json").write_text(json.dumps(out, indent=2))
    emit("scaling_complete", batch_id=batch_id, digests_identical=out["digests_identical"],
         points=[{k: p[k] for k in ("workers", "wall_s", "runs_per_sec", "speedup")} for p in points])
    return out


def rebuild_index(batch_dir: str | Path) -> dict:
    """Rebuild index.json from the run files in a batch folder (used by `simeval run`)."""
    batch_dir = Path(batch_dir)
    runs = []
    for p in sorted(batch_dir.glob("*.json")):
        if p.name in SKIP_NAMES or p.name.endswith(SKIP_SUFFIXES):
            continue
        res = json.loads(p.read_text())
        if "run_id" in res and "trajectory" in res:
            runs.append(summarize(res))
    index = build_index(batch_dir.name, runs, {"action_l2_error": 0.5}, 0.0, mode="mixed")
    (batch_dir / "index.json").write_text(json.dumps(index, separators=(",", ":")))
    update_manifest(batch_dir.parent)
    return index


def manifest_entry(batch_id: str, index: dict) -> dict:
    return {
        "batch_id": batch_id,
        "created_at": index["created_at"],
        "n_runs": len(index["runs"]),
        "mode": index.get("mode", ""),
        "throughput_rps": index.get("throughput_rps", 0.0),
    }


def update_manifest(results_dir: str | Path) -> None:
    root = Path(results_dir)
    entries = []
    for idx in sorted(root.glob("*/index.json")):
        entries.append(manifest_entry(idx.parent.name, json.loads(idx.read_text())))
    entries.sort(key=lambda e: e["created_at"], reverse=True)
    (root / "batches.json").write_text(json.dumps(entries, indent=2))


def make_engine_for(config_path: str | Path, platform: str | None = None):
    return make_engine(load_config(config_path, platform))
