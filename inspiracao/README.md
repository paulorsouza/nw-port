# Prints do jogo original (inspiração)

Prints do New World original tirados pelo dono (2026-10-13) para servir de referência às telas e ao visual do port.
As imagens ficam fora do git (conteúdo do jogo; `.gitignore`), só este índice entra. Quem trabalha em outra máquina
pede os prints ao dono ou tira os seus e acrescenta aqui.

Resolução: os prints 01–16 foram tirados com o jogo em **1920x1080 num monitor ultrawide** (a imagem sai esticada na
horizontal; para comparar com a captura do port, redimensione para 16:9, `docs/hud/README.md`). Do 17 em diante, o jogo
já estava na **resolução ultrawide nativa**. Os arquivos estão em 2000x845.

Para comparar com uma captura do port: `python3 tools/ui_compare.py inspiracao/<print>.webp extract/ui/shots/<captura>.png saida.png`.

| Arquivo | O que mostra | Útil para |
|---|---|---|
| `01_cidade_hud.webp` | Praça de uma cidade (Corinth) com o HUD normal: bússola com marcadores, rastreador de missões, barra de ações, vida e XP, buffs de comida | HUD em jogo, iluminação de cidade de dia |
| `02_inventario_equipamento.webp` | Inventário (Tab) com o equipamento à esquerda (armadura, armas, joias, munição, consumíveis rápidos), carga "Light 7.5", nível 65, gear score 598, atributos e resistências embaixo; inventário à direita com seções por categoria em 2 colunas, peso por seção, busca, filtros, ordenação, moedas e outras moedas no rodapé | Tela de inventário inteira, painel de equipamento |
| `03_inventario_aba_ferramentas.webp` | Mesma tela, aba de ferramentas de coleta e instrumentos, com os níveis dos ofícios (250) embaixo | Aba de ferramentas |
| `04_inventario_aba_ferramentas_2.webp` | Igual ao 03 (outro momento) | Aba de ferramentas |
| `05_inventario_aba_montaria.webp` | Aba de montaria: montaria (panda), equipamento da montaria e consumíveis | Aba de montaria (esteira 3) |
| `06_inventario_tooltip_buffs.webp` | Tooltip dos buffs ativos (pedra de afiar, comidas com tempo restante) | Tooltip de buffs |
| `07_inventario_tooltip_item.webp` | Tooltip de item da bolsa (botas Artifact): gear score, armaduras, perks, descrição, Tier, peso, requisito; rodapé com LALT Link to Chat, Compare, Actions | Tooltip de item, rodapé de ações |
| `08_inventario_arrastando_item.webp` | Arrastando um item da bolsa: o ícone segue o mouse sobre o personagem, o espaço de equipamento de destino acende, e aparece "Drop items" à direita (soltar fora larga no chão) | Arrastar e soltar |
| `09_inventario_filtros_colunas.webp` | Janela de filtros: número de colunas, filtros Equipment/Materials/All, lista de seções com visibilidade, largura e ordem | Filtros e seções do inventário |
| `10_inventario_e_armazem.webp` | Inventário e armazém lado a lado (Storage Shed 2217.6/6500.0), peso em cada peça, busca e filtros nos dois, seletor de assentamento "Windsward (Here)" e "Take All" | Armazém |
| `11_casa_cartao_compra.webp` | Cartão de interação de um lote de casa: "Windsward Housing", E View House, T Purchase House, preço 20,000.00 | Casas (cartão de várias opções) |
| `12_casa_interior.webp` | Dentro da casa: barra superior com Decorate, Upkeep, Hide UI, Exit; baús de armazenamento | Casas |
| `13_casa_modo_decoracao.webp` | Modo de decoração: catálogo por categoria com busca, contagem de decorações, pontuação, troféus à direita | Casas (decoração) |
| `14_cartao_atividades.webp` | Cartão de atividades da temporada (menu do Esc, aba Season) | Fora do escopo; estilo de tela cheia |
| `15_passe_temporada.webp` | Recompensas do passe de temporada | Fora do escopo; estilo de tela cheia |
| `16_configuracoes_video.webp` | Configurações > Vídeo (1920x1080, campo de visão 57–70) | Tela de configurações |
| `17_cidade_forja_ultrawide.webp` | Cidade com fundição ao lado, HUD em ultrawide nativo | HUD em ultrawide, iluminação |
| `18_posto_de_trocas_comprar.webp` | Posto de trocas, aba Comprar: categorias à esquerda, tabela com nome, preço, tier, gear score, perk, disponível; filtros e páginas | Mercado |
| `19_viagem_rapida.webp` | Viagem rápida em andamento (contagem regressiva com "Fast traveling...") | HUD de viagem |
| `20_tela_carregamento.webp` | Tela de carregamento com arte e texto de lore, barra de progresso, faixas laterais em ultrawide | Tela de carregamento |
