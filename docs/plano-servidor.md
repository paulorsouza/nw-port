# Plano: servidor dedicado na nuvem para o clã

Objetivo: o clã (10–20 pessoas) jogando junto na South Windsward, com combate em rede sem atraso perceptível,
inventário, baú, mercador, crafting e compra de casa, tudo salvo num servidor na nuvem.

## Arquitetura

```
 cliente (Unreal, no PC de cada um) ──UDP 7777 (replicação do Unreal)──▶ servidor de jogo (Unreal dedicado, Linux)
        │                                                                     │
        └──HTTPS: login ─────────────────▶  backend (Rust)  ◀── API interna ──┘
                                              │
                                          Postgres
```

| Parte | Tecnologia | Responsabilidade |
|---|---|---|
| Servidor de jogo | Unreal 5.8 compilado do código-fonte, alvo **Server** (Linux, sem renderização) | Tempo real: movimento, colisão, combate, travessia, IA, projéteis, replicação. Reaproveita todo o C++ atual. |
| Backend | **Rust** (axum + sqlx), do zero | Fonte da verdade do que persiste: contas do clã, personagens, itens, moedas, baús, casas, receitas, chat, registro de transações. Regras de economia ficam aqui. |
| Banco | Postgres | Personagens, itens (instâncias), contêineres, casas, livro-razão de moedas. |
| Cliente | Unreal, gerado por cada jogador a partir da própria instalação do New World | Desenho, input, predição; nada de regra de jogo. |

Fluxos:
- **Entrar**: o cliente faz login no backend (convite do clã + senha) e recebe um token; conecta no servidor de
  jogo com o token; o servidor valida no backend e carrega o personagem.
- **Ação de economia** (guardar no baú, comprar casa, fabricar): o cliente pede ao servidor de jogo; o servidor
  confere o lado físico (está perto do baú/estação? está vivo?) e chama o backend, que aplica a regra numa
  transação do banco e devolve o novo estado; o servidor replica para o cliente.
- **Combate**: só no servidor de jogo; o backend recebe o resultado que importa (saque, morte, moedas).

Por que não o servidor inteiro em Rust do zero: teria que reescrever movimento com colisão na cidade, travessia,
acerto pela animação (lâmina e root motion), IA e projéteis, e trocar a rede do cliente por protocolo próprio
com predição — meses antes de voltar ao estado atual. No modelo híbrido o Rust cuida do mundo persistente; dá
para crescer depois (várias cidades = vários servidores de jogo no mesmo backend).

## Plataformas (decisão de 2026-10-11)

- **Cliente: Windows é a plataforma principal.** O clã joga no Windows (onde o New World está instalado). DX12
  com SM6: Nanite, Lumen e cache de PSO no caminho mais testado da Unreal.
- **Servidor de jogo: Linux** (nuvem e Docker).
- **Desenvolvimento: continua no Linux.** O cliente Linux (Vulkan SM6) segue funcionando para testes aqui.
- A Unreal não gera build de Windows a partir do Linux: o cliente Windows é empacotado numa máquina Windows
  (em montagem, semana de 2026-10-12). O que muda nas fases: F8 passa a ser o instalador/guia para Windows
  (extração da própria instalação + build do cliente lá), e os testes de desempenho valem no Windows.

## Idiomas (decisão de 2026-10-11)

Português (pt-BR) e inglês (en-US), só esses dois: dois jogadores do clã não falam bem português. Vale para
tudo: textos tirados do jogo (arquivos .loc.xml de cada idioma), telas nossas, mensagens do servidor e do
backend (o servidor manda chave + parâmetros; o cliente traduz).

## Nuvem

- Região São Paulo (latência baixa para o clã no Brasil).
- Uma VM roda tudo com `docker compose` (servidor de jogo, backend, Postgres); backup diário do banco.
- Opções: **Oracle Cloud Free Tier** (ARM Ampere, 4 núcleos / 24 GB grátis, tem São Paulo — exige compilar o
  servidor para LinuxArm64) ou VM x86 paga (AWS, GCP, Azure, Magalu Cloud; ~2–4 vCPU / 8 GB).
