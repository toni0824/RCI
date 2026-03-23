# RCI - Proj1

Implementacao em C do projeto de RCI `OWR` (Overlay With Routing).

Estado atual:
- comandos da avaliacao intermedia: `join`, `leave`, `show nodes`, `exit`, `add edge`, `remove edge`, `show neighbors`
- comandos finais base: `announce`, `show routing`, `start monitor`, `end monitor`, `message`
- suporte a `direct join` e `direct add edge` para testes sem servidor de nos
- ligacoes TCP reais entre nos
- pedidos UDP ao servidor de nos (`REG`, `NODES`, `CONTACT`)

## Compilar

```bash
make
```

Binario gerado:

```bash
./OWR
```

Limpar:

```bash
make clean
```

## Executar

Formato:

```bash
./OWR <IP_local> <porto_TCP> [regIP regUDP]
```

Exemplo com servidor por omissao:

```bash
./OWR 127.0.0.1 5000
```

Exemplo com servidor explicito:

```bash
./OWR 127.0.0.1 5000 193.136.138.142 59000
```

## Comandos suportados

Participacao na rede:
- `join <net> <id>`
- `direct join <net> <id>`
- `leave`
- `show nodes <net>`
- `exit`

Topologia:
- `add edge <id>`
- `direct add edge <id> <ip> <porto>`
- `remove edge <id>`
- `show neighbors`

Encaminhamento e chat:
- `announce`
- `show routing <dest>`
- `start monitor`
- `end monitor`
- `message <dest> <texto>`

Atalhos aceites pelo parser:
- `j`, `dj`, `l`, `x`, `n`, `sg`, `ae`, `dae`, `re`, `a`, `sr`, `sm`, `em`, `m`

## Teste local rapido

Abrir 4 terminais na pasta `Proj1`:

```bash
./OWR 127.0.0.1 5001
./OWR 127.0.0.1 5002
./OWR 127.0.0.1 5003
./OWR 127.0.0.1 5004
```

Em cada terminal:

```text
no 01: direct join 001 01
no 02: direct join 001 02
no 03: direct join 001 03
no 04: direct join 001 04
```

Criar topologia linear:

```text
no 01: direct add edge 02 127.0.0.1 5002
no 02: direct add edge 03 127.0.0.1 5003
no 03: direct add edge 04 127.0.0.1 5004
```

Anunciar o destino e verificar rota:

```text
no 01: announce
no 04: show routing 01
```

Enviar mensagem:

```text
no 04: message 01 ola
```

## Estrutura principal

- `main.c` - parser de comandos, ciclo com `select()`, `join/leave/show`
- `tcp.c` - sockets TCP, vizinhos, `NEIGHBOR`, `ROUTE`, `COORD`, `UNCOORD`, `CHAT`
- `udp.c` - pedidos UDP ao servidor de nos
- `tcp.h` / `udp.h` - estruturas e prototipos

## Notas

- Em modo `direct join`, o `show nodes` mostra o proprio no e os vizinhos ligados, sem usar o servidor.
- O encaminhamento implementa anuncios de rota, reencaminhamento de chat e uma coordenacao base com `COORD/UNCOORD`.
- Para testes reais em varios PCs da mesma rede, usar o IP real de cada maquina e portos TCP diferentes.
