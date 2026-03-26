#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$ROOT_DIR/runs/03_intermediate/tejo_intermediate_mac.sh" "$@"