- Portas: UDP 7777 (jogo) e 443 (backend, HTTPS). Sem VPN.
- Capacidade: medido na F0, 10 jogadores = ~0,3 núcleo, ~530 MB e ~66 KB/s de saída no servidor de jogo.

Assets: o servidor de jogo precisa do conteúdo cozido (mapas, colisão, animações) — é a sua cópia extraída
indo para uma máquina sua, privada. Os clientes continuam extraindo cada um da própria instalação.

## Estado (2026-10-11)

- F0: engine 5.8.3 compilada do código-fonte em `/mnt/jogos/UnrealEngine/src-5.8.3` (dependências só de Linux;
  dois ícones do AutomationTool/IOS gerados no lugar para o UAT compilar). Servidor `NWPortServer` empacotado
  por `tools/package_server.sh` em `deploy/build/LinuxServer` (2,3 GB; cozido com o editor instalado, mesmo
  CompatibleChangelist). Versão de rede própria do jogo (`FNWPortModule`, `NetProtocol`): sem isso o servidor
  do código-fonte recusa clientes da engine instalada. Testado com `NW_SERVER=pacote tools/net_test.sh 3`: 3
  personagens entram e salvam, intruso recusado. **F0 concluída** no Docker (`deploy/build_images.sh` + `docker compose
  up -d`; segredos em `deploy/.env`). Medição com 10 bots (`NW_SERVER=docker tools/net_test.sh 10`, `-nwbot`
  anda e ataca), com os 10 conectados: CPU 26–31% de um núcleo, RAM ~530 MB, saída ~66 KB/s no total
  (~53 kbit/s por jogador). Uma VM de 2 vCPU / 2–4 GB sobra para o clã; banda não é problema.
- F1, backend: `backend/` (crate `nw-core`) pronto, com 8 testes passando contra Postgres. Ver
  `backend/README.md`.
- F1, servidor de jogo: `UNWBackend` (HTTP) e `ANWGameMode` (passe no `PreLoginAsync`, nasce na posição
  salva, salva a cada 30 s e ao sair). Habilidades e maestria passam a ser guardadas pelo servidor e
  replicadas só para o dono. Testado com `tools/net_test.sh` (servidor `-server` + 2 clientes): entrada pelo
  passe, combate entre os dois, posição/vida/arma/habilidades salvas e restauradas, conexão sem passe
  recusada. A posição só é salva com o personagem no chão (caindo, fica a última boa).
- Cliente: tela de entrada (`FNWLoginMenu`) montada com peças do jogo (landingscreenv2: painel de 3 vagas e
  Jogar; popup como janela de entrar/criar conta/criar e excluir personagem; campo de nome da criação de
  personagem). Gerada por `tools/ui_build_login.py`. Sessão lembrada 30 dias; excluir pede o nome, como no
  jogo; erro de conexão do servidor aparece na tela. Liga com `[NW.Server] Backend=` e `Address=` no
  DefaultGame.ini ou `-nwbackend= -nwconnect=`; sem isso o jogo abre direto na arena, como antes.
  Verificada por `-nwuisteps` (passos de preencher/clicar pelo mesmo caminho do mouse): entrar, senha
  errada, criar conta por convite, criar e excluir personagem, Jogar até a cidade. Digitação real no teclado
  ainda não foi testada por uma pessoa.

## Fases

Cada fase fecha testável. Teste local: servidor dedicado + vários clientes na mesma máquina, com
`Net PktLag=80 PktLoss=1` para simular internet.

### F0 — Servidor dedicado de verdade
- Unreal 5.8.3 compilado do código-fonte (GitHub da Epic; exige vincular a conta Epic ao GitHub).
- Alvo `NWPortServer`, cook para Linux, imagem Docker; sobe local e recebe 3 clientes.
- Medir CPU/RAM/banda com 10 clientes (bots de teste que andam e atacam).

### F1 — Backend Rust
- Projeto `nw-core` (axum, sqlx, Postgres, migrações): contas por convite do clã, login, personagens.
- Servidor de jogo: valida token ao conectar, carrega e salva personagem (posição, vida, arma, habilidades).
- Tela inicial do jogo (login, escolher personagem, entrar).

