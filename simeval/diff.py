"""Sim-vs-road divergence tracing: align trajectories by tick, then find the first diverging interface."""

from __future__ import annotations

import copy
import json
from pathlib import Path

import numpy as np
import simcore

POSITION_THRESHOLD_M = 0.5

# Interfaces observable on-road, in pipeline order: (component, on-road label, tolerance)
INTERFACES = [
    ("Sensor", "Sensor→Policy", 1e-6),
    ("Controller", "Controller→VehicleModel", 1e-9),
    ("VehicleModel", "VehicleModel→Sensor", 1e-6),
]


def _first_mismatch(sim: np.ndarray, ref: np.ndarray, tol: float) -> tuple[int | None, float]:
    n = min(len(sim), len(ref))
    err = np.abs(sim[:n] - ref[:n]).max(axis=1)
    bad = np.nonzero(err > tol)[0]
    return (int(bad[0]) if len(bad) else None), float(err.max()) if n else 0.0


def reference_obs(engine_cfg: dict, scenario: dict) -> np.ndarray:
    """Observations the sensor model produces from the logged on-road states (teacher-forced)."""
    cfg = copy.deepcopy(engine_cfg)
    cfg.update({"mode": "open_loop", "trace": "full", "spans": False, "log_level": "warn"})
    cfg["components"] = {**cfg["components"], "controller": "pid", "vehicle": "log_replay"}
    res = simcore.Engine(json.dumps(cfg)).run(json.dumps(scenario))
    return np.array(res["traces"]["Sensor"]["outputs"])


def diff_run(run: dict, scenario: dict, engine_cfg: dict, injected: str | None = None) -> dict:
    log = scenario["onroad_log"]
    ref_state = np.array([[e["x"], e["y"], e["yaw"], e["v"]] for e in log])
    ref_cmd = np.array([[e["steer"], e["accel"]] for e in log])
    traj = np.array(run["trajectory"])
    n = min(len(traj), len(ref_state))
    err = np.hypot(traj[:n, 0] - ref_state[:n, 0], traj[:n, 1] - ref_state[:n, 1])
    over = np.nonzero(err > POSITION_THRESHOLD_M)[0]

    traces = run["traces"]
    sim_obs = np.array(traces["Sensor"]["outputs"])
    sim_cmd = np.array(traces["Controller"]["outputs"])
    sim_state_next = np.array(traces["VehicleModel"]["outputs"])
    streams = {
        "Sensor": (sim_obs, reference_obs(engine_cfg, scenario)),
        "Controller": (sim_cmd, ref_cmd),
        # state after tick t is the logged state at tick t+1
        "VehicleModel": (sim_state_next[:-1], ref_state[1 : len(sim_state_next)]),
    }
    interfaces = []
    for comp, label, tol in INTERFACES:
        first, worst = _first_mismatch(*streams[comp], tol)
        interfaces.append(
            {"component": comp, "label": label, "tolerance": tol, "first_tick": first,
             "max_abs_diff": worst, "status": "match" if first is None else "diverges"}
        )
    diverging = [i for i in interfaces if i["first_tick"] is not None]
    # earliest tick wins; ties resolved by pipeline order (INTERFACES order)
    first_if = min(diverging, key=lambda i: i["first_tick"]) if diverging else None

    verdict = "no interface diverges within tolerance"
    if first_if:
        by = {i["component"]: i for i in interfaces}
        t = first_if["first_tick"]
        if first_if["component"] == "VehicleModel":
            verdict = (
                f"{first_if['label']}: state diverges from tick {t} while the "
                f"Controller→VehicleModel commands "
                f"{'match' if by['Controller']['status'] == 'match' else 'also differ'} "
                f"(max |Δ|={by['Controller']['max_abs_diff']:.2e}) ⇒ "
                "vehicle-dynamics/latency mismatch"
            )
        elif first_if["component"] == "Sensor":
            verdict = (
                f"{first_if['label']}: observation diverges at tick {t} "
                f"(max |Δ|={first_if['max_abs_diff']:.3f}) ⇒ data-flow fault "
                "(stale or dropped sensor data)"
            )
        else:
            verdict = (
                f"{first_if['label']}: commands diverge at tick {t} while the observation "
                "matches ⇒ policy/controller mismatch"
            )
    return {
        "run_id": run["run_id"],
        "scenario_id": scenario["id"],
        "platform": run["platform"],
        "injected_fault": injected,
        "threshold_m": POSITION_THRESHOLD_M,
        "divergence_tick": int(over[0]) if len(over) else None,
        "max_position_error_m": float(err.max()) if n else 0.0,
        "first_diverging_interface": first_if["component"] if first_if else None,
        "first_diverging_label": first_if["label"] if first_if else None,
        "first_diverging_tick": first_if["first_tick"] if first_if else None,
        "interfaces": interfaces,
        "not_observable_on_road": ["Policy"],
        "verdict": verdict,
        "error_curve": [round(float(e), 4) for e in err],
        "sim_xy": [[round(float(x), 3), round(float(y), 3)] for x, y in traj[:n, :2]],
        "onroad_xy": [[round(float(x), 3), round(float(y), 3)] for x, y in ref_state[:n, :2]],
    }


def write_diff(report: dict, out_dir: str | Path) -> Path:
    p = Path(out_dir) / f"{report['run_id']}.diff.json"
    p.write_text(json.dumps(report, separators=(",", ":")))
    return p

