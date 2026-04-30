#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

python3 "$ROOT_DIR/tree_ga_depth3.py" \
    --agent-count 5 \
    --population 30 \
    --generations 30 \
    --field-count 10 \
    --trials-per-field 10 \
    --workers 4 \
  "$@"