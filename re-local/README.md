# re-local

Cache das respostas de engenharia reversa desta máquina, gravado pelo agente `nw-re` (`docs/ghidra.md`). Fora do
git: só este arquivo é versionado. Pode apagar; o agente refaz a pergunta quando precisar.

```
re-local/
  perguntas/<id>.md      pergunta e resposta
  valores/<área>.json    números → tools/make_rules.py → unreal/NWPort/Data/regras.json (opcional)
  simbolos.md            endereços e funções já revertidos
```

O port não depende desta pasta: os valores aplicados estão no código (`FNWRules::Num(id, valor)`).
