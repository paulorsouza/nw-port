---
name: nw-play-local
description: Sobe backend + servidor dedicado + cliente na tela de entrada para o usuário jogar (tools/play_local.sh). Usar SOMENTE quando o usuário pedir explicitamente para abrir o jogo para ele.
---

# Abrir o jogo para o usuário (tools/play_local.sh)

Regra do projeto: nunca rode o jogo por conta própria para testar. Só quando o usuário pedir.

Antes: editor da Unreal fechado (usa o mesmo binário), New World original fechado (VRAM de 8 GB), nada pesado
rodando.

```bash
tools/play_local.sh            # mapa L_SouthWindsward
```

Fechar a janela do cliente encerra tudo. Logs em `unreal/NWPort/Saved/Play/`. O usuário entra com a conta
dele; menu de dev no F10.
