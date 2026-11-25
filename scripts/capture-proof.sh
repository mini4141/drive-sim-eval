#!/usr/bin/env bash
# Capture proof artifacts for a phase. Usage: capture-proof.sh phase1 | phase2
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
cd "$ROOT"
phase="${1:?usage: capture-proof.sh phase1|phase2}"

redact() { sed -E 's/[0-9]{12}/<ACCOUNT_ID>/g; s/AKIA[0-9A-Z]{16}/<ACCESS_KEY>/g'; }

start_viewer() {
  (cd viewer && npm ci --silent)
  (cd viewer && node node_modules/vite/bin/vite.js --port 5173 --host 127.0.0.1 >"$ROOT/.local/viewer.log" 2>&1 &
   echo $! >"$ROOT/.local/viewer.pid")
  for _ in $(seq 1 30); do curl -sf http://127.0.0.1:5173/ >/dev/null && return 0; sleep 1; done
  echo "viewer did not start" >&2; return 1
}

case "$phase" in
  phase1)
    P=proof/phase1-local
    mkdir -p .local "$P"/{screenshots,logs,tests}
    SEED_LOG_DIR="$P/logs" bash scripts/seed.sh >"$P/logs/seed.log" 2>&1
    TEST_OUT="$P/tests" bash scripts/test-all.sh
    cp results/demo/s0002_car_closed_loop.trace.json "$P/logs/perfetto-trace.json"
    start_viewer
    (cd viewer && node capture.mjs http://127.0.0.1:5173 "$ROOT/$P/screenshots" phase1)
    bash scripts/dev-down.sh
    ;;
  phase2)
    P=proof/phase2-cloud
    URL="$(cat .local/cloud-url.txt)"
    BATCH="$(cat .local/cloud-batch.txt)"
    mkdir -p "$P"/{screenshots,logs,tests}
    (cd viewer && node capture.mjs "$URL" "$ROOT/$P/screenshots" phase2 "$BATCH")
    (cd viewer && VIEWER_URL="$URL" PW_REPORT="$ROOT/$P/tests/playwright-cloud" npx playwright test) \
      2>&1 | redact | tee "$P/tests/playwright-cloud.log"
    ;;
  *) echo "unknown phase" >&2; exit 2 ;;
esac
echo "proof captured for $phase"
