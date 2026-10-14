---
name: nw-iluminacao
description: Esteira 5 — iluminação do port do New World (ciclo do dia, sol, céu, exposição, luzes locais, interiores, GI, desempenho), comparando converter os dados do jogo com usar recursos da Unreal. Usar quando pedirem algo de luz, escuridão, exposição, ciclo do dia ou desempenho de iluminação.
---

# Iluminação (esteira 5)

Regras de trabalho: skill `nw-esteira` e `docs/esteiras.md`. Estado, dados levantados, tentativa desfeita e perguntas
para a RE: `docs/iluminacao.md` (leia antes). Testes: `docs/testes/iluminacao.md`. Mapas: `L_QuedaEterna`, depois
`L_SouthWindsward`.

## Como

- Nada de constante inventada: número sem fonte nos dados vira pergunta da skill `nw-re` antes de virar código.
- Cada escolha (converter x recurso da Unreal) é medida num ponto só (igreja de Queda Eterna) contra o print do jogo
  original, com `stat gpu` e VRAM, antes de ir para a cidade inteira.
- Luzes e volumes entram no mapa por script em `tools/ue/` numa etapa `iluminacao-NN` (`tools/etapas/iluminacao.*`),
  nunca à mão no editor.
- O mapa é compartilhado com a esteira 4 (Queda Eterna) e com coleta/montarias (Guardaventos): só atores numa pasta
  própria (`Iluminacao/`), sem mexer nos outros.
- Mostre o plano ao dono antes de mudar configuração do projeto (`DefaultEngine.ini` é compartilhado).
