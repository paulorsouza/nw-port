# Como portar e refinar o combate de uma arma

Roteiro para quem vai trazer uma arma do New World para o port (lança, machado grande, mosquete, cajados...) ou
refinar uma que já existe. A meta é o combate ficar igual ao do jogo: números, janelas e animações saem dos dados
do jogo; o código só aplica as regras.

Armas no port hoje: espada e escudo, machadinha, arco e rapieira (estado de cada uma no fim). Regras gerais do
jogo (dano, crítico, reação, empurrão, tremida, stamina, bloqueio): `docs/combate-regras-jogo.md`.

## 0. Antes

- `tools/build_all.sh` rodado até a etapa 04 (tabelas, textos e receitas de animação extraídos).
- Ler `docs/combate-regras-jogo.md` (regras gerais do jogo: dano, empurrão, tremida, crítico, stamina).
- Editor da Unreal fechado nas etapas de import e compilação.
- Um trabalho pesado por vez (`docs/ESTADO.md`, Armadilhas).

## 1. Relatório do jogo

```bash
python3 tools/weapon_report.py <arma>
```

Gera `extract/research/<arma>_report.md` (e `.json`) com:

1. fragmentos do Mannequin com variante da arma: clipes por camada e janelas (dano, encadear, cancelar, esquiva,
   homing, som);
2. linhas de dano da arma (`javelindata_damagetable`);
3. stamina e recargas;
4. habilidades da maestria (nome, descrição, recarga, árvore);
5. feitiços (áreas, projéteis) e efeitos de status.

Arma que não está na lista `WEAPONS` do script: passe a tag e os prefixos
(`--tag Spear --adb player/player_spear.adb --base 2H_Melee --damage 2H_Spear_ --cost 2H_Spear --ability spear`).
Para descobrir a tag, abra o `.adb` da arma em `extract/models/animations/mannequin/adb/player/` e veja os
`Tags="..."` dos fragmentos.

Escreva o levantamento em `docs/armas/<arma>.md`: o que cada ação faz no jogo, com os números e a fonte, e no fim a
proposta (ações, montagens, DamageID, stamina, recarga, forma do acerto, código novo). Exemplos: `docs/armas/arco.md`,
`docs/armas/rapieira.md`.

## 2. Animações: receita no conjunto do jogador

Todas as armas do jogador moram no mesmo conjunto (`tools/sets/sword_shield.json`, mesmo esqueleto), com a parte
das armas em `tools/sets/player/armas.json`. Cada arma tem
um prefixo de montagem: `AM_` espada, `AM_Hx_` machadinha, `AM_Bw_` arco, `AM_Rp_` rapieira.

No `.json`:

- `weapons`: a malha da arma (`.cgf`/`.cdf`) e o socket (`right_hand_attach`, `left_hand_attach`).
- `montages`: `AM_Xx_Nome` → lista de seções; cada seção é a lista de clipes por camada (corpo inteiro primeiro,
  depois as camadas por cima, como no Mannequin). Combo de 3 golpes = 3 seções (`AM_Xx_LightCombo_1..3`).
- `fragments`: `AM_Xx_Nome` → fragmento(s) do `.adb` de onde saem as janelas (uma por seção).
- `window_overrides`: `{"AM_Xx_": {"adbs": [...], "tags": "..."}}` com o `.adb` e as tags da arma.
- `blendspaces` e `loops`: locomoção com a arma (strafe, sprint) e poses paradas (idle, guarda, mira).
- `hold_montages`: montagens que seguram o último quadro (carga do pesado, puxar o arco).
- `upper_montages`: montagens de corpo superior (fragmentos `_UpperBody`, clipes `_ub`): tocam no slot `UpperBody` do
  `ABP_NW`, por cima da locomoção, sem root motion (o arco atira andando).
- `montage_starts`: montagem que começa no meio do clipe (fração), como o `StartTime` do `.adb` (golpe carregado da
  rapieira em 0,74). As janelas são deslocadas no import e a montagem toca a partir desse ponto.
