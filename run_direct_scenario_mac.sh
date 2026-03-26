#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$ROOT_DIR/runs/02_direct/local_direct_mac.sh" "$@"
