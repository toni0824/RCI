#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
TMP_DIR="$ROOT_DIR/.scenario_tmp"

LOCAL_IP="${1:-10.19.233.157}"
LOCAL_TCP="${2:-58004}"
REG_UDP="${3:-58861}"
GROUP_ID="${4:-106}"
LOCAL_ID="${5:-40}"
ID10="${6:-10}"
UDP10="${7:-58862}"
ID20="${8:-20}"
UDP20="${9:-58863}"
ID30="${10:-30}"
UDP30="${11:-58864}"
SESSION_CODE="${12:-2600181}"

mkdir -p "$TMP_DIR"
make -C "$ROOT_DIR"

cat > "$TMP_DIR/tejo_final_local.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join $GROUP_ID $LOCAL_ID"
  sleep 1
  echo "start monitor"
  sleep 2
  echo "add edge $ID30"
  sleep 10
  echo "add edge $ID10"
  sleep 5
  echo "show routing $ID10"
  sleep 1
  echo "message $ID10 ola"
  sleep 12
  echo "remove edge $ID10"
  sleep 8
  echo "show routing $ID10"
  sleep 10
  echo "show routing $ID10"
  sleep 8
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$LOCAL_IP" "$LOCAL_TCP" 193.136.138.142 "$REG_UDP"
echo
echo "[tejo final local] scenario finished"
exec zsh
EOF

cat > "$TMP_DIR/tejo_final_remote.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
echo "[tejo final remote] residents: $ID10/$UDP10 $ID20/$UDP20 $ID30/$UDP30"
sleep 6
printf "announce\n" | nc -u -w 1 tejo.tecnico.ulisboa.pt "$UDP10" || true
sleep 24
printf "remove edge $ID20\n" | nc -u -w 1 tejo.tecnico.ulisboa.pt "$UDP10" || true
sleep 1
printf "remove edge $ID10\n" | nc -u -w 1 tejo.tecnico.ulisboa.pt "$UDP20" || true
sleep 5
printf "RP$SESSION_CODE $UDP10\n" | nc -w 1 tejo.tecnico.ulisboa.pt 59011 > "$ROOT_DIR/tejo_rep_final_${UDP10}.html" || true
echo "[tejo final remote] report saved to tejo_rep_final_${UDP10}.html"
exec zsh
EOF

cat > "$TMP_DIR/tejo_final_nc.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
echo "[tejo final nc] waiting for local node at $LOCAL_IP:$LOCAL_TCP"
until nc -z "$LOCAL_IP" "$LOCAL_TCP" >/dev/null 2>&1; do
  sleep 1
done
echo "[tejo final nc] local node is up"
{
  sleep 22
  echo "NEIGHBOR 99"
  sleep 22
  echo "UNCOORD $ID10"
  sleep 5
} | nc "$LOCAL_IP" "$LOCAL_TCP"
echo
echo "[tejo final nc] scenario finished"
exec zsh
EOF

chmod +x "$TMP_DIR"/tejo_final_*.sh

osascript <<EOF
tell application "Terminal"
    activate
    do script quoted form of POSIX path of "$TMP_DIR/tejo_final_local.sh"
    delay 0.5
    do script quoted form of POSIX path of "$TMP_DIR/tejo_final_remote.sh"
    delay 0.5
    do script quoted form of POSIX path of "$TMP_DIR/tejo_final_nc.sh"
end tell
EOF

echo "Tejo final scenario launched."
echo "Local node: $LOCAL_ID at $LOCAL_IP:$LOCAL_TCP"
echo "Node server: 193.136.138.142:$REG_UDP"
echo "Residents: $ID10/$UDP10 $ID20/$UDP20 $ID30/$UDP30"
echo "Group: $GROUP_ID | Session code: $SESSION_CODE"
