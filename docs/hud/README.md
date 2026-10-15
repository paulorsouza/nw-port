# Telas do jogo no port

Como portar uma tela do LyShine (canvas + Lua do jogo) e testar sem abrir o jogo. Prints do jogo original para
comparar e se inspirar: `inspiracao/` (índice em `inspiracao/README.md`). Exemplo completo: a tela de refino
(`docs/hud/refino.md`, `tools/ui_screens/refining.json`, `FNWRefiningMenu`). Pesquisa de todas as telas:
`docs/hud-original.md`, `docs/hud-levantamento.md`; RE das telas: `re-local/hud-*.md`.

## Ciclo de trabalho

1. **Spec da tela** em `tools/ui_screens/<tela>.json`: canvas, subárvores que ficam, o que o Lua liga e desliga,
   estilos de texto e de botão (formato no cabeçalho de `tools/ui_build_screen.py`).
2. `python3 tools/ui_build_screen.py tools/ui_screens/<tela>.json` grava `Data/UI/<tela>_layout.<idioma>.json` e a
   lista de texturas. Textura nova: `tools/build_all.sh 12` (extrai do jogo) e `tools/ue_import_all.sh 09` (importa).
3. **C++** preenche os dados (`TextOverride`, `Visible`, `Tint`, `TextureOverride`, `Translate`, `DrawSubtree` para
   linhas de lista), seguindo o Lua da tela. Ligar `bExactText = true` no `FNWUILayout` da tela.
4. **Captura sem o jogo**: `tools/ui_shot.sh tools/ui_shots/<tela>.json [captura]` → `extract/ui/shots/<tela>_<captura>.png`
   em ~40 s. Unreal sem janela, mundo vazio, personagem com bolsa e níveis do cenário, o mesmo C++ da tela
   (`UNWUIShotCommandlet`). Precisa do módulo compilado e do editor fechado. Idioma: `NW_LANG=en-us`.
5. **Comparar**: `python3 tools/ui_compare.py <print do jogo> <captura> saida.png [--crop x0,y0,x1,y1]` (original em
   cima). O print do jogo é redimensionado para o tamanho da captura.
6. **Medir**: `NW_SHOT_ARGS=-dump=<trecho do caminho> tools/ui_shot.sh ...` lista no log
   (`Saved/ImportLogs/ui_shot.log`, linhas `NWUIDump`) retângulo, visibilidade, opacidade, texto e textura de cada
   elemento.
7. Só no fim: compilar, abrir o editor no mapa e pedir o print ao dono.

Cenário (`tools/ui_shots/<tela>.json`): estação, moedas, níveis de ofício, itens da bolsa e as capturas (receita,
quantidade, posição do mouse, tamanho da janela, `craft` + `t` para estados animados). A bolsa é recarregada a cada
captura. Telas novas precisam de um ramo no `UNWUIShotCommandlet` (hoje só `refining`).

## Regras do LyShine que o port segue (modo `bExactText`)

| Regra | No port |
|---|---|
| Tamanho da fonte = altura da linha (ascendente + descendente) em pixels de 1080p, não o em | escala = tamanho ÷ altura de linha da própria fonte medida a 1 (`FNWUILayout::TextScale`). Medido no refino: 131×17 px contra 135×17 no jogo |
| Família exata do `.font` | Nimbus regular, semibold, bold, médio, itálico; Pica (IM Fell) e itálico; Caslon (`GameFont` 0–7) |
| `wraptextsetting`, `linespacing`, `shrinktofit` | quebra por palavra, espaço entre linhas, encolhe até `minshrinkscale` |
| `UiLayoutFitter` horizontal | largura do elemento = largura do texto, em volta do pivô; filhos ancorados na borda acompanham (ex.: quantidade e nome do ingrediente) |
| Imagem `StretchedToFit` (tipo 4) | mantém a proporção, centrada |
| Mistura aditiva = (SrcAlpha, One) | a aditiva da Unreal é (One, One) e acende o RGB onde o alfa é 0; no modo fiel vai como translúcida |
| `scaletodevice` em contêiner de tela cheia | numa janela mais larga que 16:9 o jogo **estica** a tela na horizontal (o print fica oval). O port não estica; para comparar, redimensione o print para 16:9 |
| Flipbook (`UiFlipbookAnimation`) | sem imagem no canvas: o spec cria a imagem e o C++ troca o quadro (`blue_loop/blue_NN`) |
| Máscara (`UiMask` com imagem filha, ex. `mapmask.dds`) | a imagem da máscara não pode ser desenhada: esconder no spec |

