# Plano das mecânicas

Combate de espada e escudo para 3 jogadores, com os números e as janelas do jogo onde existem e regras simplificadas no resto.

## Decisões

- **Rede desde o início.** Listen server (um dos três hospeda). O servidor decide acerto, dano, stamina e morte; o cliente só prevê animação e movimento.
- **C++ para as regras, Blueprint para animação e ajuste.** Um `UNWCombatComponent` em C++ (estado, stamina, dano, replicação). Anim Blueprint e Character Blueprint ficam em Blueprint. Sem GAS: para este escopo é mais peça do que problema.
- **Dados do jogo em DataTables.** CSV gerados do `.datasheet` e do `.adb`, importados no Unreal. Ajustar número não exige recompilar.

## Fontes de dados

| Dado | Origem | Exemplo (espada) |
|---|---|---|
| Coeficiente de dano, tipo, hitstun, stun no bloqueio, power level, empurrão | `javelindata_damagetable` | `1H_Sword_Attack1`: coef 1.0, Slash, stun 15, block stun 10, empurrão 0.5 |
| Custo de stamina | `javelindata_staminacosts_player` | esquiva 45, bloqueio 10, ataques 0 |
| Janelas por ataque | `ProcLayer` do `.adb` → curvas nas sequências | ataque leve 1: rastro 0.31–0.43 s, cancela com esquiva a partir de 0.50 s, encadeia em 0.83 s |
| Invulnerabilidade da esquiva | `.adb` (`Invulnerability`) | 0–0.5 s |
| Janela de dano explícita | `.adb` (`CAGE-Damage`) | ataque pesado: 0.17–0.33 s |
| Sons por ataque e passos | `.adb` (`Audio`) + `.animevents` + `extract/audio/sfx_index.csv` | `Play_RHSword_Atk_PrimA1` em 0.13 s |

Pendente de confirmar: a unidade de `HitStun` (15–50) e a escala de `PowerLevel`.

## Regras

**Movimento.** CharacterMovement. Com a arma em mãos o personagem gira com a câmera (strafe); andar 1.65 m/s, correr 4 m/s (valores do blend space), sprint sem custo de stamina (como no jogo).

**Ataque.** Leve: combo de 3 golpes; o próximo entra quando a curva `CanTransition` liga, e o input é guardado no buffer até lá. Pesado: um golpe. Sprint attack. Durante `Homing` o personagem gira para o alvo/câmera. Root motion vem da montagem.

**Acerto.** No servidor, enquanto `WeaponTrail` (ou `DamageWindow`, quando existir) estiver ligada: varredura de cápsula da base à ponta da lâmina entre o quadro anterior e o atual. Um acerto por alvo por golpe.

**Dano.** `dano = dano_base_arma × DmgCoef × (1 − mitigação_armadura)`. Dano base e mitigação são constantes por enquanto; atributos, gear score, crítico e tipos de dano ficam de fora.

**Bloqueio.** Segurar o botão levanta o escudo (`BlockStart` → `BlockIdle`). Bloqueia golpes num arco frontal de 180°. Golpe bloqueado não tira vida: tira stamina e aplica `BlockHitStun` (`BlockReact`). Stamina zerada no bloqueio → `BlockBreak` e atordoamento curto.

**Esquiva.** Direção do input (F/B/L/R). Custa 45 de stamina. Invulnerável de 0 a 0.5 s. Cancela ataques nas janelas `CanDodgeCancel`.

**Stamina.** Máximo 100. Regenera depois de um atraso curto. Em 0 fica exausto: sem esquiva e sem bloqueio até recuperar parte.

**Reação.** Golpe com `PowerLevel` acima da resistência do alvo interrompe a ação e toca `Hit_F/B/L/R` pelo lado do impacto, com o empurrão da tabela. Durante `ActivateGrit` o alvo não é interrompido.

**Morte.** Vida 0 → `Death`, ragdoll opcional, renasce no ponto inicial depois de alguns segundos.

**Alvos.** Primeiro um boneco de treino (só recebe golpes). Depois um NPC do jogo extraído com animações (`nwbt models npcs`), com Behavior Tree simples: aproxima, ataca, às vezes bloqueia.

## Etapas

Cada etapa termina com um teste no editor (PIE).

| # | Entrega | Teste |
|---|---|---|
| M1 | Projeto, import, personagem com câmera e locomoção (strafe/sprint) | andar e correr com as animações certas |
| M2 | Ataques leve/pesado com combo, buffer e janelas | combo de 3 golpes encadeia só na janela |
| M3 | Boneco de treino, acerto por varredura, dano, números flutuantes | dano bate com `DmgCoef` |
| M4 | Bloqueio, esquiva com invulnerabilidade, stamina e exaustão | golpe na janela da esquiva não acerta; stamina zerada quebra o bloqueio |
| M5 | Reações, interrupção, morte e renascimento; sons e rastro da arma | trocar golpes entre dois personagens |
| M6 | Rede: 3 jogadores em listen server | PIE com 3 clientes; os três veem o mesmo resultado |
| M7 | Inimigo do jogo com IA | lutar contra um NPC |

O mapa/cenário é uma frente separada, depois de M3.
