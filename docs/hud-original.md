# Telas e HUD do jogo original

Pesquisa nos arquivos do jogo (2026-10-11) e o que foi implementado no port. Levantamento detalhado de cada canvas
(elementos, textos, texturas, Lua) em `docs/hud-levantamento.md`, gerado por `python3 tools/ui_survey.py`.

As 164 telas LyShine estão em `extract/ui/lyshineui/**/*.dynamicuicanvas.json` (`tools/build_all.sh` etapa 03).
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

## Inventário, bolsa, moedas

- `newinventory`: janela à direita do centro; itens em peças de 122 × 60 (`ItemDraggable/ItemLayout`): fundo
  `itembglarge<raridade>` + faixa `itemraritybglarge<raridade>` + ícone, tier em romano, gear score, quantidade sobre
  `itemnumberbg`. Barra de peso (`WeightBar`) e moedas no rodapé (`CurrencyText`).
- `equipmentv3`: equipamento à esquerda com abas `tabweaponsarmor` e `tabtools`; espaços `framesquare` +
  `equipmentbg` com ícone do tipo quando vazio.
- `hud2024/actionbar/encumbrance`: aviso de sobrecarga na HUD.
- Peso dos itens: `Weight` das itemdefinitions, em décimos de kg; limite base 1000.

## Status

- `hud2024/paperdollstatus`: estado do equipamento (durabilidade) na HUD.
- `hud2024/xp`: barra de experiência (o port ainda não tem níveis de personagem).
- `statuseffectsv2`, `afflictions2024`: efeitos ativos.

## O que o port já tem e o que foi feito agora

Ver a tabela de estado no fim deste arquivo, atualizada a cada etapa.

| Tela | Estado |
|---|---|
| HUD de combate (vida, vigor, habilidades, armas, consumíveis) | feita antes (hud2024) |
| Inventário + equipamento + peso + moedas | feita (layout do jogo, peças originais) |
| Câmera com os valores do jogo | feita: estados normal/sprint/inventário, zoom na roda (0,2–6 m), FOV 50° vertical, personagem vira de frente no inventário (`ANWCharacter::UpdateCamera`) |
| Menu do Esc | feito: Continuar, Configurações, Sair para o Menu Principal, Sair para a Área de Trabalho (`FNWGameMenu`) |
| Configurações | feitas, textos do jogo: Vídeo (qualidade, modo de janela, FPS máximo, VSync, campo de visão, desfoque, exibir FPS), Áudio (geral, música, efeitos, ambiente), Jogo (idioma, sensibilidade H/V, inverter eixo). Salvas em GameUserSettings.ini [NW.Options] (`FNWOptions`) |
| Bússola | feita: tira compass2048 rolando com a câmera, linha e agulha (`ANWHUD::DrawCompass`; norte do mapa em yaw −4°) |
| Números de dano | feitos: sobem e somem sobre o alvo; vermelho = dano em mim; bloqueio/esquiva/guarda quebrada em texto (`ANWHUD::DrawDamageNumbers`) |
| Bolsa e sobrecarga | feitas: bolsa na barra de ações com % do peso a partir de 80%; acima de 1000 fica vermelha com o aviso do jogo (`inv_equiploadencumberednotification`) e o personagem não corre, não esquiva e não pula |
| Barra de XP, durabilidade, efeitos | depende de sistemas que o port ainda não tem |
