# Telas e HUD do jogo original

Pesquisa nos arquivos do jogo (2026-10-11) e o que foi implementado no port. Levantamento detalhado de cada canvas
(elementos, textos, texturas, Lua) em `docs/hud-levantamento.md`, gerado por `python3 tools/ui_survey.py`.

As 164 telas LyShine estão em `extract/ui/lyshineui/**/*.dynamicuicanvas.json` (`tools/linux/build_all.sh` etapa 03).
O jogo monta quase tudo por Lua (botões, listas, opções): o canvas traz só moldura e modelos; os textos dos
botões e opções estão nos `.luac` (`strings` revela as chaves `@ui_*`, traduzidas em `extract/loc`).

## Câmera (`libs/camera/gamecamera.xml`)

171 estados de câmera. Os que importam para andar e combater (CryEngine: metros, FOV vertical):

| Estado | Offset (direita, frente, altura) | Distância | Zoom | FOV | Inclinação (baixo / cima) |
|---|---|---|---|---|---|
| `DynamicCamera` (padrão) | 0,5 / 0 / 1,7 | 2,0 | 0,2 a 6,0 | 50 | −70° / 60° |
| `NormalCamera` | 0,5 / 0 / 1,7 | 2,7 | 0,2 a 6,0 | 50 | −70° / 30° |
| `MidCameraSprint` | 0,7 / 0 / 1,7 | 4,0 | — | 50 | — |
| `CloseCamera` | 0,5 / 0 / 1,6 | 1,7 | fixo | 50 | — |
| `LockedCamera` (alvo travado) | 0,4 / 0 / 1,8 | — | — | 50 | −35° / 35° |
| `UI_Inventory` | 0,77 / 0 / 0,95 | 2,75 | fixo | 50 (ignora a opção do jogador) | −70° / −10°; `BlendLookDirection`: o personagem vira de frente |

Transições entre estados: 0,2 s (`SmoothTime`). FOV 50 é vertical: em 16:9 dá ~79° horizontais (a Unreal usa
horizontal). Distância máxima de visão 3000 m.

## Menu do Esc (`escapemenu`)

Janela de 840 × 800 no centro (`EscapeMenuHoder`), seções com títulos do jogo: `@ui_escapemenu_game` (Jogo),
`@ui_escapemenu_support` (Suporte), `@ui_escapemenu_leave_game` (Sair do Jogo), e a loja à direita. Botões do
`escapemenu.luac`: `@ui_navmenu_settings` (Configurações), `@ui_escapemenu_unstuck`, `@ui_escapemenu_help`,
`@ui_escapemenu_exit_to_main_menu`, `@ui_escapemenu_exit_to_desktop`, entre outros.

## Configurações (`options`)

Abas: `@ui_options_visuals` (Vídeo), `@ui_options_audio` (Áudio), `@ui_options_gameplay` (Jogo),
`@ui_options_kbm_controls` (Atalhos), `@ui_options_controller`. 587 chaves no `options.luac`. As que o port usa:

- Vídeo: `@ui_graphics_settings` (Qualidade de Vídeo: Baixa/Média/Alta/Muito Alta), `@ui_window_mode`,
  `@ui_resolution`, `@ui_max_fps` (30…360/Ilimitado), `@ui_enable_vsync`, `@ui_options_fov` (Campo de Visão),
  `@ui_enable_motionBlur`, `@ui_show_fps`, `@ui_options_brightness`.
- Áudio: `@ui_mastervolume`, `@ui_musicvolume`, `@ui_sfxvolume`, `@ui_ambVolume`, `@ui_ambientmusicvolume`,
  `@ui_uivolume`.
- Jogo: `@ui_language` (Idioma do Texto), `@ui_looksensitivityhorizontal`, `@ui_looksensitivityvertical`,
  `@ui_invertlook`, `@ui_cameraShake`, `@ui_options_damage_numbers_display_mode`.

## Mapa do mundo (`lyshineui/worldmap/magicmap`, tecla M)