- `window_attach_adbs` (padrão) e `attach_adbs` (por prefixo): `.adb` do escopo da arma
  (`playermaleanims_1h_meleeattach.adb`), onde moram as janelas de dano exatas dos ataques básicos (`CAGE-Damage`).
  Arma sem escopo próprio: `"attach_adbs": []` no override.

O que o pipeline faz sozinho com o `.adb`:

- velocidade do clipe no fragmento (`speed="1.1"`): vira a taxa da montagem, e as janelas vão para o tempo da
  sequência (Tondo da rapieira toca a 2,2×; pesado da espada a 1,1×);
- janelas do `ProcLayer` com o tempo certo depois de clipe vazio;
- janelas de montagens que dividem o mesmo clipe são somadas (antes a última sobrescrevia).

Converter: `source tools/env.sh && tools/pesado.sh python3 tools/build_set.py tools/sets/sword_shield.json` (Blender +
nwbt, pesado).

## 3. Ações: tabela de ações da arma

Em `tools/make_datatables.py`:

- `<ARMA>_ACTIONS`: ação do port → (montagem, DamageID, CostID de stamina, próxima do combo). Nomes fixos que o
  C++ usa: `Light1..3`, `HeavyCharge`, `Heavy`, `HeavyCharged`, `SprintAttack`, `Dodge_F/B/L/R`, `BlockStart`,
  `BlockEnd`, `BlockIdle`, `BlockReact`, `BlockBreak`, `Hit_F/B/L/R`, `Death`, `StunLoop`, `Draw`, `Sheath`.
  Habilidades usam o nome que quiser (o mesmo em `ABILITY_IDS` e no `make_abilities.py`).
- `<ARMA>_SHAPES`: forma do acerto quando não é a lâmina: `Front` (cone: alcance cm, ângulo), `Around` (círculo),
  `Projectile` (alcance). Calibrar com F10 > Mostrar acertos (`nw.DebugHits 1`).
- `<ARMA>_ABILITY_IDS` (recarga do jogo) e `<ARMA>_EFFECTS` (atordoar alvo, buffs).
- `SETS["sword_shield"]`: acrescente `("DT_<Arma>Actions", ...)`.

Gerar os CSV (depois do `build_set.py`, que grava o manifest). A `DT_NWDamage` é uma só para todos os conjuntos e o
import do `lost_sword` roda depois do jogador: gere os CSV dos dois manifests, senão linhas de dano novas somem
(aconteceu com a manopla de gelo em 2026-10-15):

```bash
python3 tools/make_datatables.py extract/ue/sword_shield/manifest.json
python3 tools/make_datatables.py extract/ue/lost_sword/manifest.json
python3 tools/make_sound_map.py extract/ue/sword_shield/manifest.json
```

Colunas extras da ação (no dicionário `<ARMA>_EFFECTS`):

| Coluna | Para quê | Exemplo |
|---|---|---|
| `StatusID`, `StatusSeconds`, `StatusTick`, `StatusDamage`, `StatusMaxStacks`, `StatusMoveSpeed` | efeito no alvo (dano por tique = fração do dano de arma) | sangramento do Tondo |
| `SelfMoveSpeed`, `SelfMoveSeconds` | velocidade em si mesmo | pressa |
| `ConsumeStatusID`, `ConsumeDamage` | consome o status e causa ×dano restante | Terminar (1,1) |
| `FireTimes`, `FireDamageIDs` | disparos (s) e linha de cada um; em golpe corpo a corpo, a linha de cada janela de dano | Tiro Rápido, Rajada |
| `ProjectileSpeed`, `ProjectileRadius`, `GravityDelay`, `bPierce` | flecha (cm/s, cm, cm em linha reta) | 10000, 15, 500 |
| `ExplodeRadius`, `ExplodeDamageID` | explosão ao acertar/bater | Flecha Explosiva |
| `GroundRadius`, `GroundRange`, `GroundDelay` | área no ponto mirado | Chuva de Flechas |
| `NextAction` | ação que entra sozinha ao terminar | tiro → recarga |
| `StartAt` | fração do clipe onde começa (sai de `montage_starts`) | 0,74 |
| `CooldownGroup` | recarga compartilhada | Evadir (4 direções) |
| `bCancelsAny` | sai de qualquer ação | Evadir |
| `CounterSeconds`, `CounterAction` | postura que contra-ataca quem bate | Ripostar |
| `bStunBreaksOnDamage` | atordoamento acaba no primeiro dano | contra-ataque do Ripostar |

