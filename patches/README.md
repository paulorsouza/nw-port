# Patches do nwbt

O código do nwbt fica no fork `paulorsouza/nw-buddy`, branch `nw-port` (nunca dar push para o upstream
`giniedp/nw-buddy`).

| Arquivo | O que é |
|---|---|
| `nwbt-nwport.patch` | Referência: o que o fork tem sobre o upstream `81b2302` (já está no commit base, não é aplicado) |
| `nwbt.patch` | O patch do nw-port sobre o commit base do fork (`BASE` em `tools/build_nwbt.py`), aplicado nas duas compilações |

## Compilar

Linux e Windows usam o mesmo script, que exporta o commit base para `.nwbt/build/src` (fim de linha LF), aplica o
`nwbt.patch` e grava a versão no binário. O checkout do fork (`vendor/nw-buddy` ou `NW_BUDDY`) não é alterado.

```bash
tools/pesado.sh python3 tools/build_nwbt.py --test
```

```powershell
./tools/windows/build_nwbt.ps1 -Test
```

`nwbt --version` mostra `nwport-patch=<base>-<hash do patch>`. `tools/check.sh` e `tools/windows/doctor.py` acusam
binário compilado com outro patch (ou sem versão) e testam a conversão por `@lista`.

## Mudar o nwbt

1. `python3 tools/build_nwbt.py --preparar` monta `.nwbt/build/src` (base + patch, repositório git próprio).
   Edite em `.nwbt/build/src/tools/nwbt`.
2. `python3 tools/build_nwbt.py --gerar-patch` grava o `patches/nwbt.patch` (o cabeçalho é mantido). O build
   recusa apagar `.nwbt/build/src` com edição que ainda não foi para o patch.
3. Recompile (`tools/pesado.sh python3 tools/build_nwbt.py --test`) e commite o `patches/nwbt.patch` junto com o
   `bin/nwbt`; no Windows, cada um recompila o `nwbt.exe`.

O `vendor/nw-buddy` não é usado para compilar (só como fonte do commit base). Mudança feita lá (fontes CRLF) entra no
patch copiando os arquivos para `.nwbt/build/src` antes do passo 2.

Para trocar o commit base (mudanças levadas para o fork), atualize `BASE` em `tools/build_nwbt.py` e refaça o
patch sobre ele.

Partes do patch e donos: `sliceLight` (luzes do slice) é da esteira 5; o resto (Windows, `@lista`, erro sem arquivo,
versão) é das ferramentas.