### F2 — Rede agradável (em andamento, 2026-10-11)
Feito: predição dos golpes (cliente toca na hora; servidor confirma pelo número da predição ou recusa e o
cliente desfaz; com 150 ms simulados, 0 recusas e 1 correção em ~30 golpes), placas de nome e vida, tela de
morte do jogo com Renascer (vai ao ponto de partida da cidade; morto não anda), chat (Área/Sistema, Enter,
limite 225 caracteres e 5 mensagens/5 s, comandos /ajuda), internacionalização pt-BR/en-US em tudo.
Grupo: até 5, comandos /convidar /aceitar /recusar /sair /g (e em inglês /invite /accept /decline /leave),
convite vale 60 s, liderança passa adiante, membros não se ferem, painel com a barra de membro do jogo
(grouphud) e nome do aliado em azul. **F2 concluída.**

- Ações com predição (o cliente anima na hora; o servidor confere e replica; recusa = cancela).
- Nomes e vida sobre os jogadores, HUD de grupo do jogo, chat (canal da cidade e do grupo).
- Respawn na cidade.

### F3 — Itens e inventário (concluída, 2026-10-11)
Feito: catálogo de 48 itens das tabelas do jogo (`tools/make_items.py`: espadas/machadinhas/escudos T1–T5,
poções de vida, materiais; nomes pt/en), itens e moedas no nw-core (migração 0002, troca do inventário inteiro
numa transação, `first_join` para o kit inicial), `UNWInventoryComponent` (bolsa 40, equipamento espada/
machadinha/escudo, pilhas, poção com cura do jogo escalada e recarga de 25 s, dano × gear score da arma),
saque dos Lost (quem matou + grupo a 60 m, sorteio por jogador, aviso à direita), poção no atalho 3, tela de
inventário (Tab) com a janela do jogo, grade, dica e cliques (esquerdo pega/solta, direito usa/equipa).
Salvamento ao sair antes de o personagem ser destruído (`PawnLeavingGame`). Falta para o polimento: boneco de
equipamento do jogo, malha da arma equipada (hoje a aparência é fixa), arrastar e soltar.

- Banco de itens a partir dos datasheets do jogo (subconjunto: armas, escudos, armadura, poções, comida,
  materiais), carregado no backend; instâncias de item no Postgres.
- Telas do jogo: inventário e equipamento; equipar arma pelo inventário.
- Saque dos Lost (moedas e materiais).

### F4 — Guardaventos completo (pedido de 2026-10-11)
Só o território de Guardaventos, inteiro, antes de qualquer outra região. Etapas em `docs/guardaventos.md`:
terreno da região, coleta (minério, plantas, alquimia, madeira), criaturas dos POIs, cidade viva (NPCs,
interação com E, baú, mercador, teleportes) e baús de saque.

### F5 — Crafting
- Estações da cidade de Guardaventos com receitas dos datasheets e a tela de crafting do jogo; os materiais
  vêm da coleta da F4.

### F6 — Casas
- 23 endereços da South Windsward; compra com a tela do jogo; dono, convidados, baú da casa.

### F7 — Produção
- Deploy na nuvem, HTTPS, backups, logs; ferramentas de admin (CLI em Rust: dar itens, banir, resetar casa).

### F8 — Entrada do clã
- `tools/setup.sh` de ponta a ponta (extração da própria instalação + build do cliente) e guia curto.
- Avaliar carregar assets em tempo de execução, para o cliente sair sem assets e cada um só extrair.

## Fora do escopo agora

Outras regiões além de Guardaventos, missões, facções, decoração de casa, interior instanciado, PvP, anti-cheat.

## Riscos

- Compilar a engine do código-fonte: horas de build e ~150 GB de disco (há 267 GB livres no SSD dos jogos).
- Predição de ações (F2) é a parte delicada do "agradável".
- Entrada de 10+ pessoas: cada um precisa do New World instalado, do Unreal e de espaço para a extração.
- Free Tier ARM: compilação cruzada para LinuxArm64 e disponibilidade de instâncias na região.