- Tela cheia; o conteúdo são tiles `lyshineui/worldtiles/newworld_vitaeeterna/map_l<z>_y<yyy>_x<xxx>.dds` de 1024 px.
  Nível 1: 56 × 56 tiles de 256 m (4 px/m; o mundo tem 14 336 m). Nível z: 256·2^(z−1) m por tile, índices de
  2^(z−1) em 2^(z−1); níveis 1 a 8. O índice y cresce para o norte (conferido sobrepondo os coletáveis: só assim
  eles evitam o mar).
- Guardaventos (r_+04_+01) = x 8192–10240, y 2048–4096; a cidade (`Settlement_SouthWindsward_Complete`) está em
  (9349,6; 2702,2) com giro −94°.
- Peças: degradê `map/map_gradient` (em cima/embaixo) e `map/map_gradientside` (lados), régua `map/scale_key` com o
  texto da distância embaixo, botões à direita (`CenterPlayer`, `ZoomInButton`, `ZoomOutButton`: `buttonbg` +
  `heroframedisabled2023`), ícone do jogador `map/icon/player`. Ícones de POI em `map/icon/pois` (194),
  coletáveis em `map/icon/gatherables` e NPCs em `map/icon/npcs`.
- POIs: posição em `slices/pois/territories/r_<x>_<y>_pois.dynamicslice` (entidades com
  `TerritoryDataProviderComponent` "territory id" + `TransformComponent`, translação nos 3 últimos valores);
  nome (`NameLocalizationKey`) e ícone (`MapIcon`, .png no dado = .dds no pak) em
  `pointofinterestdefinitions/javelindata_poidefinitions_<x>_<y>`. Guardaventos: 78 territórios, 66 com ícone.

## Atalhos (`libs/config/defaultprofile.xml`)

Mapa de ações do jogo (CryEngine actionmaps), com as teclas padrão por ação: `moveforward/back/left/right` W/S/A/D,
`autorun` = (igual), `walk` Ctrl esquerdo (segurar), `jump` Espaço, `dodge` Shift esquerdo, `attack_primary` botão
esquerdo, `block` botão direito, `ability1–3` Q/R/F, `quickslot-weapon1/2` 1/2, `quickslot-consumable-1..4` 3–6,
`sheathe` ~, `crouch_toggle` C, `mount_summon` X, `toggleInventoryWindow` Tab (em `includes/uishared.xml`).
Os nomes na tela vêm de `ui_movefoward`, `ui_runtoggle`, `ui_walk`, `ui_rolldodge`, `ui_attack`, `ui_block`,
`ui_ability1..3`, `ui_weapon1/2`, `ui_consumable1`, `ui_interact`, `ui_inventory`. Conflito no jogo:
`ui_remapPopup_rebindConflict` (a outra ação fica sem tecla); no port as duas trocam de tecla.

## Inventário, bolsa, moedas

- `newinventory`: janela à direita do centro; itens em peças de 122 × 60 (`ItemDraggable/ItemLayout`): fundo
  `itembglarge<raridade>` + faixa `itemraritybglarge<raridade>` + ícone, tier em romano, gear score, quantidade sobre
  `itemnumberbg`. Barra de peso (`WeightBar`) e moedas no rodapé (`CurrencyText`).
- `equipmentv3`: equipamento à esquerda com abas `tabweaponsarmor` e `tabtools`; espaços `framesquare` +
  `equipmentbg` com ícone do tipo quando vazio.
- `hud2024/actionbar/encumbrance`: aviso de sobrecarga na HUD.
- Peso dos itens: `Weight` das itemdefinitions, em décimos de kg; limite base 1000.
- Bolsas: itens `BagAT1`–`BagAT5`, `BagBT2`–`BagBT5`, `BagAT52` (`ItemClass` Bag, ItemType Armor, em
  `itemdefinitions_master_crafting`); capacidade em `javelindata_itemdefinitions_armor` (`Encumbrance`, décimos:
  AT1 +50, AT2 +100, AT3 +150, AT4 +225, AT5 +300; BT2 +125 … ) e `EncumbrancePerGS` (2–6). Ficam em espaços
  próprios do inventário. O port ainda não tem bolsas: no jogo vêm de criação (Armoraria), que não existe aqui.

## Status

