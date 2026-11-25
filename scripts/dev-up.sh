#!/usr/bin/env bash
# Build the core (in Docker), generate data, start the viewer at http://localhost:5173
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
cd "$ROOT"
mkdir -p .local
bash scripts/build-core.sh
bash scripts/seed.sh
(cd viewer && npm ci --silent)
(cd viewer && nohup node node_modules/vite/bin/vite.js --port 5173 --host 127.0.0.1 >"$ROOT/.local/viewer.log" 2>&1 & echo $! >"$ROOT/.local/viewer.pid")
echo "viewer starting at http://localhost:5173 (log: .local/viewer.log)"
