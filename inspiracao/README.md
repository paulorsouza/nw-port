# Screenshots of the original game (inspiration)

Screenshots of the original New World taken by the maintainer (2026-10-13) as a reference for the port's screens and
look. The images stay out of git (game content; `.gitignore`); only this index is committed. Anyone working on another
machine asks the maintainer for the screenshots, or takes their own and adds them here.

Resolution: screenshots 01–16 were taken with the game at **1920x1080 on an ultrawide monitor** (the image comes out
stretched horizontally; to compare with a port capture, resize to 16:9, `docs/game/ui/README.md`). From 17 on, the game
was already at the **native ultrawide resolution**. The files are 2000x845.

To compare with a port capture: `python3 tools/ui_compare.py inspiracao/<screenshot>.webp extract/ui/shots/<capture>.png out.png`.

| File | What it shows | Useful for |
|---|---|---|
| `01_cidade_hud.webp` | A city plaza (Corinth) with the normal HUD: compass with markers, quest tracker, action bar, health and XP, food buffs | In-game HUD, daytime city lighting |
| `02_inventario_equipamento.webp` | Inventory (Tab) with the equipment on the left (armor, weapons, jewelry, ammo, quick consumables), load "Light 7.5", level 65, gear score 598, attributes and resistances below; inventory on the right with sections by category in 2 columns, weight per section, search, filters, sorting, coins and other currencies in the footer | Whole inventory screen, equipment panel |
| `03_inventario_aba_ferramentas.webp` | Same screen, tab for gathering tools and instruments, with the trade skill levels (250) below | Tools tab |
| `04_inventario_aba_ferramentas_2.webp` | Same as 03 (another moment) | Tools tab |
| `05_inventario_aba_montaria.webp` | Mount tab: mount (panda), mount equipment and consumables | Mount tab (area 3) |
| `06_inventario_tooltip_buffs.webp` | Tooltip of the active buffs (whetstone, foods with time remaining) | Buffs tooltip |
| `07_inventario_tooltip_item.webp` | Tooltip of a bag item (Artifact boots): gear score, armor values, perks, description, Tier, weight, requirement; footer with LALT Link to Chat, Compare, Actions | Item tooltip, actions footer |
| `08_inventario_arrastando_item.webp` | Dragging an item from the bag: the icon follows the mouse over the character, the target equipment slot lights up, and "Drop items" appears on the right (releasing outside drops it on the ground) | Drag and drop |
| `09_inventario_filtros_colunas.webp` | Filters window: number of columns, Equipment/Materials/All filters, list of sections with visibility, width and order | Inventory filters and sections |
| `10_inventario_e_armazem.webp` | Inventory and Storage side by side (Storage Shed 2217.6/6500.0), weight on each item, search and filters on both, settlement selector "Windsward (Here)" and "Take All" | Storage |
| `11_casa_cartao_compra.webp` | Interaction card of a housing plot: "Windsward Housing", E View House, T Purchase House, price 20,000.00 | Houses (multi-option card) |
| `12_casa_interior.webp` | Inside the house: top bar with Decorate, Upkeep, Hide UI, Exit; storage chests | Houses |
| `13_casa_modo_decoracao.webp` | Decoration mode: catalog by category with search, decoration count, score, trophies on the right | Houses (decoration) |
| `14_cartao_atividades.webp` | Season activity card (Esc menu, Season tab) | Out of scope; full-screen style |
| `15_passe_temporada.webp` | Season pass rewards | Out of scope; full-screen style |
| `16_configuracoes_video.webp` | Settings > Video (1920x1080, field of view 57–70) | Settings screen |
| `17_cidade_forja_ultrawide.webp` | City with a smelter next to it, HUD at native ultrawide | Ultrawide HUD, lighting |
| `18_posto_de_trocas_comprar.webp` | Trading Post, Buy tab: categories on the left, table with name, price, tier, gear score, perk, available; filters and pages | Market |
| `19_viagem_rapida.webp` | Fast travel in progress (countdown with "Fast traveling...") | Travel HUD |
| `20_tela_carregamento.webp` | Loading screen with art and lore text, progress bar, side bands at ultrawide | Loading screen |
