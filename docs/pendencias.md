# Pendências

Anotado em 2026-10-11, depois da primeira sessão no Unreal.

## Movimento

- Coisas estranhas no movimento com o mouse (girar a câmera enquanto anda). Reproduzir e olhar a linha de diagnóstico (`corpo` × `camera`, `dir`).
- Andar bloqueando: o personagem se arrasta. Falta o blend space de caminhada com escudo levantado (só existe o loop `rswrd_walk_block` para frente; procurar as direções no `.adb` com a tag `Blocking`).
- Diagonal para a esquerda estranha: nos clipes do jogo o quadril fica virado à direita em quase todas as direções. Provável ajuste de tronco para a mira (aim/look IK) no jogo.
- Pose a 0° (`straighten_loops`) ficou melhor; refinar depois.

## Cidade

- South Windsward convertida (`extract/cities/.../settlement_southwindsward_complete.glb`). O slice traz só construções; o chão das ruas é o terreno do mundo.
- Terreno (2026-10-11): `tools/make_terrain.py` gera 768×768 m a 1 m sob a cidade, a partir do `region.heightmap` (altura = valor/32) e do `roads_splat` (terra nas estradas); `tools/ue/import_terrain.py` põe no mapa. Partida no cruzamento central. **Testar no editor** se o personagem anda na rua.
- O personagem caía porque o ponto de partida ficava sobre uma praça sem terreno. Colisão das construções conferida: BlockAll + complex-as-simple em 1130 malhas.
- Blocos verdes/vermelhos = materiais NoDraw do jogo (`NoCollide`, `COL`, `Obstruct`): o nwbt os mantém com opacidade 0.1 e o UE desenha opaco. `tools/glb_strip_nodraw.py` remove essas primitivas (glb limpo em `extract/cities/limpa/`, mesmo nome do original para o UE substituir os assets).
- Fora da conversão (o nwbt só gera malhas estáticas e luzes): 25 bandeiras de guilda (`RenderGuildFlagComponent`), decalques, partículas (fogo/fumaça), água, vegetação solta da região (`region.vegetation`).
- Sem animação: moinhos, rodas d'água, bandeiras, NPCs (vêm como malha parada).
- Estações de ofício: o slice complete aponta para as versões T5 (ex. `Settlement_Station_Blacksmithing_T5`). Conferir se resta algum placeholder depois do filtro NoDraw.
- `.aliasasset` (sorteio de variantes): o walker do nwbt inclui todas as variantes sobrepostas. Verificar se aparece na cidade.
- Casas de jogador: no jogo são instâncias separadas; aqui dá para entrar em todas (decidir se fecha as portas).
- Diagnóstico `nwbt models slicestats` (novo) converte texturas sem necessidade por causa do coletor; ajustar antes de usar.
- Textura do terreno simplificada (grama + terra). O jogo mistura dezenas de camadas por `*_splat` (rough, soil, cliffs...); o `macro_diff` não é mapa de cor da região.
- Fora dos 768 m não há terreno (só o chão de segurança). Vegetação (`region.vegetation`) e água não extraídas.

## Pulo e travessia (2026-10-11)

- `UNWTraversalComponent`: Espaço pula; com obstáculo à frente faz degrau, vault ou mantle, nas faixas do `TraversalComponent` do jogo (`slices/player.dynamicslice`). Root motion ajustado à altura/distância reais. Espaço segurado no ar agarra borda (mantle).
- Teclas do jogo (`libs/config/defaultprofile.xml`): Espaço pula, Shift esquiva. Corrida foi para o Alt esquerdo (no jogo não há tecla equivalente com a arma em mãos).
- Pista de teste em `L_Arena`, atrás do início. `nw.DebugTraversal 1` desenha os raios e escreve a decisão no log.
- A testar: escolha entre variantes, encaixe no fim (teleporte para o ponto calculado), rede (cliente remoto), pouso pesado. Escada (`unarmed_ladder_*`) não implementada.

## Ataque pesado (2026-10-11)

- Fluxo do jogo (player_sword.adb): apertar = `Attack_Start_Charge`; soltar < 0,333 s = leve (continua o combo); 0,333–0,667 s = `Attack_Heavy` (1H_Sword_Attack2, 1,2×); 0,667 s = `Attack_Heavy_Charged` sai sozinho (1H_Sword_Attack2_Charged, 1,6×). Implementado no `UNWCombatComponent` (ações HeavyCharge/Heavy/HeavyCharged) com barra de carga na HUD.
- A conferir: custo de stamina do pesado (CostID `Attack_1H_Sword_Heavy` saiu 0 na tabela); relógio da barra de carga em cliente remoto (usa o tempo do servidor).

