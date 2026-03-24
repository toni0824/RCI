#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
TMP_DIR="$ROOT_DIR/.scenario_tmp"
IP="${1:-10.19.5.49}"
REG_PORT=59000
P1=58001
P2=58002
P3=58003

mkdir -p "$TMP_DIR"
make -C "$ROOT_DIR"

cat > "$TMP_DIR/inter_server.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
echo "[server] starting UDP registry on $IP:$REG_PORT"
python3 59000.py "$IP" "$REG_PORT"
EOS

cat > "$TMP_DIR/inter_t1.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join 001 01"
  sleep 3
  echo "show nodes 001"
  sleep 4
  echo "add edge 02"
  sleep 4
  echo "show neighbors"
  sleep 6
  echo "remove edge 02"
  sleep 3
  echo "show neighbors"
  sleep 4
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P1" "$IP" "$REG_PORT"
echo
 echo "[intermediate t1] scenario finished"
exec zsh
EOS

cat > "$TMP_DIR/inter_t2.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join 001 02"
  sleep 4
  echo "show nodes 001"
  sleep 10
  echo "add edge 03"
  sleep 4
  echo "show neighbors"
  sleep 10
  echo "remove edge 03"
  sleep 3
  echo "show neighbors"
  sleep 4
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P2" "$IP" "$REG_PORT"
echo
 echo "[intermediate t2] scenario finished"
exec zsh
EOS

cat > "$TMP_DIR/inter_t3.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join 001 03"
  sleep 7
  echo "show nodes 001"
  sleep 14
  echo "show neighbors"
  sleep 8
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P3" "$IP" "$REG_PORT"
echo
 echo "[intermediate t3] scenario finished"
exec zsh
EOS

chmod +x "$TMP_DIR"/inter_*.sh

osascript <<EOS
 tell application "Terminal"
     activate
     do script quoted form of POSIX path of "$TMP_DIR/inter_server.sh"
     delay 1
     do script quoted form of POSIX path of "$TMP_DIR/inter_t1.sh"
     delay 0.5
     do script quoted form of POSIX path of "$TMP_DIR/inter_t2.sh"
     delay 0.5
     do script quoted form of POSIX path of "$TMP_DIR/inter_t3.sh"
 end tell
EOS

echo "Intermediate scenario launched."
echo "IP: $IP | UDP: $REG_PORT | TCP: $P1 $P2 $P3"
