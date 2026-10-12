# nw-port

Port do New World para Unreal Engine 5.8.3, para o clã jogar num servidor próprio. Os assets saem da instalação
do New World de cada um: nada do jogo entra neste repositório.

## Começar

| Para | Leia |
|---|---|
| montar tudo do zero (ferramentas, extração, Unreal) | [docs/SETUP.md](docs/SETUP.md) |
| saber o que funciona, o que falta e as armadilhas | [docs/ESTADO.md](docs/ESTADO.md) |
| o território de Guardaventos (coleta, cenário, criaturas) | [docs/guardaventos.md](docs/guardaventos.md) |
| servidor, backend e fases | [docs/plano-servidor.md](docs/plano-servidor.md) |
| combate e mecânicas | [docs/plano-mecanicas.md](docs/plano-mecanicas.md) |
| pendências de polimento | [docs/pendencias.md](docs/pendencias.md) |
| como cada asset sai do jogo (nwbt, áudio, animações) | [docs/ASSETS.md](docs/ASSETS.md) |

Resumo do setup, depois das ferramentas instaladas:

```bash
tools/build_all.sh         # extrai e converte da sua instalação do New World (2 a 4 h)
tools/ue_import_all.sh     # monta o projeto Unreal (editor fechado, ~1 h 30)
```

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `unreal/NWPort` | projeto Unreal (C++ em `Source/`, configuração em `Config/`) |
| `tools/` | extração e conversão; `tools/ue/` roda dentro da Unreal |
| `backend/` | nw-core (Rust + Postgres): contas, personagens, inventário |
| `deploy/` | Docker do servidor dedicado e do backend |
| `patches/` | diff do nw-buddy; o código fica em `paulorsouza/nw-buddy`, branch `nw-port` |
| `docs/` | documentação |

Fora do git (gerados): `extract/`, `vendor/`, `bin/`, `unreal/NWPort/{Content,Data,Binaries,Intermediate,Saved}`.