## 4. Sons, habilidades na tela, ícone

- Sons: `tools/make_sound_map.py` lê as janelas `Audio` do `.adb` sozinho. Som que não saiu: ver se o evento está em
  `extract/audio/sfx_index.csv`; banco não extraído entra em `tools/extract_audio.py` (`SFX_GROUPS`).
- Habilidades: entrada em `WEAPONS` de `tools/make_abilities.py` (árvores, ações, as três padrão, ícone).

## 5. C++: registrar a arma

Em `ANWCharacter::AddPlayerWeaponSets` (`NWCharacterArmas.cpp`), um `FNWWeaponSet` por arma: `Id`, malha, tabela de ações, blend space de
locomoção, ícone, habilidades padrão, mão (`Socket`), números da arma (`BaseDamage` na escala do port = dano do jogo ×
100/64, `CritChance`, `CritDamageMultiplier`, `StaggerDamage`, `BlockStaminaDamage`, `BlockStability`), carga do
pesado (`HeavyMinHold`, `HeavyFullCharge`) e `bAims` (arma de mira). O par de armas em uso (teclas 1 e 2) é o
`Loadout`; F10 > Trocar par de armas alterna entre os pares. Mecânicas que já existem e servem para qualquer arma:

| Mecânica | Onde | Dado |
|---|---|---|
| Combo leve com buffer e janelas | `UNWCombatComponent::CheckAction` | curvas `CanTransition`, `CanDodgeCancel`, `CanBlockCancel` |
| Carga do pesado (soltar cedo = leve) | `AttackPressed/Released`, `ReleaseCharge` | `HeavyMinHold`, `HeavyFullCharge` |
| Acerto por varredura da lâmina | `SweepWeapon` | curvas `WeaponTrail`/`DamageWindow` |
| Acerto em cone/círculo | `AreaHit` | `HitShape`, `HitRange`, `HitAngle` |
| Projétil (arremesso, flecha) | `SpawnProjectile`, `ANWProjectile` | `HitShape = Projectile` |
| Bloqueio, quebra de guarda | `ReceiveHit` | `BlockPowerLevel`, stamina |
| Esquiva com invulnerabilidade | `RequestDodge` | curva `Invulnerability` |
| Reação, empurrão, tremida, hit stop | `ReceiveHit`, `MulticastShowHit`, `MulticastHitStop` | tabela de dano |
| Atordoar, buffs | `ApplyStun`, `StartAction` | `<ARMA>_EFFECTS` |
| Crítico (sorteio da arma e pelas costas ≥ 135°), cabeça | `ReceiveHit`, `ANWProjectile::IsHeadHit` | `DmgCoefCrit`, `DmgCoefHead`, `NoBackstab` |
| Reação por categoria (PowerLevel), stagger da IA, limitador de 2 s | `ReceiveHit` | `PowerLevel(_AI)`, `StaggerDmgModifier` |
| Empurrão para trás e para o lado com decaimento | `Push` | `ImpactDistanceX/Y`, `ImpactDecayRate` |
| Tremida de câmera do jogo | `ANWCharacter::PlayCameraShake` | `CameraShakeID`… e `DT_NWCameraShake` |
| Efeitos de status com tique e acúmulo | `ApplyStatus`, `TickStatuses` | colunas `Status*` |
| Mira (arco), puxada, recarga | `RequestAim`, `ReleaseCharge` | `bAims`, `AimFullCharge` |
| Flecha com gravidade, atravessar, explodir | `SpawnProjectile`, `ANWProjectile` | colunas de projétil |
| Área no chão mirada | `FireGround` | `Ground*` |
| Ripostar, Evadir | `ReceiveHit`, `CheckAction` | `Counter*`, `bCancelsAny`, `CooldownGroup` |
| IA que usa a arma | `UNWBrainComponent` | estilos Duelist, Pack, Charger, Archer |

