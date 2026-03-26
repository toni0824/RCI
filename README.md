# OWR - Overlay With Routing

Implementacao em C do projeto de RCI. O executavel `OWR` cria um no de uma rede sobreposta com:

- registo UDP num servidor de nos (`REG`, `NODES`, `CONTACT`)
- ligacoes TCP entre vizinhos (`NEIGHBOR`)
- encaminhamento com `ROUTE`, `COORD`, `UNCOORD`
- envio de mensagens `CHAT`
- modo direto sem servidor (`direct join`, `direct add edge`)

## Estado do projeto

O que esta implementado e validado:

- comandos da parte intermedia: `join`, `leave`, `show nodes`, `exit`, `add edge`, `remove edge`, `show neighbors`
- comandos da parte final: `announce`, `show routing`, `start monitor`, `end monitor`, `message`
- modo sem servidor: `direct join`, `direct add edge`
- protocolos pedidos no enunciado: `REG`, `NODES`, `CONTACT`, `NEIGHBOR`, `ROUTE`, `COORD`, `UNCOORD`, `CHAT`
- cenario final dificil validado localmente e no `tejo`, com convergencia final para `INF`

Resultado esperado do cenario dificil:

```text
ROUTING 10 state=exp distance=INF next=-
```

## Estrutura do repositorio

```text
Proj1/
|-- main.c
|-- tcp.c
|-- tcp.h
|-- udp.c
|-- udp.h
|-- 59000.py
|-- Makefile
|-- README.md
`-- runs/
    |-- 01_basic_tejo/
    |   `-- tejo_basic_mac.sh
    |-- 02_direct/
    |   `-- local_direct_mac.sh
    |-- 03_intermediate/
    |   |-- local_intermediate_mac.sh
    |   `-- tejo_intermediate_mac.sh
    |-- 04_message/
    |   |-- local_message_mac.sh
    |   `-- tejo_message_mac.sh
    `-- 05_final/
        |-- local_final_mac.sh
        `-- tejo_final_mac.sh
```

## Compilacao

```bash
make
```

Executavel gerado:

```bash
./OWR
```

Limpeza:

```bash
make clean
```

## Execucao manual

Formato:

```bash
./OWR <IP_local> <porto_TCP> [regIP regUDP]
```

Exemplo local:

```bash
./OWR 127.0.0.1 58001 127.0.0.1 59000
```

Exemplo com servidor oficial:

```bash
./OWR 10.19.233.157 58001 193.136.138.142 58861
```

## Comandos suportados

### Rede

- `join <net> <id>`
- `direct join <net> <id>`
- `leave`
- `show nodes <net>`
- `exit`

### Topologia

- `add edge <id>`
- `direct add edge <id> <ip> <porto>`
- `remove edge <id>`
- `show neighbors`

### Encaminhamento e chat

- `announce`
- `show routing <dest>`
- `start monitor`
- `end monitor`
- `message <dest> <texto>`

### Abreviaturas aceites

- `j`, `dj`, `l`, `x`, `n`, `sg`, `ae`, `dae`, `re`, `a`, `sr`, `sm`, `em`, `m`

## Ficheiros principais

- `main.c` - parser de comandos, ciclo com `select()`, comandos de utilizador
- `udp.c` - comunicacao UDP com o servidor de nos
- `tcp.c` - vizinhos TCP, routing, coordenacao e chat
- `59000.py` - servidor de nos simples para testes locais
- `runs/` - scripts de teste organizados por progressao/dificuldade

## Testes locais no macOS

Os scripts locais usam `osascript` para abrir janelas do Terminal e `59000.py` como servidor UDP local.

### 1. Modo direto

```bash
./runs/02_direct/local_direct_mac.sh
```

Cobre:

- `direct join`
- `direct add edge`
- `show neighbors`
- `leave`

### 2. Parte intermedia

```bash
./runs/03_intermediate/local_intermediate_mac.sh
```

Cobre:

- `join`
- `show nodes`
- `add edge`
- `show neighbors`
- `remove edge`
- `leave`

### 3. Rotas e mensagens

```bash
./runs/04_message/local_message_mac.sh
```

Cobre:

- `announce`
- `show routing`
- `message`

### 4. Cenario final completo

```bash
./runs/05_final/local_final_mac.sh
```

Cobre:

- linha `01-02-03-04`
- fecho do anel
- no artificial `99` com `nc`
- `COORD` / `UNCOORD`
- isolamento do destino
- convergencia final para `INF`

## Testes no tejo

No `tejo`, a sessao e ativada manualmente. O `README` assume o fluxo oficial do enunciado.

### 1. Abrir sessao

```bash
echo "106:A" | nc tejo.tecnico.ulisboa.pt 59011 > init.html
```

Depois ler em `init.html`:

- `CONTACT PORT` do node server
- portos UDP dos nos residentes
- `SESSION ACCESS CODE`

### 2. Scripts de teste no tejo

#### Basico

```bash
./runs/01_basic_tejo/tejo_basic_mac.sh <IP_local> <TCP_local> <regUDP> <res_id> <res_udp> <grupo> <id_local> <session_code>
```

Exemplo:

```bash
./runs/01_basic_tejo/tejo_basic_mac.sh 10.19.233.157 58001 58861 10 58862 106 01 2600408
```

#### Intermedio

```bash
./runs/03_intermediate/tejo_intermediate_mac.sh 10.19.233.157 58001 58861 10 58862 106 01 2600408
```

#### Mensagem

```bash
./runs/04_message/tejo_message_mac.sh 10.19.233.157 58001 58861 10 58862 106 01 2600408
```

#### Final

```bash
./runs/05_final/tejo_final_mac.sh 10.19.233.157 58004 58861 106 40 10 58862 20 58863 30 58864 2600408
```

### 3. Fechar sessao

```bash
printf "FIN2600408\n" | nc tejo.tecnico.ulisboa.pt 59011 > rep.html
```

## Cenario final dificil

Cenario validado:

1. criar a linha `10-20-30-40` ou localmente `01-02-03-04`
2. anunciar o destino inicial
3. fechar o anel
4. injetar um no artificial `99` com `nc`
5. remover a aresta direta para o destino
6. remover a aresta restante que isola o destino
7. enviar `UNCOORD`
8. verificar convergencia final para `INF`

Resultado esperado:

```text
ROUTING 10 state=exp distance=INF next=-
```

Isto confirma que:

- o destino ficou inalcancavel
- o no sai de coordenacao
- nao fica uma rota residual ou ciclo entre vizinhos

## Validacao feita

### Local

- direto: ok
- intermedio: ok
- mensagem: ok
- final: ok

### Tejo

- basico: ok
- intermedio: ok
- mensagem: ok
- final dificil: ok

## Extra: Playground UI

Existe tambem uma pasta isolada para demonstracao local em browser:

```text
playground_ui/
```

Serve para:

- lancar os scripts ja existentes por botao
- ver logs dos runs no browser
- arrancar/parar capturas com `tshark` ou `tcpdump`

Arranque rapido:

```bash
python3 playground_ui/server.py
```

Depois abrir:

```text
http://127.0.0.1:8787
```

## Observacoes

- Os scripts em `runs/` foram organizados por progressao para facilitar demonstracao e regressao.
- Os scripts locais usam `127.0.0.1` por omissao e o servidor local faz bind em `0.0.0.0` para evitar problemas ao mudar de rede.
- Os scripts do `tejo` assumem que a sessao ja foi aberta manualmente e que os parametros foram copiados do `init.html`.
- Se a sessao do `tejo` ficar bloqueada, e necessario fechar com `FIN<codigo>` ou esperar o timeout da plataforma.
