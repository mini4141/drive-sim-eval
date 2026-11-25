"""AWS Batch entrypoints: array-job shards and the aggregate job (S3 in, S3 out)."""

from __future__ import annotations

import json
import time
from datetime import UTC, datetime
from pathlib import Path

import boto3

from . import batch
from .config import load_config, make_engine
from .log import emit


def split_uri(uri: str) -> tuple[str, str]:
    if not uri.startswith("s3://"):
        raise ValueError(f"expected s3:// URI, got {uri!r}")
    bucket, _, key = uri[5:].partition("/")
    return bucket, key.rstrip("/")


def _list_keys(s3, bucket: str, prefix: str) -> list[str]:
    keys = []
    for page in s3.get_paginator("list_objects_v2").paginate(Bucket=bucket, Prefix=prefix + "/"):
        keys += [o["Key"] for o in page.get("Contents", [])]
    return sorted(keys)


def _put_json(s3, bucket: str, key: str, obj) -> None:
    s3.put_object(
        Bucket=bucket, Key=key, Body=json.dumps(obj, separators=(",", ":")).encode(),
        ContentType="application/json",
    )


def batch_shard(index: int, shard_size: int, in_uri: str, out_uri: str, config: str,
                platform: str | None = None) -> dict:
    s3 = boto3.client("s3")
    in_bucket, in_prefix = split_uri(in_uri)
    out_bucket, out_prefix = split_uri(out_uri)
    keys = [k for k in _list_keys(s3, in_bucket, in_prefix) if k.endswith(".json")]
    mine = keys[index * shard_size : (index + 1) * shard_size]
    cfg = load_config(config, platform)
    engine = make_engine(cfg)
    emit("shard_start", index=index, scenarios=len(mine), total=len(keys))
    t0 = time.perf_counter()
    runs = []
    for key in mine:
        scenario = s3.get_object(Bucket=in_bucket, Key=key)["Body"].read().decode()
        res = engine.run(scenario)
        res["run_id"], res["platform"] = res["scenario_id"], cfg["platform"]["name"]
        _put_json(s3, out_bucket, f"{out_prefix}/{res['run_id']}.json", res)
        runs.append(batch.summarize(res))
    wall = time.perf_counter() - t0
    summary = {"index": index, "wall_s": wall, "runs": runs, "mode": cfg["mode"],
               "platform": cfg["platform"]["name"], "thresholds": cfg.get("thresholds", {})}
    _put_json(s3, out_bucket, f"{out_prefix}/shards/shard-{index:03d}.json", summary)
    emit("shard_complete", index=index, runs=len(runs), wall_s=round(wall, 3))
    return summary


def aggregate_cloud(out_uri: str) -> dict:
    s3 = boto3.client("s3")
    bucket, prefix = split_uri(out_uri)
    shards = [
        json.loads(s3.get_object(Bucket=bucket, Key=k)["Body"].read())
        for k in _list_keys(s3, bucket, f"{prefix}/shards")
    ]
    runs = [r for s in shards for r in s["runs"]]
    wall = max((s["wall_s"] for s in shards), default=0.0)  # shards run in parallel
    batch_id = prefix.rsplit("/", 1)[-1]
    index = batch.build_index(
        batch_id, runs, shards[0]["thresholds"] if shards else {},
        len(runs) / wall if wall else 0.0,
        workers=len(shards), mode=shards[0]["mode"] if shards else "", platform="car",
        wall_s=wall, combined_digest=batch.combined_digest(runs), backend="aws-batch",
    )
    _put_json(s3, bucket, f"{prefix}/index.json", index)

    root = prefix.rsplit("/", 1)[0]
    entries = []
    for k in _list_keys(s3, bucket, root):
        if k.endswith("/index.json"):
            idx = json.loads(s3.get_object(Bucket=bucket, Key=k)["Body"].read())
            entries.append(batch.manifest_entry(idx["batch_id"], idx))
    entries.sort(key=lambda e: e["created_at"], reverse=True)
    _put_json(s3, bucket, f"{root}/batches.json", entries)
    emit("aggregate_complete", batch_id=batch_id, runs=len(runs), shards=len(shards),
         at=datetime.now(UTC).isoformat(), index=str(Path(prefix) / "index.json"))
    return index
