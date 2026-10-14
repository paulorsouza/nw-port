# Testar com backend

Todo teste no editor usa o nw-core local: o Play entra sempre com a mesma conta e o mesmo personagem, e a bolsa, o
equipamento, a posição, os níveis, o armazém e os brindes ficam salvos entre um Play e outro.

## Uso

1. Subir o nw-core (uma vez por boot; rodar de novo não faz nada se já estiver no ar):

   ```bash
   tools/backend_dev.sh
   ```

   Sobe o Postgres (`nw-core-db`, `backend/compose.yaml`), compila o nw-core se o fonte mudou (fila de trabalho
   pesado), sobe a API em `127.0.0.1:8090` e cria a conta `editor` com o personagem `Exilado` se não existirem.
2. Abrir o editor e dar Play (sozinho, "Standalone"/"Play in Viewport"). No log: `Play com backend: Exilado (editor)`.
3. Parar o Play salva tudo (também salva a cada 30 s).

Outros comandos: `tools/backend_dev.sh status | log | down`. `down` para só a API; o banco continua com os dados.

## Como funciona

- `backend/dev.env` (valores de teste, no repositório): `NW_BACKEND`, `NW_SERVER_SECRET`, `NW_PIE_ACCOUNT`,
  `NW_PIE_CHARACTER` e a senha `NW_TEST_PASSWORD`.
- No Play sozinho (PIE standalone) o `ANWGameMode::BeginPlay` chama `UNWBackend::EnablePie`, que lê o `dev.env`. O
  jogador local entra pelo mesmo caminho de um cliente no servidor dedicado: login, `join` (passe) e resgate
  (`/internal/tickets/redeem`); o personagem salvo é aplicado (posição, se for o mesmo mapa; bolsa, equipamento,
  progresso). Salva pelo `PUT /internal/characters/{id}/state` no timer e ao fechar o mundo.
- A sessão da conta fica só na memória do Play (não grava `[NW.Session]` no GameUserSettings).
- Com o nw-core fora do ar, o Play segue sem backend (kit inicial, nada salvo) e avisa na tela "Jogando sem backend".
  Para jogar sem backend de propósito: `tools/backend_dev.sh down`.
- Com backend, os níveis de ofício contam (sem backend eram livres). Para testar algo que pede nível ou material, use
  os botões do F10.

## Limpar o personagem

Apagar e recriar volta ao começo (kit inicial):

```bash
docker exec nw-core-db psql -U nw -d nw -c "delete from characters where lower(name) = 'exilado'"
```

```bash
tools/backend_dev.sh
```

## Outros testes

- `tools/play_local.sh`: servidor dedicado + cliente com a tela de entrada (usa o mesmo nw-core; precisa do editor
  fechado).
- `tools/net_test.sh`: vários clientes sem tela (contas `NW_TEST_ACCOUNTS`).
- Testes do nw-core: `backend/README.md` (cada teste cria um banco temporário).