## Lua: onde procurar cada coisa

- Estilos de texto: `tools/ui_styles.py` executa `_common/uistyle.lua` e grava `extract/ui/uistyle.json` (1207
  entradas, 588 estilos). `python3 tools/ui_styles.py FONT_STYLE_X` imprime um.
- Quem usa qual estilo: chamadas `SetTextStyle(self.Properties.<caminho>, UIStyle.<estilo>)` no Lua da tela. O nome
  da propriedade bate com o nome do elemento; vai para `"styles"` do spec.
- Botões: `slices/button.lua` `SetButtonStyle`. Padrão: fundo `buttonbg2023`, moldura `primarybuttonframe2023`, foco
  0,2, sem camadas de herói. Herói: fundo preto, sequência azul a 1, brilho inferior y −11 `#63d5ff`, borda herói 0
  (só no foco), texto `FONT_STYLE_BUTTON_HERO_2023`. Ícone (`SetIconPath`): liga `buttonIconHolder` com o sprite. No
  spec: `"buttons": {"/caminho": "default" | "hero"}`.
- Posição feita pelo Lua (`UiTransformBus.SetLocalPositionY`, `ScriptedEntityTweener.Set {x, y, w, h}`): procurar no
  Lua da tela e aplicar no C++ como `Translate` ou `Offsets` (ex.: `UpdateRequirements` sobe requisitos e quantidade
  28 px por linha de requisito; `SLIDER_STYLE_2` põe o valor numa caixa 250×40 acima da barra).
- Textos: chaves do jogo novas vão em `tools/strings/sistemas.json` (`chaves_do_jogo`). Nomes de estação:
  `@<tipo>_station` + `ui_tier` (as chaves `<Ofício>T<n>_MasterName` estão desalinhadas no jogo: T2 = "Tier 3").

## Anotado para a próxima sessão de HUD (dono, 2026-10-13)

Refino:
- Feito (2026-10-13), falta o teste do dono: arrastar o marcador e repetir − e + segurando (`button.lua`
  `repeatParamsDefault`); Espaço = ação `tryskip` (`uishared.xml`): máximo na escolha, pula a contagem, volta à estação;
  botão CONFECCIONAR piscava porque o laço do jogo tem 30 quadros a 30 fps e o port tocava 15 a 15 fps (agora
  `blue_00..29` e `gray_00..29` no apagado, carregados na abertura).
- Feito (2026-10-13): ordem da árvore pela `CategorySortOrder`/`GroupSortOrder` de `craftingsortinfo.lua`
  (`make_crafting.py` grava `category_order`/`group_order`).
- Feito (2026-10-13): taxa com `@ui_craft_fee` ("Taxa de Queda Eterna: 0,26") e moedas em centavos em todas as telas
  (`UNWInventoryComponent::FormatCoins`, ÷100 como `GetLocalizedCurrency`); XP com `@ui_craft_bonus` = reward × Σ Qty dos
  ingredientes × quantidade (`sistemas-005`). Primeira confecção ×3 (receitas feitas em `progression.crafted`); o port não tem buffs de facção nem boosts.
- Barra de título: só moedas. O jogo (`stationproperties.lua` → `screenheader.lua`) mostra azoth, moedas e o círculo
  do ofício da estação (`SetSkill1Circle`: nível, anel de progresso, ícone). O círculo é o slice 300×300 dentro de
  `SkillContainer1` (60×60), escalado pelo Lua: fazer com um print do original para comparar. Azoth o port não tem.
- Pin Recipe: já escondido no layout.
- Caixa-alta com acento ("FUNDIçãO"): `caseoverride` passa a usar `FText::ToUpper` (`FNWUILayout`, compartilhado).

Inventário, equipamento e armazém: spec em `docs/hud/inventario.md` (arrastar e soltar, seções, armazém).

Ordem das telas depois do refino: armazém (`catcontainer.lua`), atributos, ofícios, inventário (atalhos de
`re-local/hud-inventario.md`) e maestria (com a esteira 1).

`bExactText` está ligado só no refino. Ligar nas outras telas é uma linha no `Load` de cada uma, mas muda o tamanho
do texto delas (sai ~1,56× menor que hoje, como no jogo): fazer tela a tela com a captura.