## HUD (H1, 2026-10-11)

- Telas do jogo convertidas: `tools/ui_canvas.py` (LyShine .dynamicuicanvas → layout) e `tools/ui_build_hud.py` (vitals2024, weaponquickslots, consumablequickslots, reticles → `Content/NW/UI/hud_layout.json`); texturas por `tools/ue/import_ui.py`; fontes Nimbus lidas do .otf em `Content/NW/UI/Fonts` (o importador de fontes abre janela e derruba o commandlet).
- `FNWUILayout` desenha com as regras do LyShine (âncoras/deslocamentos, 9-slice, preenchimento linear/radial, texto). `ANWHUD` põe os valores que o Lua punha.
- Habilidades com recarga do jogo (javelindata_cooldowns_player). Ícone sobre fundo por categoria (AbilitiesCommon: ataque = bg2 vermelho, defesa = bg3 azul).
- `-nwshot=arquivo.png` no jogo: usa Q e R, tira print aos 6 s e fecha (conferência automática).
- A fazer: estilo exato da recarga (o Lua usa COLOR_ABILITY_COOLDOWN; hoje a barra dourada cobre o ícone), poções funcionando (3–6), barra do alvo e números de dano no estilo do jogo (hud/damagenumbers, nameplates), H2 (menus) e H3 (machadinha + troca 1/2).
- Custo de stamina continua 0 (CostID do datasheet não casa com o da tabela; conferir).

## Menus (2026-10-11)

- K = menu de habilidades (SNWSkillMenu, Slate): 6 ativas da espada com nomes/descrições pt-BR do jogo (`tools/make_abilities.py` → `Data/UI/abilities.json`); escolha gravada em GameUserSettings.ini [NW.Loadout].
- F10 = menu de dev (SNWDevMenu): sem recarga, imortal, vigor infinito, curar, invocar Lost/bonecos, matar/remover, câmera lenta, viagem arena/cidade, depuração.
- Efeitos novos: Pancada atordoa 2 s (loop AM_StunLoop); Determinação −30% de dano por 8 s (montagem shield_spc_attack_3_start); Golpe com Salto usa o clipe do golpe de corrida; Transtorno agora usa 1H_Sword_Attack_Stab (180%).
- A fazer: Corrida com Escudo empurrar de verdade (hoje só o root motion do clipe); o "−20% de dano de vigor ao bloquear" da Determinação; fim da animação da Determinação (`_end`); passivas e pontos de maestria.
- Layout/fontes/dados de UI ficam em `unreal/NWPort/Data/UI` (fora de Content, que o editor vigia e tentaria importar).

## Machadinha (2026-10-11)

- Segunda arma no mesmo conjunto (sword_shield): malha `wep_axe1h_throwingaxea_t5`, blend spaces `BS_Hatchet_*` (base de uma mão + braço da espada, sem escudo), montagens `AM_Hx_*` (janelas do player_axe.adb, tags 1H_Melee+Axe), tabela `DT_HatchetActions`.
- Troca no 1/2 (`ANWCharacter::WeaponSets`, `ActiveWeapon` replicado): toca o sacar; arma inativa no quadril (`Stowed` em Hips), escudo nas costas (Spine2, inclinado 29° para compensar a postura). HUD mostra a arma ativa embaixo e a outra acima (fundo inativo do jogo). Menu de habilidades com abas por arma; loadout salvo por arma.
- Habilidades: Berserk (+20% de dano 5 s), Salto Feroz, Enxurrada Furiosa, Arremesso Rasgante/Distanciamento Social/Arremesso Infectado (projétil `ANWProjectile`). Especiais 4/5 = Infectado/Berserk por dedução (conferir visualmente).
- ABP_NW precisa do pino Locomotion (ver resposta de 2026-10-11) para a locomoção trocar com a arma.
- A fazer: efeitos secundários (Vulnerável, lentidão, enfermidade, recuo invulnerável do Distanciamento, extensão do Berserk por acerto); a machadinha some da mão durante o arremesso; reações/morte usam as montagens da espada (braço do escudo).
- Ataques do jogo não gastam estamina (javelindata_staminacosts_player = 0 para ataques); só desvio/bloqueio.

## Tela de maestria do jogo (2026-10-11)

