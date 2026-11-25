#!/usr/bin/env bash
# Run every local test layer: ctest, ruff, pytest, vitest, playwright. Exits non-zero on failure.
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
cd "$ROOT"
OUT="${TEST_OUT:-.local/test-reports}"
mkdir -p "$OUT"

bash scripts/build-core.sh "$OUT" 2>&1 | tee "$OUT/ctest.log" | tail -n 25
in_dev ruff check . 2>&1 | tee "$OUT/ruff.log"
in_dev python -m pytest -v --junitxml="$OUT/pytest-junit.xml" 2>&1 | tee "$OUT/pytest.log"

(cd viewer && npm ci --silent && npx tsc --noEmit)
(cd viewer && npx vitest run --reporter=default --reporter=junit --outputFile.junit="../$OUT/vitest-junit.xml") 2>&1 | tee "$OUT/vitest.log"

if [ ! -f results/demo/s0002_car_diff.diff.json ] || [ ! -f results/repro.json ]; then
  bash scripts/seed.sh >"$OUT/seed.log" 2>&1
fi
(cd viewer && PW_REPORT="$ROOT/$OUT/playwright-report" npx playwright test) 2>&1 | tee "$OUT/playwright.log"
echo "ALL TESTS PASSED (reports in $OUT)"
