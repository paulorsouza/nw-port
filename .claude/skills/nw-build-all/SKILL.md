---
name: nw-build-all
description: Extrai e converte os assets do New World da instalação local (tools/linux/build_all.sh, 14 etapas retomáveis). Usar ao montar o projeto do zero, depois de instalar as ferramentas, ou para refazer uma etapa (ex.: "refaz a etapa 10", "extrai os assets").
---

# Extrair e converter (tools/linux/build_all.sh)

Antes:
1. Confirme com o usuário que o New World original e a Unreal estão fechados (o `systemd-oomd` fecha programas por
   pressão de memória com vários trabalhos pesados juntos). Não rode junto com outro trabalho pesado.
2. `NW_GAME` aponta para a pasta do jogo (padrão `/mnt/jogos/Steam/steamapps/common/New World`).
3. Etapa 00 confere as ferramentas; se faltar algo, siga a skill `setup`.

Rodar (em segundo plano, leva horas):

```bash
tools/linux/build_all.sh          # todas as que faltam
tools/linux/build_all.sh 10       # refaz só a etapa 10
```

Conferir: `ls extract/.etapas` (00.ok…13.ok). Etapa interrompida por memória: rodar de novo, ela recomeça.
Tabela de etapas e tempos: `docs/SETUP.md`. Próximo passo: skill `nw-ue-import`.

No Windows nativo: `python tools/build_all.py [NN]` (skill `nw-windows`). Os scripts `.sh` daqui ficam em `tools/linux/` (skill `nw-linux`).
