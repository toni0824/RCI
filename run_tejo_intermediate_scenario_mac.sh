#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
TMP_DIR="$ROOT_DIR/.scenario_tmp"

LOCAL_IP="${1:-10.19.233.157}"
LOCAL_TCP="${2:-58001}"
REG_UDP="${3:-58861}"
RESIDENT_ID="${4:-10}"
RESIDENT_UDP="${5:-58862}"
GROUP_ID="${6:-106}"
LOCAL_ID="${7:-01}"
SESSION_CODE="${8:-2589460}"

mkdir -p "$TMP_DIR"
make -C "$ROOT_DIR"

cat > "$TMP_DIR/tejo_inter_local.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join $GROUP_ID $LOCAL_ID"
  sleep 2
  echo "show nodes $GROUP_ID"
  sleep 2
  echo "add edge $RESIDENT_ID"
  sleep 3
  echo "show neighbors"
  sleep 8
  echo "remove edge $RESIDENT_ID"
  sleep 2
  echo "show neighbors"
  sleep 3
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$LOCAL_IP" "$LOCAL_TCP" 193.136.138.142 "$REG_UDP"
echo
echo "[tejo intermediate local] scenario finished"
exec zsh
EOF

cat > "$TMP_DIR/tejo_inter_remote.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
echo "[tejo intermediate remote] resident node $RESIDENT_ID on UDP $RESIDENT_UDP"
sleep 6
printf "show neighbors\n" | nc -u -w 1 tejo.tecnico.ulisboa.pt "$RESIDENT_UDP" || true
sleep 4
printf "show neighbors\n" | nc -u -w 1 tejo.tecnico.ulisboa.pt "$RESIDENT_UDP" || true
sleep 4
printf "RP$SESSION_CODE $RESIDENT_UDP\n" | nc -w 1 tejo.tecnico.ulisboa.pt 59011 > "$ROOT_DIR/tejo_rep_intermediate_${RESIDENT_UDP}.html" || true
echo "[tejo intermediate remote] report saved to tejo_rep_intermediate_${RESIDENT_UDP}.html"
exec zsh
EOF

chmod +x "$TMP_DIR"/tejo_inter_*.sh

osascript <<EOF
tell application "Terminal"
    activate
    do script quoted form of POSIX path of "$TMP_DIR/tejo_inter_local.sh"
    delay 0.5
    do script quoted form of POSIX path of "$TMP_DIR/tejo_inter_remote.sh"
end tell
EOF

echo "Tejo intermediate scenario launched."
echo "Local node: $LOCAL_ID at $LOCAL_IP:$LOCAL_TCP"
echo "Resident node: $RESIDENT_ID on UDP $RESIDENT_UDP"
echo "Group: $GROUP_ID | Session code: $SESSION_CODE"
