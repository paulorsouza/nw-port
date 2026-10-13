# Itens do port

Regras do port para armas, armaduras, joias e ferramentas. Decididas com o dono em 2026-10-13; não seguem o jogo
original. Fabricação, tempo e espectros: `docs/oficio.md`. Lista das perks do jogo com a decisão de cada uma (coleção
`decisoes`, documento = `PerkID`, campos `v` fica|sai, `nova`, `nota`): página
https://claude.ai/artifact/QCkhEVRxbP2xFBCGMpjiYD.

## Dano e defesa

- Dois tipos de dano: físico e mágico. Não há dano elemental.
- Não existem Energizado, Reforço, Pressa nem debuffs vindos de perk.
- Suporte é cura e escudo. Escudo = % × vida máxima de quem aplica, com o tempo e os acúmulos do efeito original.
- Ameaça é da classe de tanque, não de item.
- Sorte só existe nos troféus.

## Atributo e gear score

- Cada item tem um atributo só (FOR, DES, INT, FOC ou CON). Sem atributo dividido, sem Maxify.
- O atributo é sorteado ao fabricar e pode ser sorteado de novo na mesa do ofício do item, pagando materiais de ofício.
  Só o atributo muda; GS, qualidade e perks ficam.
- O GS muda só esse atributo. Ex.: item de ferro em GS 800 dá CON 40.
- GS ao fabricar: sorteado na faixa do material, mais o bônus do artesão.
- Sortear o GS muda só o GS e o atributo. Qualidade, perks, dano e resistência não mudam.
- Aprimorar o GS é sortear de novo na faixa da fabricação (material + bônus do artesão pelo nível, teto `gs_maximo`,
  provisório 800): pode subir ou descer. Na mesa do ofício do item, aba "Melhorar itens", pagando materiais de ofício.

## Qualidade

- Um valor de 0 a 100% para o item inteiro.
- Posiciona todos os valores do item na faixa: valor = mínimo + (máximo − mínimo) × qualidade. Ex.: resistência
  500–520 com 10% dá 502.
- Valores com uma casa decimal.
- Sorteio de qualidade mínima até 100%. A mínima sobe com o artesão: ofício no máximo, 20%; ofício, roupas de ofício
  e troféus no máximo, 50%.
- No tooltip: atributo, resistências, qualidade, perks.

## Material

- O material decide só a resistência base (ex.: couro 200–220).
- O material não muda as perks.

## Perks

- Perk com faixa fixa (ex.: amplificação mágica 6–10%), sem depender do GS nem do material.
- 3 perks por item; anel, 2. Sem repetir; no máximo 1 perk de habilidade por item.
- Perk de habilidade só cai na arma daquela habilidade.
- Redução de recarga sem teto: recarga = base ÷ (1 + total). 100% deixa a recarga na metade.
- Chance crítica com teto de 50% (o jogo não tem resistência a crítico). Dano crítico sem teto.

| Equipamento | Perks |
|---|---|
| Arma | Chance crítica, Dano crítico, Redução de recarga, Roubo de vida, de habilidade da arma; cajado da vida e manopla do vácuo também Cura causada |
| Escudo | Chance crítica, Dano crítico, Redução de recarga, Regeneração de vigor, Roubo de vida, de habilidade de escudo |
| Armadura | Amplificação física, Amplificação mágica, Vida máxima, Redução de recarga, Regeneração de vigor, Roubo de vida |
| Amuleto | Vida máxima, Redução de recarga, Regeneração de vigor, Recuperação Reforçada, Recuperação Revigorante |
| Anel | Chance crítica, Dano crítico, Cura causada, Ventos Curativos, Regeneração de vigor |
| Brinco | Redução de recarga, Regeneração de vigor, Brinde de Reforço, Regeneração |
| Ferramenta | Velocidade, Rendimento e Experiência de coleta |

Perks de habilidade com efeito trocado (sem Energizado, Reforço, velocidade ou debuff): campo `nota` na página.

## Slot

- 1 slot por arma e armadura. Joias não têm slot.
- Gema: 1, 2, 3, 4 ou 5% pelo tier (T1–T5).
  - Armadura: resistência mágica, resistência física, híbrida 50/50, híbrida 70/30 mais mágica, híbrida 70/30 mais
    física. A híbrida divide o total.
  - Arma: chance crítica ou dano crítico.
- Vidro rúnico: amplificação de dano físico ou mágico. Usa o mesmo slot da gema (um ou outro) e precisa de uma gema para
  ser equipado.

## Vínculo (bind)

- Vinculado ao equipar: o item fica preso ao personagem quando é equipado. Pode ser desvinculado na mesa do ofício do
  item (aba "Melhorar itens"): volta a "vincula ao equipar"; o custo sobe com a qualidade do item.
- Vinculado ao pegar: o item cai do saque já preso ao personagem e não se desvincula.

## Assinatura

- Item fabricado com qualidade de 80% ou mais leva a assinatura do artesão: o nick aparece no item.
- Cada item assinado tem um número de série (artesão + sequencial) emitido pelo nw-core e guardado em `props.serial`. O
  registro (`item_serials`) guarda dono e local; toda gravação confere que o número existe e aparece uma vez só somando
  bolsa, armazém e posto de trocas, senão a gravação inteira é recusada (migração 0009, `backend/src/routes/serials.rs`).

## Pendente (balanceamento)

- Faixa de GS de cada material e bônus do artesão.
- Curva do GS para o atributo.
- Custo de aprimorar e de desvincular.
- Faixa de cada perk (sugestões na página) e resistência base de cada material.
- Quais itens caem vinculados ao pegar.
- Custo de sortear o atributo de novo.
