import json
import re
from pathlib import Path

from typer.testing import CliRunner

from simeval.cli import app

runner = CliRunner()


def invoke(*args):
    result = runner.invoke(app, [str(a) for a in args])
    assert result.exit_code == 0, result.output
    return result


def test_bindings_engine_run(scenarios):
    import simcore

    from simeval.config import load_config, make_engine

    assert simcore.__version__
    res = make_engine(load_config("configs/closed_loop.yaml", "car")).run(
        (scenarios / "s0001.json").read_text()
    )
    assert re.fullmatch(r"[0-9a-f]{64}", res["digest"])


def test_gen_scenarios(tmp_path):
    invoke("gen-scenarios", "--n", 2, "--seed", 3, "--out", tmp_path)
    sc = json.loads((tmp_path / "s0001.json").read_text())
    assert sc["onroad_log"] and sc["map"]["nodes"] and "goal" in sc


def test_run_produces_finite_metrics(scenarios, tmp_path):
    invoke("run", "--scenario", scenarios / "s0001.json", "--platform", "robot",
           "--batch-id", "t", "--results", tmp_path)
    run = json.loads((tmp_path / "t" / "s0001_robot_closed_loop.json").read_text())
    assert all(isinstance(v, (int, float)) and v == v for v in run["metrics"].values())
    assert re.fullmatch(r"[0-9a-f]{64}", run["digest"])


def test_batch_writes_index(scenarios, tmp_path):
    invoke("batch", "--scenarios", scenarios, "--workers", 2, "--batch-id", "b", "--results", tmp_path)
    index = json.loads((tmp_path / "b" / "index.json").read_text())
    assert len(index["runs"]) == 10 and index["throughput_rps"] > 0
    assert json.loads((tmp_path / "batches.json").read_text())[0]["batch_id"] == "b"


def test_bench_scaling_digests_identical(scenarios, tmp_path):
    invoke("bench-scaling", "--scenarios", scenarios, "--batch-id", "s", "--results", tmp_path,
           "--workers", "1,2")
    scaling = json.loads((tmp_path / "s" / "scaling.json").read_text())
    assert scaling["digests_identical"] and len(scaling["points"]) == 2


def test_repro_check_is_reproducible(scenarios, tmp_path):
    invoke("repro-check", "--scenario", "s0001", "--scenarios", scenarios, "--repeats", 3,
           "--workers", "1,2", "--results", tmp_path)
    assert json.loads((tmp_path / "repro.json").read_text())["reproducible"] is True


def test_diff_names_vehicle_model(scenarios, tmp_path):
    invoke("diff", "--onroad", "s0001", "--scenarios", scenarios, "--results", tmp_path)
    report = json.loads((tmp_path / "demo" / "s0001_car_diff.diff.json").read_text())
    assert report["first_diverging_interface"] == "VehicleModel"
    invoke("diff", "--onroad", "s0001", "--scenarios", scenarios, "--results", tmp_path,
           "--platform", "van", "--inject", "sensor_dropout@tick=40")
    injected = json.loads(
        (tmp_path / "demo" / "s0001_van_diff_sensor_dropout40.diff.json").read_text()
    )
    assert injected["first_diverging_interface"] == "Sensor"
    assert injected["first_diverging_tick"] == 40


def test_profile_writes_chrome_trace(scenarios, tmp_path):
    invoke("run", "--scenario", scenarios / "s0002.json", "--batch-id", "p", "--results", tmp_path)
    invoke("profile", "--run", tmp_path / "p" / "s0002_car_closed_loop.json")
    trace = json.loads((Path(tmp_path) / "p" / "s0002_car_closed_loop.trace.json").read_text())
    assert any(e["ph"] == "X" for e in trace["traceEvents"])
