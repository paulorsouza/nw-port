# Teste em rede do clã (Guardaventos, build Development)

Decisão do dono (2026-10-13): os servidores rodam no PC do dono; o nw-core sai pelo Cloudflare Tunnel; o cliente
Windows é empacotado pelo dono e passado aos dois amigos, que têm o jogo. É uma exceção à regra de que cada um extrai
da própria instalação (`docs/plano-servidor.md`): o pacote é privado, só para eles.

## Peças

| Peça | Onde | Endereço |
|---|---|---|
| Postgres + nw-core | Docker neste PC, projeto `nw-rede` (`deploy/compose.yaml` + `deploy/compose.rede.yaml`) | `127.0.0.1:8091` → `https://<NW_REDE_HOST>` (Cloudflare Tunnel) |
| Servidor de jogo (Unreal dedicado Linux, Guardaventos) | Docker neste PC, mesmo projeto | UDP 7777 em `<IP do PC>`, alcançado pelo Cloudflare WARP |
| Cliente | Windows de cada um | `Jogar.bat` com `-nwbackend` e `-nwconnect` |

O nw-core de desenvolvimento (`tools/linux/backend_dev.sh`, porta 8090) continua separado, com outro banco.
Segredos da rede em `deploy/.env` (fora do git).

## Por que WARP

O provedor usa CGNAT (o salto depois do roteador é `<salto do provedor>`): abrir a porta no roteador não traz o jogo de
fora. O nw-core sai pelo túnel (HTTP); o jogo (UDP) chega pela rede privada do mesmo túnel: cada amigo liga o app
WARP da organização Zero Trust do dono e o cliente conecta em `<IP do PC>:7777` como se estivesse na rede local.
Reservar esse IP para o PC no DHCP do roteador, para ele não mudar.

## Uma vez

1. No terminal: `cloudflared tunnel login` (abre o navegador; escolher `<seu domínio>`). Depois `tools/linux/rede.sh tunnel`
   (cria o túnel `nw-rede`, o `<NW_REDE_HOST>` e a rota privada `<IP do PC>/32`).
2. Painel Zero Trust (one.dash.cloudflare.com), organização gratuita:
   - Settings > Custom pages / Team name: escolher o nome da equipe (os amigos digitam no WARP).
   - Settings > Authentication: método "One-time PIN" (código por e-mail).
   - Settings > WARP Client > Device enrollment permissions: regra com os e-mails do dono e dos dois amigos.
   - Settings > WARP Client > perfil Default > Split Tunnels: trocar para "Include IPs and domains" e incluir só
     `<IP do PC>/32` (o resto da internet deles não passa pelo Cloudflare).
   - Settings > Network > Proxy: ligado, com TCP e UDP.
3. Cada amigo: instalar o Cloudflare WARP (1.1.1.1), em Preferências > Conta > "Login with Cloudflare Zero Trust",
   digitar o nome da equipe, entrar com o código do e-mail e deixar ligado enquanto joga.

## Servidor (neste PC)

```bash
tools/linux/rede.sh build
```

```bash
tools/linux/rede.sh up
```

`build` compila o servidor na engine do código-fonte, cozinha Guardaventos e refaz as imagens (pesado, editor
fechado). `up` sobe Postgres, nw-core e jogo, e o túnel (<NW_REDE_HOST> + rede privada). `tools/linux/rede.sh invite` gera convites de conta; `tools/linux/rede.sh admin <conta>` libera o F10 só para essa conta (sem admin, o servidor recusa e o menu não abre);
`tools/linux/rede.sh down` para tudo (o banco fica); `tools/linux/rede.sh logs` mostra os logs.

## Cliente Windows (na máquina Windows do dono)

Com o projeto extraído e compilando no Windows (`docs/SETUP.md`), na raiz do repositório:

```bat
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%CD%\unreal\NWPort\NWPort.uproject" -platform=Win64 -clientconfig=Development -map=/Game/NW/Maps/L_SouthWindsward -build -cook -stage -pak -archive -archivedirectory="%CD%\deploy\build" -unattended -nop4 -utf8output
```

Na pasta `deploy\build\Windows`, criar `Jogar.bat`:

```bat
start "" "%~dp0NWPort.exe" -nwbackend=https://<NW_REDE_HOST> -nwconnect=<IP do PC>:7777
```

Zipar a pasta `Windows` e mandar só para os amigos (com o WARP ligado). Cada um entra pela tela de entrada com o convite
(`tools/linux/rede.sh invite`).

## Versão

Cliente e servidor precisam do mesmo C++ e dos mesmos dados (`Data/`): depois de mudar código, refazer os dois.
