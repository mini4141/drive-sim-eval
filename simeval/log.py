"""Structured JSON logging to stdout (the C++ core logs the same shape to stderr)."""

from __future__ import annotations

import json
import sys
from datetime import UTC, datetime


def emit(event: str, level: str = "info", **fields) -> None:
    rec = {
        "ts": datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z",
        "level": level,
        "logger": "simeval",
        "event": event,
        **fields,
    }
    sys.stdout.write(json.dumps(rec) + "\n")
    sys.stdout.flush()
