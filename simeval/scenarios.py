"""Synthetic scenario generation: lane-graph templates, ego start, goal, actors and an on-road log."""

from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np
import yaml

from .config import make_engine, root

LANE_STEP = 2.0  # metres between lane-graph nodes


class _Graph:
    def __init__(self, x: float, y: float):
        self.nodes: list[list[float]] = [[x, y]]
        self.edges: list[tuple[int, int]] = []

    def chain(self, start: int, pts: list[tuple[float, float]]) -> list[int]:
        ids = [start]
        for p in pts:
            self.nodes.append([p[0], p[1]])
            self.edges.append((ids[-1], len(self.nodes) - 1))
            ids.append(len(self.nodes) - 1)
        return ids


def _straight(x, y, h, length):
    n = max(1, round(length / LANE_STEP))
    return [(x + math.cos(h) * LANE_STEP * i, y + math.sin(h) * LANE_STEP * i) for i in range(1, n + 1)]


def _arc(x, y, h, radius, angle):
    """Points along an arc; angle > 0 turns left. Returns (points, end heading)."""
    s = 1.0 if angle >= 0 else -1.0
    cx, cy = x - s * radius * math.sin(h), y + s * radius * math.cos(h)
    n = max(2, math.ceil(abs(angle) * radius / LANE_STEP))
    pts = []
    for k in range(1, n + 1):
        hk = h + angle * k / n
        pts.append((cx + s * radius * math.sin(hk), cy - s * radius * math.cos(hk)))
    return pts, h + angle


def _extend(g: _Graph, last: int, h: float, kind: str, value) -> tuple[int, float]:
    x, y = g.nodes[last]
    if kind == "straight":
        pts, nh = _straight(x, y, h, value), h
    else:
        pts, nh = _arc(x, y, h, *value)
    ids = g.chain(last, pts)
    return ids[-1], nh


def make_scenario(rng: np.random.Generator, index: int, seed: int) -> dict:
    """One scenario without an on-road log."""
    template = ["straight", "curve", "intersection"][index % 3]
    h0 = float(rng.uniform(-math.pi, math.pi))
    x0, y0 = (float(v) for v in rng.uniform(-200, 200, 2))
    g = _Graph(x0, y0)
    h = h0
    if template == "straight":
        last, h = _extend(g, 0, h, "straight", float(rng.uniform(120, 260)))
    elif template == "curve":
        last, h = _extend(g, 0, h, "straight", float(rng.uniform(30, 50)))
        angle = float(rng.uniform(0.8, 1.8)) * (1 if rng.random() < 0.5 else -1)
        last, h = _extend(g, last, h, "arc", (float(rng.uniform(25, 60)), angle))
        last, h = _extend(g, last, h, "straight", float(rng.uniform(30, 60)))
    else:
        junction, h_in = _extend(g, 0, h, "straight", float(rng.uniform(50, 70)))
        branches = {}
        for name in ("ahead", "left", "right"):
            if name == "ahead":
                end, bh = _extend(g, junction, h_in, "straight", 90.0)
            else:
                turn = 1 if name == "left" else -1
                mid, mh = _extend(g, junction, h_in, "arc", (12.0, turn * math.pi / 2))
                end, bh = _extend(g, mid, mh, "straight", 60.0)
            branches[name] = (end, bh)
        chosen = ["ahead", "left", "right"][int(rng.integers(0, 3))]
        last, h = branches[chosen]
    goal = g.nodes[last]

    # Route length by walking the graph from node 0 to the goal (single path per template).
    adj: dict[int, list[int]] = {}
    for a, b in g.edges:
        adj.setdefault(a, []).append(b)
    prev: dict[int, int] = {}
    stack = [0]
    while stack:
        u = stack.pop()
        for v in adj.get(u, []):
            prev[v] = u
            stack.append(v)
    path, u = [last], last
    while u != 0:
        u = prev[u]
        path.append(u)
    path.reverse()
    length = sum(
        math.dist(g.nodes[a], g.nodes[b]) for a, b in zip(path, path[1:], strict=False)
    )

    limit = float(rng.choice([8.0, 10.0, 12.0, 14.0]))
    actors = []
    if rng.random() < 0.5:  # slower leader in the ego lane
        actors.append(
            {"id": 1, "s0": float(rng.uniform(25, min(80, 0.5 * length))),
             "speed": float(rng.uniform(3.0, min(6.0, limit))), "lateral": 0.0}
        )
    for _ in range(int(rng.integers(0, 3))):  # parked actors beside the lane
        actors.append(
            {"id": len(actors) + 1, "s0": float(rng.uniform(40, max(41.0, length - 20))),
             "speed": 0.0, "lateral": float(rng.choice([-4.0, 4.0]))}
        )
    actors = actors[:3]

    return {
        "id": f"s{index + 1:04d}",
        "seed": seed * 100000 + index,
        "template": template,
        "speed_limit": limit,
        "map": {
            "nodes": [{"id": i, "x": n[0], "y": n[1]} for i, n in enumerate(g.nodes)],
            "edges": [{"from": a, "to": b} for a, b in g.edges],
        },
        "ego_start": {"x": x0, "y": y0, "yaw": h0, "v": float(rng.uniform(0, 4))},
        "goal": {"x": goal[0], "y": goal[1]},
        "actors": actors,
        "onroad_log": [],
    }


def onroad_engine():
    """The 'real vehicle': expert driver on the van platform (actuator latency)."""
    cfg_dir = root() / "configs"
    van = yaml.safe_load((cfg_dir / "platforms" / "van.yaml").read_text())
    cfg = {
        "mode": "closed_loop",
        "horizon_s": 150,
        "obs_noise": 0.0,
        "trace": "summary",
        "components": {"policy": "expert", "router": "astar", "controller": "pid", "vehicle": "platform"},
        "platform": van,
    }
    return make_engine(cfg)


def attach_onroad_log(scenario: dict, engine) -> dict:
    res = engine.run(json.dumps(scenario))
    scenario["onroad_log"] = [
        {"tick": i, "x": r[0], "y": r[1], "yaw": r[2], "v": r[3], "steer": r[4], "accel": r[5]}
        for i, r in enumerate(res["trajectory"])
    ]
    scenario["onroad_platform"] = "van"
    scenario["onroad_completion_pct"] = res["metrics"]["route_completion_pct"]
    return scenario


def generate(n: int, seed: int, out_dir: str | Path) -> list[Path]:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    engine = onroad_engine()
    paths = []
    for i in range(n):
        for attempt in range(5):  # keep only on-road logs of drives that reached the goal
            rng = np.random.default_rng([seed, i, attempt])
            sc = attach_onroad_log(make_scenario(rng, i, seed), engine)
            if sc["onroad_completion_pct"] >= 99.0:
                break
        p = out / f"{sc['id']}.json"
        p.write_text(json.dumps(sc, separators=(",", ":")))
        paths.append(p)
    return paths


def resolve(scenario: str | Path, directory: str | Path = "scenarios") -> Path:
    p = Path(scenario)
    return p if p.exists() else Path(directory) / f"{scenario}.json"
