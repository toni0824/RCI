#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
TMP_DIR="$ROOT_DIR/.scenario_tmp"
IP="${1:-127.0.0.1}"
P1=58001
P2=58002
P3=58003

mkdir -p "$TMP_DIR"
make -C "$ROOT_DIR"

cat > "$TMP_DIR/direct_t1.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "direct join 001 01"
  sleep 2
  echo "direct add edge 02 $IP $P2"
  sleep 4
  echo "show neighbors"
  sleep 8
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P1"
echo
 echo "[direct t1] scenario finished"
exec zsh
EOS

cat > "$TMP_DIR/direct_t2.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "direct join 001 02"
  sleep 5
  echo "direct add edge 03 $IP $P3"
  sleep 4
  echo "show neighbors"
  sleep 8
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P2"
echo
 echo "[direct t2] scenario finished"
exec zsh
EOS

cat > "$TMP_DIR/direct_t3.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "direct join 001 03"
  sleep 9
  echo "show neighbors"
  sleep 8
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P3"
echo
 echo "[direct t3] scenario finished"
exec zsh
EOS

chmod +x "$TMP_DIR"/direct_*.sh

osascript <<EOS
 tell application "Terminal"
     activate
     do script quoted form of POSIX path of "$TMP_DIR/direct_t1.sh"
     delay 0.5
     do script quoted form of POSIX path of "$TMP_DIR/direct_t2.sh"
     delay 0.5
     do script quoted form of POSIX path of "$TMP_DIR/direct_t3.sh"
 end tell
EOS

echo "Direct scenario launched."
echo "IP: $IP | TCP: $P1 $P2 $P3"
