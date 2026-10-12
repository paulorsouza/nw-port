# Estado do projeto

Ponto de partida para quem chega (pessoa ou sessão nova de IA). Atualizado em 2026-10-12.

## O que é

Port do New World para Unreal 5.8.3 para o clã jogar num servidor próprio. Assets da instalação de cada um
(nada do jogo no repositório). Cliente principal Windows, servidor Linux, desenvolvimento no Linux. Só pt-BR e
en-US. Escopo atual: o território de Guardaventos inteiro (região `r_+04_+01`), uma região por vez.

Decisões fechadas: Unreal é o plano principal; o cliente original com servidor emulado (Discord OpenWorld) é um
plano paralelo do dono (agora no repo cliente original (fora deste repositório): https://github.com/paulorsouza/cliente original (fora deste repositório)); Lumberyard/O3DE descartado. Tema jurídico encerrado.

## Funciona

| Área | Onde |
|---|---|
| Combate de espada e escudo e machadinha (leve, pesado carregado, bloqueio, esquiva, habilidades Q/R/F, maestria) | `NWCombatComponent`, `NWCharacter`, `docs/plano-mecanicas.md` |
| Lost (inimigo) com IA simples e saque | `NWLost`, `ANWGameMode::GrantLoot` |
| Servidor dedicado + backend Rust (contas por convite, personagens, inventário salvo) + Docker | `backend/`, `deploy/`, `docs/plano-servidor.md` |
| Tela de entrada, chat, grupo, morte/renascer, HUD do jogo, tela de maestria | `NWLoginMenu`, `NWChat`, `NWHUD`, `NWMasteryMenu` |
| Inventário com itens do jogo, raridade, peso, equipamento de armas e ferramentas | `NWInventoryComponent`, `NWInventoryMenu` |
| Coleta: 100 mil coletáveis da paisagem (minério, árvores, plantas, alquimia, pedras), ferramentas, saque das tabelas do jogo, respawn | `NWGatherField`, `NWGatherComponent`, `tools/make_gatherables.py` |
| Cidade de Guardaventos, terreno da região (2 km), cenário pré-gerado (penhascos, ruínas) | `tools/ue/import_*.py` |
| Música e ambiente de Guardaventos | `NWAmbienceComponent` |
| Telas do jogo: menu do Esc (com Desprender), configurações (vídeo, áudio, jogo, atalhos), barra de navegação, mapa (M) com POIs, bússola com POIs, tooltip de item com comparação, aviso de saque, faixa de aviso, indicador de golpe, sobrecarga, barra de XP, banner de nível, atributos, aba de ofícios na janela K | `NWGameMenu`, `NWOptions`, `NWWorldMap`, `NWHUD`, `NWInventoryMenu`, `NWTradeskillMenu`; pesquisa e estado em `docs/hud-original.md` |
| Nível, XP e atributos com as tabelas do jogo (XP de personagem ainda sem fonte; "Ganhar 1 nível" no F10); ofícios de coleta com XP, nível, velocidade, bônus no saque e nível mínimo dos coletáveis; tudo salvo no nw-core | `NWProgressionComponent`, `tools/make_progression.py`, migration 0003 |

Fases do servidor F0–F3 concluídas; F4 (Guardaventos completo) em andamento: `docs/guardaventos.md`.

## Falta / problemas conhecidos

Lista completa em `docs/pendencias.md`. Os principais agora:

- Chão com uma textura só (falta o material por bioma do jogo: `region.tractmap.tif` + materiais de terreno).
- Personagem "voando" em alguns pontos e pedregulho flutuando: sem diagnóstico ainda.
- Copas das árvores da cidade com placas coloridas (folhagem masked).
- Criaturas dos POIs (javali, lobo, esqueletos...), NPCs da cidade, baús: etapas G3–G5 de `docs/guardaventos.md`.
- Texturas novas das telas (tooltip, mapa, barra de XP, banner, aviso de saque) ainda não importadas na Unreal:
  com o editor fechado, `tools/ue_import_all.sh 09`. Sem isso essas telas aparecem sem as imagens.
- Telas feitas em 2026-10-11/12 sem editor aberto: compilam, mas ninguém viu na tela ainda.

## Como trabalhar

- Montar do zero: `docs/SETUP.md` (`tools/build_all.sh` + `tools/ue_import_all.sh`).
- Mudou uma tabela ou script de dados: `tools/make_data.sh`. Mudou C++: compilar (passo 01 do `ue_import_all.sh`).
- Verificar algo visual: abrir o editor e olhar (ou pedir o print a quem está com o editor). Não automatizar
  "rodar o jogo e tirar print": consumiu horas sem mostrar o que importa.
- Commits e push só quando o dono pedir.

## Armadilhas (já custaram horas)

Memória e máquina:
- Ubuntu fecha programas pelo `systemd-oomd` quando a *pressão* de memória passa de 50% por 20 s, mesmo com RAM
  "livre" no htop (o cache de disco conta). Causa: vários trabalhos pesados juntos (build + import + nwbt +
  New World original). Regra: um trabalho pesado por vez. Não usar `systemd-run -p MemoryMax`: o teto faz o
  próprio trabalho gerar pressão e o oomd fecha outro programa.
- `nwbt` carrega ~6 GB de índice dos paks a cada chamada; conversões de malha em lotes de 8 com `-w 2`.
- Import de glb muito grande na Unreal esgota a memória: o cenário de props da região num arquivo só (2,4 GB,
  150 mil peças) derrubou a sessão. Exportar e importar uma camada de capitals por vez.
- Vigia de memória rodando dentro do app do Claude morre junto quando o oomd fecha o app; o trabalho pesado
  continua sem vigia.
- VRAM de 8 GB: New World original aberto + cliente Unreal não cabem juntos.
- `/tmp` é RAM (tmpfs): nunca deixar o cache do nwbt lá (`.nwbt/` fica na raiz do projeto).
- `pgrep -f`/`pkill -f` casam com a própria linha de comando do shell.

Unreal:
- glTF pelo Interchange: usar os pipelines de glTF do projeto (DefaultGLTFAssetsPipeline + DefaultGLTFPipeline),
  que fazem instâncias do `M_Default` com as texturas. Só o pipeline genérico deixa os materiais sem textura.
- Uma pasta por malha importada: materiais de malhas diferentes têm o mesmo nome ("stone").
- Materiais precisam de "Used with Nanite" e "Used with Instanced Static Meshes"; senão o jogo fora do editor
  usa o material cinza padrão. Os pais do Interchange ficam no plugin da engine: `fix_nanite_materials.py`
  copia para `/Game/NW/Materials/Interchange` e reaponta (não altera a engine instalada).
- Nanite só desenha opaco e masked: malha com material translúcido fica sem Nanite.
- Build de servidor é unity build: `static` de mesmo nome em dois .cpp colide.
- JSON da Unreal 5.8: chaves de `FJsonObject::Values` são `TSharedString` (usar `*Key`).
- Commandlets de Python devolvem erro por avisos de material do glTF; conferir "Python script executed
  successfully" no log.
- Python do UE: `unreal.Rotator(a, b, c)` posicional é (roll, pitch, yaw); usar argumentos nomeados.

Dados do jogo:
- Coordenadas: glTF do nwbt = (−x, z, y) do CryEngine; UE = (−x, y, z) × 100. O mapa usa o referencial da cidade
  (giro de −94° em volta do capital `Settlement_SouthWindsward_Complete`).
- Altura do heightmap = valor / 32.
- Saque: dado 0..MaxRoll; tabela AND dá todo item com limiar ≤ dado; OR fica com o maior limiar alcançado.

## Mapa do repositório

| Pasta | Conteúdo |
|---|---|
| `unreal/NWPort/Source` | C++ do jogo (cliente e servidor) |
| `unreal/NWPort/Config` | configuração (Nanite, Lumen, VRAM, cook) |
| `tools/` | extração e conversão (Python/bash); `tools/ue/` roda dentro da Unreal |
| `backend/` | nw-core (Rust, Postgres): contas, personagens, inventário |
| `deploy/` | Docker do servidor e do backend |
| `docs/` | este arquivo, setup, planos e pendências |
| `patches/` | diff do nw-buddy (o código fica em paulorsouza/nw-buddy, branch nw-port) |
| `extract/` (fora do git) | tudo que sai do jogo |
| `unreal/NWPort/Content`, `Data` (fora do git) | gerados pelos scripts |
