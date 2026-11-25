"""Train the 8 -> 32 -> 2 MLP policy by imitating the expert (pure pursuit + speed profile).

Rollouts come from the C++ engine driven by the expert policy on generated scenarios with
perturbed ego starts and steering noise (so the data covers recovery states). The network is
trained with numpy (Adam, MSE) and exported as JSON for the C++ `MlpPolicy`.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import numpy as np
import simcore
import yaml

from simeval import scenarios as scen

K_SCALE, V_SCALE = 0.25, 15.0
HIDDEN = 32


def collect(n_scenarios: int, seed: int, root: Path) -> tuple[np.ndarray, np.ndarray]:
    car = yaml.safe_load((root / "configs/platforms/car.yaml").read_text())
    cfg = {
        "mode": "closed_loop", "horizon_s": 60, "obs_noise": 0.01, "trace": "summary",
        "record_samples": True, "action_noise": 0.04,
        "components": {"policy": "expert", "router": "astar", "controller": "pid", "vehicle": "platform"},
        "platform": car,
    }
    engine = simcore.Engine(json.dumps(cfg))
    rng = np.random.default_rng(seed)
    xs, ys = [], []
    for i in range(n_scenarios):
        sc = scen.make_scenario(np.random.default_rng([seed, i]), i, seed)
        e = sc["ego_start"]
        lat = rng.uniform(-2.0, 2.0)
        e["x"] += -math.sin(e["yaw"]) * lat
        e["y"] += math.cos(e["yaw"]) * lat
        e["yaw"] += rng.uniform(-0.35, 0.35)
        e["v"] = rng.uniform(0.0, 12.0)
        res = engine.run(json.dumps(sc))
        xs += res["samples"]["obs"]
        ys += res["samples"]["action"]
    return np.array(xs), np.array(ys) / np.array([K_SCALE, V_SCALE])


LOSS_W = np.array([8.0, 1.0])  # curvature errors destabilise tracking far more than speed errors


def train(x: np.ndarray, y: np.ndarray, epochs: int, seed: int) -> tuple[list, dict]:
    rng = np.random.default_rng(seed)
    n = len(x)
    idx = rng.permutation(n)
    n_val = n // 10
    val, tr = idx[:n_val], idx[n_val:]
    w1 = rng.normal(0, 1 / math.sqrt(8), (HIDDEN, 8))
    b1 = np.zeros(HIDDEN)
    w2 = rng.normal(0, 1 / math.sqrt(HIDDEN), (2, HIDDEN))
    b2 = np.zeros(2)
    params = [w1, b1, w2, b2]
    m = [np.zeros_like(p) for p in params]
    v = [np.zeros_like(p) for p in params]
    step = 0
    for epoch in range(epochs):
        lr = 5e-3 * (0.01 ** (epoch / epochs))
        order = rng.permutation(tr)
        for s in range(0, len(order), 256):
            b = order[s : s + 256]
            h = np.tanh(x[b] @ w1.T + b1)
            d = (h @ w2.T + b2 - y[b]) * LOSS_W * (2 / len(b))
            grads = [
                ((d @ w2) * (1 - h * h)).T @ x[b], ((d @ w2) * (1 - h * h)).sum(0),
                d.T @ h, d.sum(0),
            ]
            step += 1
            for p, g, mi, vi in zip(params, grads, m, v, strict=True):
                mi[:] = 0.9 * mi + 0.1 * g
                vi[:] = 0.999 * vi + 0.001 * g * g
                p -= lr * (mi / (1 - 0.9**step)) / (np.sqrt(vi / (1 - 0.999**step)) + 1e-8)
    pred = np.tanh(x[val] @ w1.T + b1) @ w2.T + b2
    rmse = np.sqrt(((pred - y[val]) ** 2).mean(0)) * np.array([K_SCALE, V_SCALE])
    metrics = {"val_rmse_curvature": float(rmse[0]), "val_rmse_speed_mps": float(rmse[1])}
    layers = [{"W": w1.tolist(), "b": b1.tolist()}, {"W": w2.tolist(), "b": b2.tolist()}]
    return layers, metrics


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenarios", type=int, default=180)
    ap.add_argument("--epochs", type=int, default=400)
    ap.add_argument("--cache", default=".local/train_data.npz")
    ap.add_argument("--seed", type=int, default=1234)
    ap.add_argument("--out", default="tools/policy_weights.json")
    a = ap.parse_args()
    root = Path.cwd()
    cache = Path(a.cache)
    if cache.exists():
        d = np.load(cache)
        x, y = d["x"], d["y"]
    else:
        x, y = collect(a.scenarios, a.seed, root)
        cache.parent.mkdir(exist_ok=True)
        np.savez(cache, x=x, y=y)
    layers, metrics = train(x, y, a.epochs, a.seed)
    out = {
        "layers": layers, "activation": "tanh", "out_scale": [K_SCALE, V_SCALE],
        "training": {"samples": len(x), "scenarios": a.scenarios, "epochs": a.epochs,
                     "seed": a.seed, **metrics},
    }
    Path(a.out).write_text(json.dumps(out))
    print(json.dumps({"event": "policy_trained", **out["training"]}))


if __name__ == "__main__":
    main()
