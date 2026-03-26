# OWR Playground UI

Isto e uma camada extra para brincar com o projeto sem mexer na logica principal do `Proj1`.

O playground faz tres coisas:

- arranca os scripts de teste que ja existem em `Proj1/runs/`
- mostra logs dos runs numa pagina local em `localhost`
- permite arrancar e parar capturas com `tshark` ou `tcpdump`, se estiverem instalados

## Estrutura

```text
playground_ui/
|-- server.py
|-- README.md
|-- static/
|   |-- index.html
|   |-- app.js
|   `-- styles.css
`-- runtime/
    |-- logs/
    `-- captures/
```

## Arranque

Na pasta `Proj1`:

```bash
cd "/Users/antoniofernandes/Documents/Tecnico/3 ano/RCI/Proj1"
python3 playground_ui/server.py
```

Abrir no browser:

```text
http://127.0.0.1:8787
```

## O que a UI faz

### Cenarios

Cada card corresponde a um script real em `runs/`.

- `local_direct`
- `local_intermediate`
- `local_message`
- `local_final`
- `tejo_basic`
- `tejo_intermediate`
- `tejo_message`
- `tejo_final`

A UI so arranca o script e guarda o stdout/stderr num ficheiro em `playground_ui/runtime/logs/`.

### Captura

Se existir `tshark` ou `tcpdump` no sistema, a UI consegue arrancar uma captura com filtro configuravel e gravar um `.pcap` em:

```text
playground_ui/runtime/captures/
```

Isto nao substitui o Wireshark. A ideia e:

- arrancar a captura no dashboard
- correr um cenario
- abrir o `.pcap` depois no Wireshark

### Logs

A UI lista runs, permite parar um run e mostra o log associado.

## Limites desta versao

- e uma ferramenta de demonstracao, nao faz parte da entrega formal
- os scripts continuam a ser os mesmos, incluindo os que usam `osascript` no macOS
- a UI nao interpreta o protocolo; apenas arranca cenarios, mostra logs e guarda capturas
- os cenarios do `tejo` continuam a precisar que a sessao seja aberta manualmente e que os parametros certos sejam introduzidos

## Extensoes obvias

Se quiseres evoluir isto, o passo seguinte e deixar de depender dos scripts com `osascript` e criar runners headless para:

- arranque de varios `OWR` por subprocesso
- envio programatico de comandos
- timeline mais precisa do cenario
- parsing dos logs `ROUTE`, `COORD`, `UNCOORD`, `CHAT`
