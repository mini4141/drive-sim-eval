"""Profiler export: scoped-timer spans -> Chrome Trace Event JSON (Perfetto) + summary table."""

from __future__ import annotations

import json
from pathlib import Path

import pandas as pd


def chrome_trace(run: dict) -> dict:
    events = [
        {"name": "process_name", "ph": "M", "pid": 1, "args": {"name": f"simcore {run['run_id']}"}},
        {"name": "thread_name", "ph": "M", "pid": 1, "tid": 1, "args": {"name": "sim loop"}},
    ]
    for s in run["spans"]:
        events.append(
            {"name": s["name"], "cat": "sim", "ph": "X", "ts": s["ts_us"], "dur": s["dur_us"],
             "pid": 1, "tid": 1}
        )
    return {"traceEvents": events, "displayTimeUnit": "ms"}


def summary(run: dict) -> dict:
    return {
        "run_id": run["run_id"],
        "ticks": run["ticks"],
        "components": {
            name: {k: t[k] for k in ("count", "total_us", "mean_us", "p95_us")}
            for name, t in run["timings"].items()
        },
    }


def summary_table(s: dict) -> str:
    df = pd.DataFrame.from_dict(s["components"], orient="index")
    return df.sort_values("total_us", ascending=False).round(2).to_string()


def write_profile(run: dict, out_dir: str | Path) -> tuple[Path, Path, str]:
    if "spans" not in run:
        raise ValueError("run has no spans; re-run with a config that sets `spans: true`")
    out = Path(out_dir)
    trace_p = out / f"{run['run_id']}.trace.json"
    sum_p = out / f"{run['run_id']}.profile.json"
    trace_p.write_text(json.dumps(chrome_trace(run), separators=(",", ":")))
    s = summary(run)
    sum_p.write_text(json.dumps(s, indent=2))
    return trace_p, sum_p, summary_table(s)
