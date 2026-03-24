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

## Cenario final validado

O cenario seguinte foi usado para validar a parte final do projeto:

1. criar a linha `01-02-03-04`
2. anunciar o no `01`
3. fechar o anel com a aresta `01-04`
4. criar um no artificial `99` ligado ao no `04` com `nc`
5. remover as arestas `01-04` e `01-02`
6. enviar `UNCOORD 01` na sessao `nc`
7. verificar a convergencia final

### Arranque

Servidor UDP:

```bash
python3 59000.py 10.19.5.49 59000
```

Nos:

```bash
./OWR 10.19.5.49 58001 10.19.5.49 59000
./OWR 10.19.5.49 58002 10.19.5.49 59000
./OWR 10.19.5.49 58003 10.19.5.49 59000
./OWR 10.19.5.49 58004 10.19.5.49 59000
```

### Sequencia de comandos

Nos 4 terminais:

```text
T1: join 001 01
T1: start monitor

T2: join 001 02
T2: start monitor

T3: join 001 03
T3: start monitor

T4: join 001 04
T4: start monitor
```

Criar a linha:

```text
T1: add edge 02
T2: add edge 03
T3: add edge 04
```

Anunciar e testar:

```text
T1: announce
T4: show routing 01
T4: message 01 ola
```

Fechar o anel:

```text
T1: add edge 04
T4: show routing 01
```

Criar o no artificial:

```bash
nc 10.19.5.49 58004
```

Na sessao `nc`:

```text
NEIGHBOR 99
```

Remover arestas:

```text
T1: remove edge 04
T1: remove edge 02
```

Terminar a coordenacao na sessao `nc`:

```text
UNCOORD 01
```

### Resultado esperado

Depois da convergencia, o no `04` deve ficar sem rota valida para `01`:

```text
ROUTING 01 state=exp distance=INF next=-
```

Isto indica que o no `01` ficou isolado e que nao existe ciclo de encaminhamento residual entre `03` e `04`.

### Script automatico no macOS

Existe um script para abrir o Terminal e correr automaticamente este cenario:

```bash
./run_final_scenario_mac.sh
```

Ou com IP explicito:

```bash
./run_final_scenario_mac.sh 10.19.5.49
```

O script:
- compila o projeto com `make`
- abre uma janela para o servidor UDP
- abre 4 janelas para os nos `01`, `02`, `03` e `04`
- abre uma janela com `nc` para simular o no artificial `99`
- executa a sequencia completa de comandos com tempos de espera entre passos

Nota:
- o script usa `osascript` e foi pensado para o Terminal do macOS
- as janelas ficam abertas no fim com `exec zsh`

## Outros scripts de teste

Foram adicionados varios scripts para validar cenarios diferentes no macOS:

- `./run_intermediate_scenario_mac.sh`
  - testa a parte intermédia com servidor UDP
  - cobre `join`, `show nodes`, `add edge`, `show neighbors`, `remove edge`, `leave`

- `./run_direct_scenario_mac.sh`
  - testa o modo sem servidor
  - cobre `direct join`, `direct add edge`, `show neighbors`, `leave`

- `./run_message_scenario_mac.sh`
  - testa propagacao de rotas e envio de mensagens
  - cobre `announce`, `show routing`, `message`

- `./run_final_scenario_mac.sh`
  - testa o cenario final completo com coordenacao
  - cobre anel, no artificial `99`, `COORD`, `UNCOORD` e convergencia para `INF`

- `./run_tejo_basic_test_mac.sh`
  - testa uma sessao oficial no `tejo`
  - arranca o teu `OWR`, liga a um no residente e envia `announce` por `nc -u`
  - guarda um relatorio parcial HTML do no residente

Todos aceitam opcionalmente o IP da maquina:

```bash
./run_intermediate_scenario_mac.sh 10.19.5.49
./run_direct_scenario_mac.sh 10.19.5.49
./run_message_scenario_mac.sh 10.19.5.49
./run_final_scenario_mac.sh 10.19.5.49
```

Exemplo para a sessao do `tejo` mostrada no manual/`init.html`:

```bash
./run_tejo_basic_test_mac.sh 10.19.233.157 58001 58861 10 58862 106 01 2589460
```

Parametros do script do `tejo`:
- `IP_local`
- `porto_TCP_local`
- `porto_UDP_do_node_server`
- `id_do_no_residente`
- `porto_UDP_do_no_residente`
- `rede/grupo`
- `id_do_teu_no`
- `codigo_da_sessao`

## Estrutura principal

- `main.c` - parser de comandos, ciclo com `select()`, `join/leave/show`
- `tcp.c` - sockets TCP, vizinhos, `NEIGHBOR`, `ROUTE`, `COORD`, `UNCOORD`, `CHAT`
- `udp.c` - pedidos UDP ao servidor de nos
- `tcp.h` / `udp.h` - estruturas e prototipos

## Notas

- Em modo `direct join`, o `show nodes` mostra o proprio no e os vizinhos ligados, sem usar o servidor.
- O encaminhamento implementa anuncios de rota, reencaminhamento de chat e uma coordenacao base com `COORD/UNCOORD`.
- Para testes reais em varios PCs da mesma rede, usar o IP real de cada maquina e portos TCP diferentes.
