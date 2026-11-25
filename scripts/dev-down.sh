#!/usr/bin/env bash
# Stop the local viewer started by dev-up.sh / capture-proof.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"
if command -v powershell >/dev/null 2>&1; then
  # On Windows the bash PID is not the node PID: stop whatever listens on the viewer port.
  powershell -NoProfile -Command 'Get-NetTCPConnection -LocalPort 5173 -State Listen -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.OwningProcess -Force }' || true
elif [ -f "$ROOT/.local/viewer.pid" ]; then
  kill "$(cat "$ROOT/.local/viewer.pid")" 2>/dev/null || true
fi
rm -f "$ROOT/.local/viewer.pid"
echo "viewer stopped"
