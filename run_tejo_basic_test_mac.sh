#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$ROOT_DIR/runs/01_basic_tejo/tejo_basic_mac.sh" "$@"
