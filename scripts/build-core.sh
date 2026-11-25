#!/usr/bin/env bash
# Build the C++ core + run GoogleTest (ctest) inside a Linux container; build the dev/runtime images.
# Usage: build-core.sh [report-dir (relative to repo root)]
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
OUT="${1:-.local/test-reports}"
mkdir -p "$ROOT/$OUT"
docker build -f "$HROOT/docker/Dockerfile" --target core-build -t drive-sim-eval:core-build "$HROOT"
cid="$(docker create drive-sim-eval:core-build)"
docker cp "$cid:/out/ctest-junit.xml" "$HROOT/$OUT/ctest-junit.xml"
docker rm "$cid" >/dev/null
build_images
echo "core built, ctest report: $OUT/ctest-junit.xml"