- K abre a MasteryTreeWindow original (`tools/ui_build_mastery.py` → `Data/UI/mastery_layout.json`), desenhada pelo FNWUILayout (agora com máscaras, mistura aditiva, deslocamento e clique). Lógica em `FNWMasteryMenu`: nós por TreeRow/TreeColumn do datasheet, arte por tipo (active/passive/info), setas das melhorias, espaços Q/R/F, pontos (19), ultimate com 10 pontos na árvore, tooltip com texto rico, abas 1/2 por arma. Estilo padrão da moldura (FrameMultiBg) e textos @chave aplicados no build.
- Passivas e ultimate só marcam pontos (efeitos não implementados). Menu Slate antigo de habilidades removido.
- A fazer: tooltip com a arte de tooltip do jogo; animações de desbloqueio (pulsos); arrastar habilidade para o espaço.

## Adiado (2026-10-11)

- Locomoção própria da machadinha: camadas do jogo `raxe_pose_add` (parado) e `v2_raxe_cmbt_walk_layer_l/r` (andando) sobre `melee1h_cmbt_nav_strafe_blend`. São aditivas (sufixo `_add`); o nwbt/pipeline ainda não soma camada aditiva. Hoje BS_Hatchet_* = base de uma mão + braço da espada.

## Cidade: polimento (2026-10-11)

- Tecidos retorcidos no céu e linhas azuis no chão: bandeiras/varais/cordas que no jogo são tecido simulado
  viraram malha estática deformada. Filtrar ou corrigir na conversão.
- PlayerStart da South Windsward ficava dentro da estátua da praça: `tools/ue/fix_spawn.py` moveu 4 m. O
  servidor empacotado ainda tem o mapa antigo: recozinhar no próximo pacote (`tools/package_server.sh`).
- Bots de teste sem render (-nullrhi) ignoram t.MaxFPS (~460 fps) e enchem o buffer de movimentos do
  CharacterMovement ("Hit limit of 96 saved moves") com atraso simulado. Só afeta os bots; a medição de 10 bots
  da F0 ficou pessimista (mais tráfego que jogadores reais).
- Morte: o corpo volta à pose de pé quando a montagem Death termina (segurar o último quadro, como em
  hold_montages). Data na lista de personagens em dia/mês também no inglês.
- Idioma: telas do jogo trocam na hora (HUD, maestria, morte, entrada); textos já desenhados pelo Slate do menu
  de dev só mudam ao reabrir.

## Coleta: polimento (F4/G1, 2026-10-11)

- Cartão de coleta já usa as peças do jogo (markers/unifiedinteract); falta o cartão de várias opções
  (`hud/unifiedinteractcard/interactscreen`) para NPCs e estações.
- Ofícios (Mineração, Colheita, Corte de madeira, Esfolamento) sem nível nem XP; o saque usa o dado sem bônus de
  ofício (`RollBonusSetting` = AddToRoll ignorado). Tabelas com `Conditions` (tônicos de ferramenta, temporada,
  PvP) e baldes `[LBID]` ficam de fora.
- Sem VFX/SFX de esgotar (`DepletedVFX`/`DepletedSFX` das variações) nem ícone na bússola (`CompassIcon`).
- Vegetação dos lotes `<PLOT>` (1.169) e árvores fora da camada de coletáveis: etapa G5.

## Telas do jogo sem editor (2026-10-12)

Feitas com o PC sozinho (sem abrir o editor nem rodar o jogo): compilam, falta conferir na tela.

- Importar as texturas novas: editor fechado, `tools/ue_import_all.sh 09` (listas `tooltip`, `lootticker2024`,
  `worldmap`, `levelup` e a tela `xp` do `hud_textures`). Os 52 tiles do mapa entram comprimidos (DXT1).
- Conferir: tooltip (Tab, mouse num item), aviso de saque (coletar), mapa (M: arrastar, roda, botões, POIs),
  bússola com POIs, barra de XP e banner (F10 > Ganhar 1 nível), atributos e "+" no painel de status,
  aba Atalhos (trocar tecla, Esc cancela, Restaurar Padrões) e corrida automática (=).
- Progressão: XP não vem de coleta nem de abate (as tabelas não dizem a fórmula; ver `docs/hud-original.md`);
  nível/XP/atributos não são salvos no nw-core (precisa migration + `backend/src/routes/internal.rs`);
  atributos ainda não mudam dano nem vida.
- Mapa: faltam coletáveis rastreados, NPCs, outros jogadores do grupo; névoa de áreas não descobertas.
- Atalhos: só teclado e mouse; sem controle. Conflito troca as duas teclas (o jogo apaga a outra).
