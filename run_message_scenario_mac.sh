#!/bin/zsh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
TMP_DIR="$ROOT_DIR/.scenario_tmp"
IP="${1:-127.0.0.1}"
REG_PORT=59000
P1=58001
P2=58002
P3=58003
P4=58004

mkdir -p "$TMP_DIR"
make -C "$ROOT_DIR"

cat > "$TMP_DIR/msg_server.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
echo "[server] starting UDP registry on $IP:$REG_PORT"
python3 59000.py "0.0.0.0" "$REG_PORT"
EOS

cat > "$TMP_DIR/msg_t1.sh" <<EOS
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
  sleep 14
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P1" "$IP" "$REG_PORT"
echo
 echo "[message t1] scenario finished"
exec zsh
EOS

cat > "$TMP_DIR/msg_t2.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join 001 02"
  sleep 1
  echo "start monitor"
  sleep 7
  echo "add edge 03"
  sleep 16
  echo "show routing 01"
  sleep 6
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P2" "$IP" "$REG_PORT"
echo
 echo "[message t2] scenario finished"
exec zsh
EOS

cat > "$TMP_DIR/msg_t3.sh" <<EOS
#!/bin/zsh
cd "$ROOT_DIR"
{
  sleep 1
  echo "join 001 03"
  sleep 1
  echo "start monitor"
  sleep 10
  echo "add edge 04"
  sleep 13
  echo "show routing 01"
  sleep 6
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P3" "$IP" "$REG_PORT"
echo
 echo "[message t3] scenario finished"
exec zsh
EOS

cat > "$TMP_DIR/msg_t4.sh" <<EOS
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
  sleep 6
  echo "show routing 01"
  sleep 6
  echo "leave"
  sleep 1
  echo "exit"
} | ./OWR "$IP" "$P4" "$IP" "$REG_PORT"
echo
 echo "[message t4] scenario finished"
exec zsh
EOS

chmod +x "$TMP_DIR"/msg_*.sh

osascript <<EOS
 tell application "Terminal"
     activate
     do script quoted form of POSIX path of "$TMP_DIR/msg_server.sh"
     delay 1
     do script quoted form of POSIX path of "$TMP_DIR/msg_t1.sh"
     delay 0.5
     do script quoted form of POSIX path of "$TMP_DIR/msg_t2.sh"
     delay 0.5
     do script quoted form of POSIX path of "$TMP_DIR/msg_t3.sh"
     delay 0.5
     do script quoted form of POSIX path of "$TMP_DIR/msg_t4.sh"
 end tell
EOS

echo "Message scenario launched."
echo "IP: $IP | UDP: $REG_PORT | TCP: $P1 $P2 $P3 $P4"
