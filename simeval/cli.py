"""simeval CLI (typer)."""

from __future__ import annotations

import json
import os
from pathlib import Path

import typer

from . import batch as batch_mod
from . import scenarios as scen
from .config import load_config, make_engine, parse_fault
from .log import emit

app = typer.Typer(add_completion=False, no_args_is_help=True, help="Drive Sim Eval CLI")

RESULTS = typer.Option("results", help="Results root folder")
SCENARIOS = typer.Option("scenarios", help="Scenario folder")


def _run_single(scenario_path: Path, config: str, platform: str | None, batch_id: str,
                results: str, inject: str | None, tag: str) -> dict:
    cfg = load_config(config, platform)
    engine = make_engine(cfg)
    if inject:
        engine.set_fault(*parse_fault(inject))
    res = engine.run(scenario_path.read_text())
    plat = cfg["platform"]["name"]
    suffix = f"_{inject.split('@')[0]}{inject.rsplit('=', 1)[-1]}" if inject else ""
    res["run_id"] = f"{res['scenario_id']}_{plat}_{tag}{suffix}"
    res["platform"] = plat
    res["injected_fault"] = inject
    out_dir = Path(results) / batch_id
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / f"{res['run_id']}.json").write_text(json.dumps(res, separators=(",", ":")))
    batch_mod.rebuild_index(out_dir)
    emit("run_complete", run_id=res["run_id"], platform=plat, mode=res["mode"],
         digest=res["digest"], metrics=res["metrics"])
    return res


@app.command("gen-scenarios")
def gen_scenarios(
    n: int = typer.Option(200, help="Number of scenarios"),
    seed: int = typer.Option(42),
    out: Path = typer.Option(Path("scenarios")),
) -> None:
    """Generate synthetic scenarios (lane graph, ego, goal, actors, on-road log)."""
    paths = scen.generate(n, seed, out)
    emit("scenarios_generated", n=len(paths), out=str(out), seed=seed)


@app.command()
def run(
    scenario: Path = typer.Option(..., help="Scenario JSON"),
    config: str = typer.Option("configs/closed_loop.yaml"),
    platform: str = typer.Option(None, help="car | van | robot"),
    batch_id: str = typer.Option("adhoc", "--batch-id"),
    results: str = RESULTS,
    inject: str = typer.Option(None, help="e.g. sensor_dropout@tick=40"),
) -> None:
    """Run one scenario and write results/<batch>/<run_id>.json."""
    tag = Path(config).stem
    _run_single(scenario, config, platform, batch_id, results, inject, tag)


@app.command("batch")
def batch_cmd(
    scenarios: Path = typer.Option(Path("scenarios")),
    config: str = typer.Option("configs/open_loop.yaml"),
    workers: int = typer.Option(8),
    batch_id: str = typer.Option(..., "--batch-id"),
    results: str = RESULTS,
    platform: str = typer.Option(None),
) -> None:
    """Run every scenario in a process pool and write index.json."""
    batch_mod.run_batch(scenarios, config, workers, batch_id, results, platform)


@app.command("bench-scaling")
def bench_scaling(
    scenarios: Path = typer.Option(Path("scenarios")),
    config: str = typer.Option("configs/open_loop.yaml"),
    batch_id: str = typer.Option(..., "--batch-id"),
    results: str = RESULTS,
    workers: str = typer.Option("1,2,4,8", help="Comma-separated worker counts"),
) -> None:
    """Run the same batch at several worker counts and write scaling.json."""
    batch_mod.bench_scaling(scenarios, config, batch_id, results,
                            [int(w) for w in workers.split(",")])


@app.command("repro-check")
def repro_check(
    scenario: str = typer.Option(..., help="Scenario id or path"),
    repeats: int = typer.Option(5),
    workers: str = typer.Option("1,4"),
    scenarios: Path = typer.Option(Path("scenarios")),
    config: str = typer.Option("configs/closed_loop.yaml"),
    platform: str = typer.Option(None),
    results: str = RESULTS,
) -> None:
    """Run a scenario repeatedly and compare digests."""
    from .repro import repro_check as check

    out = check(scen.resolve(scenario, scenarios), config, repeats,
                [int(w) for w in workers.split(",")], results, platform)
    if not out["reproducible"]:
        raise typer.Exit(1)


@app.command()
def diff(
    onroad: str = typer.Option(..., help="Scenario id or path holding the on-road log"),
    run: Path = typer.Option(None, help="Existing run.json with full traces"),
    inject: str = typer.Option(None, help="e.g. sensor_dropout@tick=40 (re-runs the sim)"),
    platform: str = typer.Option("car"),
    config: str = typer.Option("configs/diff.yaml"),
    scenarios: Path = typer.Option(Path("scenarios")),
    batch_id: str = typer.Option("demo", "--batch-id"),
    results: str = RESULTS,
) -> None:
    """Find where a simulated run first diverges from the on-road log."""
    from .diff import diff_run, write_diff

    sc_path = scen.resolve(onroad, scenarios)
    scenario = json.loads(sc_path.read_text())
    cfg = load_config(config, platform)
    if run is None or inject:
        res = _run_single(sc_path, config, platform, batch_id, results, inject, Path(config).stem)
    else:
        res = json.loads(run.read_text())
    report = diff_run(res, scenario, cfg, inject)
    path = write_diff(report, Path(results) / batch_id)
    emit("diff_complete", run_id=res["run_id"], first_diverging_interface=report["first_diverging_interface"],
         first_tick=report["first_diverging_tick"], divergence_tick=report["divergence_tick"],
         verdict=report["verdict"], report=str(path))


@app.command()
def profile(
    run: Path = typer.Option(..., help="run.json with spans"),
) -> None:
    """Emit Chrome-trace JSON and a per-component summary for a run."""
    from .profile import write_profile

    res = json.loads(run.read_text())
    trace_p, sum_p, table = write_profile(res, run.parent)
    typer.echo(table)
    emit("profile_written", trace=str(trace_p), summary=str(sum_p),
         note="trace.json opens in Perfetto: https://ui.perfetto.dev")


@app.command("index-batches")
def index_batches(results: str = RESULTS) -> None:
    """Rewrite results/batches.json."""
    batch_mod.update_manifest(results)


@app.command("batch-shard")
def batch_shard(
    index: int = typer.Option(None, help="Shard index (default: $AWS_BATCH_JOB_ARRAY_INDEX)"),
    shard_size: int = typer.Option(10, "--shard-size"),
    in_uri: str = typer.Option(..., "--in"),
    out_uri: str = typer.Option(..., "--out"),
    config: str = typer.Option("configs/open_loop.yaml"),
    platform: str = typer.Option(None),
) -> None:
    """AWS Batch array child: process one slice of scenarios from S3."""
    from .cloud import batch_shard as shard

    idx = index if index is not None else int(os.environ["AWS_BATCH_JOB_ARRAY_INDEX"])
    shard(idx, shard_size, in_uri, out_uri, config, platform)


@app.command()
def aggregate(
    out_uri: str = typer.Option(..., "--out", help="s3://bucket/results/<batch_id>"),
) -> None:
    """AWS Batch aggregate job: merge shard summaries into index.json."""
    from .cloud import aggregate_cloud

    aggregate_cloud(out_uri)


if __name__ == "__main__":
    app()
