# re-local

Respostas da engenharia reversa do `NewWorld.exe`, geradas na máquina de cada um pelo agente `nw-re`
(`.claude/skills/nw-re/SKILL.md`, `docs/ghidra.md`). Fora do git: só este arquivo é versionado.

```
re-local/
  perguntas/<esteira>-NNN.md   pergunta e resposta
  valores/<área>.json          fatos numéricos → tools/make_rules.py → unreal/NWPort/Data/regras.json
  simbolos.md                  endereços e funções já revertidos
  colunas.json                 colunas da damagetable lidas pelo exe (tools/re/colunas.py)
  <tema>.md                    análises por tema
```

Sem esta pasta o port roda com os valores padrão do código (`FNWRules::Num(id, padrão)`).
