import socket
import sys


HOST = "0.0.0.0"
PORT = 59000


def main() -> None:
    host = sys.argv[1] if len(sys.argv) >= 2 else HOST
    port = int(sys.argv[2]) if len(sys.argv) >= 3 else PORT

    db = {}

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((host, port))

    print(f"UDP server em {host}:{port}")
    print("TCP ports esperados para testes: 58001, 58002, 58003, 58004")

    while True:
        data, addr = sock.recvfrom(4096)
        msg = data.decode(errors="ignore").strip()
        print("recv:", msg, "from", addr)

        parts = msg.split()
        if len(parts) < 3:
            sock.sendto(b"ERR 000 9\n", addr)
            continue

        kind = parts[0]

        if kind == "NODES":
            if len(parts) < 4:
                sock.sendto(b"NODES 000 9 000\n", addr)
                continue

            tid, op, net = parts[1], parts[2], parts[3]
            if op == "0":
                ids = sorted(db.get(net, {}).keys())
                resp = f"NODES {tid} 1 {net}\n"
                for node_id in ids:
                    resp += f"{node_id}\n"
            else:
                resp = f"NODES {tid} 9 {net}\n"
            sock.sendto(resp.encode(), addr)
            continue

        if kind == "CONTACT":
            if len(parts) < 5:
                sock.sendto(b"CONTACT 000 9 000 00\n", addr)
                continue

            tid, op, net, node_id = parts[1], parts[2], parts[3], parts[4]
            if op == "0":
                if net in db and node_id in db[net]:
                    ip, tcp = db[net][node_id]
                    resp = f"CONTACT {tid} 1 {net} {node_id} {ip} {tcp}\n"
                else:
                    resp = f"CONTACT {tid} 2 {net} {node_id}\n"
            else:
                resp = f"CONTACT {tid} 9 {net} {node_id}\n"
            sock.sendto(resp.encode(), addr)
            continue

        if kind == "REG":
            if len(parts) < 5:
                sock.sendto(b"REG 000 9 000 00\n", addr)
                continue

            tid, op, net, node_id = parts[1], parts[2], parts[3], parts[4]
            if op == "0":
                if len(parts) < 7:
                    resp = f"REG {tid} 9 {net} {node_id}\n"
                else:
                    ip, tcp = parts[5], parts[6]
                    db.setdefault(net, {})
                    db[net][node_id] = (ip, tcp)
                    resp = f"REG {tid} 1 {net} {node_id}\n"
            elif op == "3":
                if net in db and node_id in db[net]:
                    del db[net][node_id]
                    if not db[net]:
                        del db[net]
                resp = f"REG {tid} 4 {net} {node_id}\n"
            else:
                resp = f"REG {tid} 9 {net} {node_id}\n"

            sock.sendto(resp.encode(), addr)
            continue

        sock.sendto(b"ERR 000 9\n", addr)


if __name__ == "__main__":
    main()
