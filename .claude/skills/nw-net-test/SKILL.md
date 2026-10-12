---
name: nw-net-test
description: Teste de rede local com clientes automáticos e print (tools/net_test.sh). Usar SOMENTE se o usuário pedir explicitamente esse teste; por padrão, abra o editor e peça o print ao usuário.
---

# Teste de rede automático (tools/net_test.sh)

O usuário pediu (2026-10-11) para não rodar testes automáticos do jogo: eles consumiram horas, disputaram
memória/VRAM com o New World original e não mostravam o que importa. Só rode se ele pedir com essas palavras.

```bash
NW_BOT= NW_TEST_SECONDS=60 tools/net_test.sh 1          # 1 cliente, sem bot
NW_SERVER=docker tools/net_test.sh 10                    # 10 bots contra o Docker
```

Ganchos de teste do cliente (em `NW_CLIENT_EXTRA`): `-nwtestdev=GoToOre|GiveTools`, `-nwtestgather=N`,
`-nwtestinv=...`, `-nwtestchat=...`, `-nwtestdie=N`, `-nwlang=en`. Logs e prints em `unreal/NWPort/Saved/NetTest/`.