- `hud2024/paperdollstatus`: estado do equipamento (durabilidade) na HUD.
- `hud2024/xp`: barra de 334 × 58 no rodapé central: nível (`LevelText`, 27) em três molduras `xpmeternumberframe*`,
  barra `xpmeterfill` com máscara e ponta `afflictionmeteredge`, XP descansado (`MeterRestedFill`, roxo),
  multiplicador "XP x N" e o aviso "+N XP" (`DeltaXpText`).
- `statuseffectsv2`, `afflictions2024`: efeitos ativos.

## Nível, XP e atributos (tabelas do jogo)

- `javelindata_xpamountsbylevel`: 70 linhas (`Level Number` 0–69). `XPToLevel` é o XP **total** acumulado
  (150, 480, 1080…; 2,7 M na última). `AttributePoints`: 5 por nível no começo, 10 no fim; 510 no total.
  Também `AttributeRespecCost` (moedas para redistribuir) e `Health` (vida por nível).
  Leitura do port: nível n começa em `XPToLevel[n-1]`; ao chegar no nível n+1 ganha `AttributePoints[n]`.
- `javelindata_attribute<força|destreza|inteligência|foco|constituição>`: uma linha por valor do atributo (5 a 500).
  `ModifierValue` cai de 1,25 para 0,8 (retorno decrescente) e `ModifierValueSum` é o acumulado, usado no
  escalonamento do dano das armas. Foco tem `HealScaling`. Constituição tem `Health`, já acumulado
  (6 = 40, 14 = 360, 54 = 1908, 500 = 13769). Cada atributo começa em 5.
- `javelindata_gameevents`: recompensas por evento. Coleta: `Gather_<Mining|Logging|Harvesting|Skinning>T<n>` com
  `UniversalExpAmount` (3,35 a 23,45) e XP de profissão (`CategoricalProgressionReward`). Abate: `KillEvent` com
  `UniversalExpAmount` 1. Cada coletável aponta para o seu evento na coluna `GameEventID` de `javelindata_gatherables`
  (só em 454 das 1779 linhas, por isso não aparece na primeira linha da tabela). Não está nas tabelas a escala do
  `UniversalExpAmount` (no abate vale 1, então é multiplicado por algo do nível da criatura): o port ainda não dá XP
  de personagem por coleta ou abate, só o comando de dev "Ganhar 1 nível".
- Ofícios de coleta (`javelindata_categoricalprogression`: Mining, Logging, Harvesting, Skinning, nível máximo 200,
  250 com a expansão): curva em `javelindata_tradeskill<ofício>` (`InfluenceCost` = XP do nível anterior até este;
  Mineração 250 = 678 254 XP), `GatheringEfficiency` (+0,05 por nível no começo, 2,2 no 250) e `RollBonus` (+10 por
  nível no dado do saque). XP por coleta = `CategoricalProgressionReward` do evento (ex.: pedregulho pequeno 64,
  pinheiro grande 199). `RequiredTradeskillLevel` exige nível (pinheiro grande 50, planta da morte 30).

## Telas do jogo que o port ainda não tem

Todas extraídas em `extract/ui/lyshineui/<pasta>` (rodar `tools/ui_canvas.py` para ver a árvore). Cada uma depende
de um sistema que o port não tem:

| Tela | Canvas | Depende de |
|---|---|---|
| Criação (estações) | `crafting/craftingscreenv4` | receitas, estações na cidade, ofícios de refino/criação |
| Contratos / bolsa de comércio | `contracts/contractbrowser` | economia entre jogadores no nw-core |
| Troca | `trade/tradescreen` | troca entre jogadores (RPC + backend) |
| Social / Companhia | `socialpane/*`, `guildmenu/guild` | amigos e companhias no nw-core |
| Diário e objetivos | `objectives/journalscreen`, `objectiveshud` | missões (dados `javelindata_objectives*`) |
| Efeitos ativos | `statuseffects/statuseffectsv2`, `hud2024/afflictions` | efeitos de estado (poções com duração, venenos) |
| Moradia | `housing/*` | casas e decoração |
| Território, projetos | `territory/*` | posse de território, impostos |
| Pousada | `inn/inninteractmenu` | pontos de renascer/viagem rápida |
| Quadro de missões | `missionboard/missionboard` | missões de cidade |
| Emotes | `emoteui/emoteui` | animações de emote (há clipes no pak) |
| Masmorras | `dungeons/dungeonenterscreen` | expedições |