Mecânica nova (mira do arco, riposte da rapieira, sangramento...): escreva no componente de combate, com o número do
jogo e a fonte num comentário curto, e acrescente uma linha nesta tabela.

## 6. Importar e testar

```bash
tools/ue_import_all.sh 01   # compila
tools/ue_import_all.sh 02   # conjunto do jogador (animações, montagens, sons, tabelas)
```

Arma com montagens de corpo superior: o `ABP_NW` precisa do slot `UpperBody` (feito uma vez, à mão; passo a passo em
`docs/testes-g2.md`, rodada da arena).

Teste na arena (`L_Arena`, F10 > Ir para a arena):

1. Cada ação: animação certa, sem deslizar, arma na mão certa.
2. Janelas: o combo só encadeia na janela; esquiva cancela onde o jogo deixa; o golpe só acerta no trecho de dano
   (`nw.DebugHits 1`).
3. Números: dano no boneco = base × DmgCoef da linha (boneco sem armadura); stamina e recarga iguais ao relatório.
4. Reações: atordoamento, empurrão (distância e direção), tremida de câmera de quem bate e de quem apanha.
5. Sons nos tempos certos.
6. Contra a IA: F10 > Arena > Duelista / Matilha / Javalis / Onda mista.

Comparar com o jogo: gravar a mesma ação no New World e no port (mesma câmera) e ver lado a lado quadro a quadro.
Diferença achada: anote em `docs/armas/<arma>.md` (seção "Diferenças conhecidas") com o que falta para fechar.

## 7. Onde mexer para refinar

| Sintoma | Onde olhar |
|---|---|
| Golpe encadeia cedo/tarde | janelas do fragmento (relatório, seção 1) e `window_overrides` |
| Dano diferente | `DmgCoef` da linha, `WeaponBaseDamage`, mitigação (`docs/combate-regras-jogo.md`) |
| Alvo não reage / reage demais | `PowerLevel` × resistência do alvo, `HitStun` |
| Empurrão estranho | `ImpactDistanceX/Y/Z`, `ImpactDecayRate` |
| Câmera parada no golpe | `CameraShakeID`/`TargetCameraShakeID` da linha e `javelindata_camerashake` |
| Acerto de longe/perto demais | `<ARMA>_SHAPES` ou a malha da arma (lâmina) |
| Som fora de hora | janelas `Audio` do fragmento; `extract/ue/sword_shield/sounds.json` |

## Estado das armas

| Arma | Ações | Habilidades | Falta |
|---|---|---|---|
| Espada e escudo | completas | 6 | — |
| Machadinha | completas | 6 | — |
| Arco | tiro sem mira, mira, puxada (normal/carregado), recarga automática, esquiva, reações | Tiro Rápido, Tiro Evasivo, Tiro Penetrante, Disparo Venenoso, Flecha Explosiva, Chuva de Flechas | nuvem de veneno, indicador da Chuva, aim offset, corda animada, tremor da carga cheia; `docs/armas/arco.md` |
| Manopla de gelo (1a) | leve e pesado (projéteis), carga, bloqueio, esquiva, sacar/guardar; mana; efeitos Niagara do jogo | Estacas, Vento Arrepiante, Tempestade, Chuva, Torre, Sepultado | v1 com bugs conhecidos no básico e em habilidades; passivas, regeneração de mana, sons; `docs/armas/manopla-gelo.md` |
| Rapieira | combo de 3 (Light2 com 2 acertos), carga até 0,667 s, golpe de corrida, passo curto, bloqueio | Tondo, Rajada, Brandir e Terminar, Ripostar, Fleche, Evadir | caixa do Tondo, colisão desligada no Fleche, passivas da maestria, Interrupção (Wake); `docs/armas/rapieira.md` |

Diferenças conhecidas comuns: gear score e mitigação do jogador sem fórmula do jogo; a escala de dano do port é a
espada = 100 (o jogo usa 64).
