#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

python3 "$ROOT_DIR/validate_tree_depth3.py" \
  --runs 4500 \
  --field-seed 20260407 \
  --skip-build \
  "$@"