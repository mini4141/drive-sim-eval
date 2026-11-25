#!/usr/bin/env bash
# Generate demo data: 200 scenarios, the open-loop batch, scaling benchmark, platform runs,
# divergence reports, a profile and the repro check. Idempotent. Logs go to $SEED_LOG_DIR.
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
cd "$ROOT"
LOG_DIR="${SEED_LOG_DIR:-.local/logs}"
mkdir -p "$LOG_DIR"

if [ "$(ls scenarios/s*.json 2>/dev/null | wc -l)" -lt 200 ]; then
  sim gen-scenarios --n 200 --seed 42 --out scenarios
fi

sim batch --scenarios scenarios --config configs/open_loop.yaml --workers 8 --batch-id local-001 \
  2>&1 | tee "$LOG_DIR/batch-local-001.log"
sim bench-scaling --scenarios scenarios --config configs/open_loop.yaml --batch-id local-001 \
  --workers 1,2,4,8 2>&1 | tee "$LOG_DIR/bench-scaling.log"
cp results/local-001/scaling.json "$LOG_DIR/scaling.json"

rm -rf results/demo
for p in car van robot; do
  sim run --scenario scenarios/s0002.json --config configs/closed_loop.yaml --platform "$p" --batch-id demo
done
sim diff --onroad s0002 --platform car --batch-id demo 2>&1 | tee "$LOG_DIR/diff-default.log"
sim diff --onroad s0002 --platform van --batch-id demo --inject sensor_dropout@tick=40 2>&1 \
  | tee "$LOG_DIR/diff-injected.log"
sim diff --onroad s0002 --platform van --batch-id demo 2>&1 | tee "$LOG_DIR/diff-van-match.log"
sim profile --run results/demo/s0002_car_closed_loop.json 2>&1 | tee "$LOG_DIR/profile.log"
sim repro-check --scenario s0002 --repeats 5 --workers 1,4 --config configs/closed_loop.yaml \
  2>&1 | tee "$LOG_DIR/repro-check.log"
sim index-batches
echo "seed complete: results/ is ready for the viewer"
