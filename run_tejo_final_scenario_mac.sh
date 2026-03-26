#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
TMP_DIR="$ROOT_DIR/.scenario_tmp"

LOCAL_IP="${1:-10.19.233.157}"
LOCAL_TCP="${2:-58004}"
GROUP_ID="${3:-106}"
SETUP_TYPE="${4:-A}"
LOCAL_ID="${5:-40}"
ID10="${6:-10}"
ID20="${7:-20}"
ID30="${8:-30}"

mkdir -p "$TMP_DIR"
make -C "$ROOT_DIR"

INIT_HTML="$ROOT_DIR/init.html"
echo "$GROUP_ID:$SETUP_TYPE" | nc tejo.tecnico.ulisboa.pt 59011 > "$INIT_HTML"

REG_UDP="$(perl -ne 'print "$1\n" if /NODE SERVER AT IP:[^ ]+\s+PORT:(\d+)/' "$INIT_HTML" | head -n1)"
SESSION_CODE="$(perl -ne 'print "$1\n" if /SESSION ACCESS CODE:\s*(\d+)/' "$INIT_HTML" | head -n1)"
UDP10="$(perl -ne 'print "$1\n" if /\b'"$ID10"': \[[^ ]+ (\d+)\]/' "$INIT_HTML" | head -n1)"
UDP20="$(perl -ne 'print "$1\n" if /\b'"$ID20"': \[[^ ]+ (\d+)\]/' "$INIT_HTML" | head -n1)"
UDP30="$(perl -ne 'print "$1\n" if /\b'"$ID30"': \[[^ ]+ (\d+)\]/' "$INIT_HTML" | head -n1)"

if [[ -z "$REG_UDP" || -z "$SESSION_CODE" || -z "$UDP10" || -z "$UDP20" || -z "$UDP30" ]]; then
  echo "Falha ao ler dados da sessao em init.html"
  exit 1
fi

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
  sleep 5
  echo "show neighbors"
  sleep 8
  echo "show routing $ID10"
  sleep 1
  echo "message $ID10 ola"
  sleep 6
  echo "add edge $ID10"
  sleep 4
  echo "show routing $ID10"
  sleep 10
  echo "remove edge $ID10"
  sleep 10
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
sleep 18
printf "remove edge $ID20\n" | nc -u -w 1 tejo.tecnico.ulisboa.pt "$UDP10" || true
sleep 3
printf "RP$SESSION_CODE $UDP10\n" | nc -w 1 tejo.tecnico.ulisboa.pt 59011 > "$ROOT_DIR/tejo_rep_final_${UDP10}.html" || true
sleep 1
printf "FIN$SESSION_CODE\n" | nc -w 1 tejo.tecnico.ulisboa.pt 59011 > "$ROOT_DIR/rep.html" || true
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
  sleep 27
  echo "NEIGHBOR 99"
  sleep 18
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
echo "Group: $GROUP_ID | Setup: $SETUP_TYPE | Session code: $SESSION_CODE"