## O que o port já tem e o que foi feito agora

Ver a tabela de estado no fim deste arquivo, atualizada a cada etapa.

| Tela | Estado |
|---|---|
| HUD de combate (vida, vigor, habilidades, armas, consumíveis) | feita antes (hud2024) |
| Inventário + equipamento + peso + moedas | feita (layout do jogo, peças originais) |
| Câmera com os valores do jogo | feita: estados normal/sprint/inventário, zoom na roda (0,2–6 m), FOV 50° vertical, personagem vira de frente no inventário (`ANWCharacter::UpdateCamera`) |
| Menu do Esc | feito: Continuar, Configurações, Desprender (volta a uma posição recente no chão depois de 5 s, recarga de 60 s, mensagens `ui_unstuck_*` do jogo), Sair para o Menu Principal, Sair para a Área de Trabalho (`FNWGameMenu`) |
| Configurações | feitas, textos do jogo: Vídeo (qualidade, modo de janela, FPS máximo, VSync, campo de visão, desfoque, exibir FPS, brilho), Áudio (geral, música, efeitos, ambiente), Jogo (idioma, sensibilidade H/V, inverter eixo, números de dano flutuando ou empilhados). Salvas em GameUserSettings.ini [NW.Options] (`FNWOptions`) |
| Atalhos | feitos: aba "Atalhos de Teclado e Mouse" nas configurações, 19 ações com os textos e as teclas padrão do jogo; clicar e apertar a tecla troca (Esc cancela, conflito troca as duas), "Restaurar Padrões"; salvos em [NW.Options] `Key.<Ação>` e aplicados na hora (`ANWCharacter::RebuildKeys`). Corrida automática (=) adicionada: frente/trás cancela |
| Mapa do mundo (M) | feito: tiles do jogo (nível 2 de perto, 3 de longe) em volta de Guardaventos, borda da região, ícone do jogador virado para onde olha, degradês, régua de escala em metros, coordenadas do jogo, botões centralizar/aproximar/afastar, roda aproxima no cursor, arrastar move (`FNWWorldMap`). POIs da região com ícone e nome do jogo (nome de perto ou com o mouse em cima). Membros do grupo (`groupmember` com o nome), último local de morte (`lastdeathposition`) e marcador com o botão direito (`waypoint`; na bússola `waypoint_compass` com a distância em metros). Faltam coletáveis e NPCs no mapa |
| Bússola | feita: tira compass2048 rolando com a câmera, linha e agulha, POIs a até 400 m no rumo deles (`ANWHUD::DrawCompass`; norte do mapa em yaw −4°) |
| Números de dano | feitos: sobem e somem sobre o alvo; vermelho = dano em mim; bloqueio/esquiva/guarda quebrada em texto (`ANWHUD::DrawDamageNumbers`) |
| Barra de navegação | feita: `navbar` 2023 no alto do inventário, da janela K e do mapa (fundo `navbar_bg_2023`, `highlight` na aba ativa, ícones do jogo), com Inventário / Personagem / Mapa trocando de tela e o selo `unspentpoints` quando há pontos de atributo livres (`ANWHUD::DrawNavbar`) |
| Faixa de aviso | feita: `warningbar` do jogo (fundo `background_warningbar`, texto amarelo) por 3 s no terço de cima da tela para bolsa cheia, coletável ocupado e Desprender recusado (`ANWPlayerController::Warning`, `ANWHUD::DrawWarning`); o resto continua no chat |
| Indicador de golpe | feito: golpe recebido mostra a seta `hitdirection` em volta da mira (204 px do centro) apontando para o atacante e o brilho vermelho `hiteffect_light_b`/`_medium_b` (acima de 15% da vida) na borda da tela daquele lado, por 1 s, até 5 ao mesmo tempo (`ANWHUD::DrawHitIndicators`) |
| Cartão de coleta | feito: anel `marker_ringgather` no objeto, nome na faixa `unifiedinteractcardtextbg`, tecla em `hintframefilled` + ação; durante a coleta o anel de progresso enche com o tempo no meio (`ANWHUD::DrawGather`) |
| Tooltip do item | feito: layout do `DynamicTooltip` do jogo: cabeçalho `tooltip_header_bg_<raridade>` (104 px), moldura `tooltip_itemframe_rarity*` 90×90 com ícone e tier, nome/raridade/tipo (`ItemTypeDisplayName`), divisor dourado, gear score com `icon_gearscore`, Dano Base, Velocidade de Coleta, descrição com quebra por largura, Peso; faixa "Equipado" acima quando o item está equipado; item da bolsa que vai num espaço ocupado mostra o equipado ao lado para comparar, como no jogo (`FNWInventoryMenu::DrawTooltip`) |
| Aviso de saque (loot ticker) | feito: fundo `loottickerbg`, linhas de 58 px com sombra `progressiontickeritembg`, item em `lootticker_itembg_square` + raridade + ícone + tier, "+N" e nome; moedas com `reward_coin_cover`; até 5 linhas, a mais nova em cima (`ANWHUD::DrawLootTicker`) |
| Bolsa e sobrecarga | feitas: bolsa na barra de ações com % do peso a partir de 80%; acima de 1000 fica vermelha com o aviso do jogo (`inv_equiploadencumberednotification`) e o personagem não corre, não esquiva e não pula |
| Status (painel de equipamento) | feito: Pontuação do Equipamento (média das armas), Saúde, Vigor e Peso com os textos do jogo, no fundo `equipmentstatsbackdrop2023`; atributos (FOR/DES/INT/FOC/CON) dependem de um sistema que o port ainda não tem |
| Barra de XP | feita: tela `hud2024/xp` do jogo na HUD (nível, preenchimento do nível atual, ponta que acompanha, "+N XP" por 2,5 s). Nível e XP no `UNWProgressionComponent` com a curva do jogo; sem XP descansado nem multiplicador |
| Banner de nível | feito: ao subir de nível, a parte de XP do `bannerlevelup` do jogo (runa `bannerleveluprune_xp` com a `_x` girando, faixa `banner_light_blue2`, linha `notification_showline`, "Subiu de Nível" e o número) por 4,5 s (`ANWHUD::DrawLevelUp`) |
| Atributos | feitos no painel de status: Nível com XP do nível, Força/Destreza/Inteligência/Foco/Constituição (base 5), pontos livres e "+" para distribuir (servidor confere). Ainda não mudam dano nem vida |
| Ofícios de coleta | feitos: XP de Mineração/Extração de Madeira/Colheita/Esfolamento por coleta com os valores do jogo, nível pela curva do jogo, bônus de velocidade de coleta do nível somado à eficiência da ferramenta, linha de progresso no aviso de saque (ícone do ofício, "+N", nome, nível e barra). Nível mínimo do coletável (`RequiredTradeskillLevel`, "Requer o Nível 50 (Extração de Madeira)") vale para personagem do servidor; `RollBonus` do nível no dado do saque conforme o `RollBonusSetting` de cada tabela (AddToRoll soma, ClampMax soma até o máximo, IgnoreBonus/None não) |
| Aba de ofícios (K) | feita: abas no topo da janela K (Maestria em Armas / Hab. Mercantes); a aba de ofícios é a `GatheringSection` do `skills` do jogo (células `skillcellbg`/`skillcellframe`, círculo `skilllevelcircleprogress*` com o nível no meio e o anel enchendo até o próximo nível, destaque `skillcellhover`), sem pesca (`FNWTradeskillMenu`, `tools/ui_build_tradeskills.py`) |
| Aba de atributos (K) | feita: `Attributes` do `skills` do jogo (runa de pontos disponíveis, linhas Força/Destreza/Inteligência/Foco/Constituição com valor, "+N" pendente, botões − e + com foco no hover, Confirmar, barra de XP com o nível e "N EXP para o Nível X"); barra de cada atributo desenhada com `attributebarbg` até 300 e marcas a cada 50 (o jogo monta as seções de limiar por Lua). Sem Redistribuir, sem a coluna de estatísticas (`FNWAttributesMenu`, `tools/ui_build_attributes.py`) |
| Pendências de progressão | origem do XP (coleta e abate, ver acima); salvar nível/XP/atributos no nw-core (migration); efeitos dos atributos no combate |
| Durabilidade, efeitos | dependem de sistemas que o port ainda não tem |
