---
name: nw-re
description: Pede uma engenharia reversa específica do NewWorld.exe (regra, fórmula ou valor que os dados extraídos não dizem) ao agente nw-re em segundo plano e aplica a resposta no port. Usar quando um número ou comportamento do jogo não tiver fonte nos datatables, .adb, slices ou actionlists, ou quando o dono pedir "pergunta pra RE".
---

# Pedir e aplicar engenharia reversa

Antes de perguntar, procure se já foi respondido: `re-local/valores/`, `perguntas/`,
`combate-cliente.md`, `ia-mobs.md`, `simbolos.md`. Dano, crítico, grit e mitigação são de servidor: não estão no
cliente.

## Perguntar

1. Crie `re-local/perguntas/<esteira>-NNN.md` (próximo número livre da sua esteira; um arquivo por pergunta
   para várias pessoas não colidirem):

   ```
   # <esteira>-NNN Título curto
   - Esteira: armas | coleta | montarias | sistemas
   - Estado: aberta
   - Pergunta: o que o código do jogo faz, em uma ou duas frases.
   - Para quê: onde o port vai usar (arquivo, função) e o que precisa (valor, fórmula, unidade, sinal).
   - Pistas: strings, nomes de classe, colunas, cvars que você já viu.
   ```

2. Rode o agente em segundo plano e siga com outra coisa:
   `Agent(subagent_type="nw-re", run_in_background=true, prompt="Responda re-local/perguntas/<esteira>-NNN.md")`.
   Uma pergunta por agente. Não rode a RE você mesmo.

## Aplicar a resposta

1. Leia a resposta no arquivo da pergunta e os ids novos em `re-local/valores/<área>.json`.
2. `python3 tools/make_rules.py` (gera `Data/regras.json`).
3. Número: `FNWRules::Num(TEXT("id"), padrão)` no C++ (include `NWRules.h`). Regra ou fórmula: implemente e cite o
   id em comentário no ponto onde ela vale. Prova H (hipótese): use só se não houver alternativa e diga no teste.
4. `python3 tools/re_check.py`: o id tem que aparecer como aplicado.
5. Item de teste em `docs/testes/<esteira>.md` e doc da esteira atualizado (tire o item de "Sem fonte").
6. No arquivo da pergunta, mude o estado para `aplicada` e diga onde.
