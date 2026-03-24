#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
TMP_DIR="$ROOT_DIR/.scenario_tmp"
IP="${1:-10.19.5.49}"
REG_PORT=59000
P1=58001
P2=58002
P3=58003
P4=58004

mkdir -p "$TMP_DIR"

make -C "$ROOT_DIR"

cat > "$TMP_DIR/run_server.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
echo "[server] starting UDP registry on $IP:$REG_PORT"
python3 59000.py "$IP" "$REG_PORT"
EOF

cat > "$TMP_DIR/run_t1.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join 001 01"
  sleep 1
  echo "start monitor"
  sleep 4
  echo "add edge 02"
  sleep 8
  echo "announce"
  sleep 8
  echo "add edge 04"
  sleep 3
  echo "show neighbors"
  sleep 8
  echo "remove edge 04"
  sleep 1
  echo "remove edge 02"
  sleep 10
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P1" "$IP" "$REG_PORT"
echo
echo "[t1] scenario finished"
exec zsh
EOF

cat > "$TMP_DIR/run_t2.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join 001 02"
  sleep 1
  echo "start monitor"
  sleep 7
  echo "add edge 03"
  sleep 20
  echo "show routing 01"
  sleep 10
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P2" "$IP" "$REG_PORT"
echo
echo "[t2] scenario finished"
exec zsh
EOF

cat > "$TMP_DIR/run_t3.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join 001 03"
  sleep 1
  echo "start monitor"
  sleep 10
  echo "add edge 04"
  sleep 18
  echo "show routing 01"
  sleep 10
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P3" "$IP" "$REG_PORT"
echo
echo "[t3] scenario finished"
exec zsh
EOF

cat > "$TMP_DIR/run_t4.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join 001 04"
  sleep 1
  echo "start monitor"
  sleep 16
  echo "show routing 01"
  sleep 1
  echo "message 01 ola"
  sleep 8
  echo "show neighbors"
  sleep 1
  echo "show routing 01"
  sleep 6
  echo "show neighbors"
  sleep 6
  echo "show routing 01"
  sleep 8
  echo "show routing 01"
  sleep 10
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P4" "$IP" "$REG_PORT"
echo
echo "[t4] scenario finished"
exec zsh
EOF

cat > "$TMP_DIR/run_nc.sh" <<EOF
#!/bin/zsh
cd "$ROOT_DIR"
echo "[nc] waiting before connecting to node 04"
{
  sleep 27
  echo "NEIGHBOR 99"
  sleep 13
  echo "UNCOORD 01"
  sleep 5
} | nc "$IP" "$P4"
echo
echo "[nc] scenario finished"
exec zsh
EOF

chmod +x "$TMP_DIR"/run_*.sh

osascript <<EOF
tell application "Terminal"
    activate
    do script quoted form of POSIX path of "$TMP_DIR/run_server.sh"
    delay 1
    do script quoted form of POSIX path of "$TMP_DIR/run_t1.sh"
    delay 0.5
    do script quoted form of POSIX path of "$TMP_DIR/run_t2.sh"
    delay 0.5
    do script quoted form of POSIX path of "$TMP_DIR/run_t3.sh"
    delay 0.5
    do script quoted form of POSIX path of "$TMP_DIR/run_t4.sh"
    delay 0.5
    do script quoted form of POSIX path of "$TMP_DIR/run_nc.sh"
end tell
EOF

echo "Scenario launched."
echo "IP: $IP"
echo "UDP: $REG_PORT"
echo "TCP: $P1 $P2 $P3 $P4"
echo "Temporary scripts: $TMP_DIR"
