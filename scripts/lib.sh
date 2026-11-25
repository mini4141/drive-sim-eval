#!/usr/bin/env bash
# Shared helpers. Source this file; do not run it.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*"
DEV_IMAGE="${DEV_IMAGE:-drive-sim-eval:dev}"
RUNTIME_IMAGE="${SIMEVAL_IMAGE:-drive-sim-eval:local}"

# Host path usable by Docker (C:/... on Windows Git Bash, plain path elsewhere).
host_root() { (cd "$ROOT" && (pwd -W 2>/dev/null || pwd)); }
HROOT="$(host_root)"

build_images() {
  docker build -q -f "$HROOT/docker/Dockerfile" --target dev -t "$DEV_IMAGE" "$HROOT" >/dev/null
  docker build -q -f "$HROOT/docker/Dockerfile" --target runtime -t "$RUNTIME_IMAGE" "$HROOT" >/dev/null
}

# Run a command in the dev image with the repo mounted at /work.
# SIMEVAL_NATIVE=1 (CI) runs it directly on the host instead.
in_dev() {
  if [ "${SIMEVAL_NATIVE:-0}" = "1" ]; then
    (cd "$ROOT" && "$@")
  else
    docker run --rm -e PYTHONPATH=/work -v "$HROOT:/work" -w /work --entrypoint "$1" "$DEV_IMAGE" "${@:2}"
  fi
}

sim() { in_dev simeval "$@"; }
