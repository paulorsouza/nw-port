---
name: nw-re
description: Pede uma engenharia reversa específica do NewWorld.exe (regra, fórmula ou valor que os dados extraídos não dizem) ao agente nw-re em segundo plano e aplica a resposta no port. Usar quando um número ou comportamento do jogo não tiver fonte nos datatables, .adb, slices ou actionlists, ou quando o dono pedir "pergunta pra RE".
---

# Pedir e aplicar engenharia reversa

O repositório guarda o método, não as respostas. Ninguém depende de RE já feita: se precisar de um fato do
executável, pergunte de novo. O agente usa o cache local (`re-local/`, fora do git) quando ele existe.

Dano, crítico, grit e mitigação são de servidor: não estão no cliente.

## Perguntar

Rode o agente em segundo plano e siga com outra coisa. Uma pergunta por agente; não rode a RE você mesmo.

```
Agent(subagent_type="nw-re", run_in_background=true, prompt="""
<esteira>-<assunto curto>
- Pergunta: o que o código do jogo faz, em uma ou duas frases.
- Para quê: onde o port vai usar (arquivo, função) e o que precisa (valor, fórmula, unidade, sinal).
- Pistas: strings, nomes de classe, colunas, cvars que você já viu.
""")
```

## Aplicar a resposta

1. Número: constante no código com o valor da resposta, ou `FNWRules::Num(TEXT("<área>.<nome>"), valor)` quando
   precisar ajustar sem recompilar. O padrão do `Num` é o próprio valor: o port tem que funcionar sem `re-local/`.
2. Regra ou fórmula: implemente no ponto onde ela vale. Prova H (hipótese): use só se não houver alternativa e diga
   no teste.
3. Nada de endereço do executável no código, nos docs ou nas specs.
4. Item de teste em `docs/testes/<esteira>.md` e doc da esteira atualizado (tire o item de "Sem fonte").
