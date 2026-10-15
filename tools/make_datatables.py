#!/usr/bin/env python3
"""Gera os CSV das DataTables do Unreal (structs FNWDamageRow e FNWActionRow) a partir dos dados do jogo.

uso: make_datatables.py extract/ue/sword_shield/manifest.json
Saída: extract/ue/<conjunto>/data/DT_NWDamage.csv e DT_NWActions.csv (importados pelo tools/ue/import_nw.py).
Requer os datasheets em JSON em extract/data (nwbt unpack --x-datasheet json).
"""
import csv
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from armas_manopla import (ICEGAUNTLET_ABILITY_IDS, ICEGAUNTLET_ACTIONS, ICEGAUNTLET_EFFECTS,  # noqa: E402
                           ICEGAUNTLET_SHAPES, ICEGAUNTLET_TRANSITIONS, ICEGAUNTLET_DAMAGE_OVERRIDES)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT = os.path.join(ROOT, "extract", "data", "sharedassets", "springboardentitites", "datatables")

# Ação -> (montagem, DamageID do jogo, CostID de stamina do jogo, próxima do combo).
ACTIONS = {
    "Light1": ("AM_LightCombo_1", "1H_Sword_Attack1", "Attack_1H_Sword_Primary", "Light2"),
    "Light2": ("AM_LightCombo_2", "1H_Sword_Attack1", "Attack_1H_Sword_Primary", "Light3"),
    "Light3": ("AM_LightCombo_3", "1H_Sword_Attack1_Ender", "Attack_1H_Sword_Primary", ""),
    "Heavy": ("AM_Heavy", "1H_Sword_Attack2", "Attack_1H_Sword_Heavy", ""),
    # Segurar o botão: carga (Attack_Start_Charge); soltar cedo = leve, no meio = Heavy, carga cheia = HeavyCharged.
    "HeavyCharge": ("AM_HeavyCharge", "", "", ""),
    "HeavyCharged": ("AM_HeavyCharged", "1H_Sword_Attack2_Charged", "Attack_1H_Sword_Heavy", ""),
    "SprintAttack": ("AM_SprintAttack", "1H_Sword_Attack_Charge", "Attack_1H_Sword_Run", ""),
    "ShieldBash": ("AM_ShieldBash", "OH_Shield_Attack_Bash", "", ""),
    "ShieldRush": ("AM_ShieldRush", "OH_Shield_Attack_Rush", "", ""),
    "SpinAttack": ("AM_SpinAttack", "1H_Sword_Attack_Spin", "Attack_1H_Sword_Special", ""),
    # Transtorno (Ability_Sword_ReverseStab): 180% = 1H_Sword_Attack_Stab (a linha Swipe é de outra versão).
    "SwipeStab": ("AM_SwipeStab", "1H_Sword_Attack_Stab", "Attack_1H_Sword_Special", ""),
    # Golpe com Salto: mesmo clipe do golpe de corrida (fragmento Ability_Sword_Leap_*).
    "LeapingStrike": ("AM_SprintAttack", "1H_Sword_Attack_Charge", "Attack_1H_Sword_Special", ""),
    "FinalStand": ("AM_FinalStand", "", "", ""),
    "StunLoop": ("AM_StunLoop", "", "", ""),
    "Shout": ("AM_Shout", "", "", ""),
    "Dodge_F": ("AM_Dodge_F", "", "Dodge_Roll_Fast", ""),
    "Dodge_B": ("AM_Dodge_B", "", "Dodge_Roll_Fast", ""),
    "Dodge_L": ("AM_Dodge_L", "", "Dodge_Roll_Fast", ""),
    "Dodge_R": ("AM_Dodge_R", "", "Dodge_Roll_Fast", ""),
    "BlockStart": ("AM_BlockStart", "", "", ""),
    "BlockEnd": ("AM_BlockEnd", "", "", ""),
    "BlockIdle": ("AM_BlockIdle", "", "", ""),
    "BlockReact": ("AM_BlockReact", "", "", ""),
    "BlockBreak": ("AM_BlockBreak", "", "", ""),
    "Hit_F": ("AM_Hit_F", "", "", ""),
    "Hit_B": ("AM_Hit_B", "", "", ""),
    "Hit_L": ("AM_Hit_L", "", "", ""),
    "Hit_R": ("AM_Hit_R", "", "", ""),
    "Death": ("AM_Death", "", "", ""),
    "Draw": ("AM_Draw", "", "", ""),
}


# Lost: ataques do inimigo usando as linhas de dano da espada do jogador (as do NPC usam outra escala).
LOST_ACTIONS = {
    "Attack1": ("AM_Attack1", "1H_Sword_Attack1", "", ""),
    "Attack2": ("AM_Attack2", "1H_Sword_Attack1", "", ""),
    "Heavy": ("AM_Heavy", "1H_Sword_Attack2", "", ""),
    "Leap": ("AM_Leap", "1H_Sword_Attack_Spec3", "", ""),
    "Aggro": ("AM_Aggro", "", "", ""),
    "Hit_F": ("AM_Hit_F", "", "", ""),
    "Hit_B": ("AM_Hit_B", "", "", ""),
    "Hit_L": ("AM_Hit_L", "", "", ""),
    "Hit_R": ("AM_Hit_R", "", "", ""),
    "Death": ("AM_Death", "", "", ""),
}
# Criaturas (G3): DamageID do jogo da tabela da criatura com prefixo (as tabelas repetem nomes: Attack_Gore, Fall...).
BOAR_ACTIONS = {
    "Attack1": ("AM_Attack1", "Boar_Attack_Gore", "", ""),
    "Heavy": ("AM_Heavy", "Boar_Attack_Charge", "", ""),
    "Aggro": ("AM_Aggro", "", "", ""),
    "Growl": ("AM_Growl", "", "", ""),
    # Investida em três partes, encadeadas pela IA: preparar (sem dano), correr (Attack_Charge), frear.
    "ChargeStart": ("AM_ChargeStart", "", "", ""),
    "ChargeEnd": ("AM_ChargeEnd", "", "", ""),
    "Hit_F": ("AM_Hit_F", "", "", ""), "Hit_B": ("AM_Hit_B", "", "", ""),
    "Hit_L": ("AM_Hit_L", "", "", ""), "Hit_R": ("AM_Hit_R", "", "", ""),
    "Death": ("AM_Death", "", "", ""),
}
WOLF_ACTIONS = {
    "Attack1": ("AM_Attack1", "Wolf_Attack_01_Bite", "", ""),
    "Attack2": ("AM_Attack2", "Wolf_Attack_03_Bite_Combo", "", ""),
    "Heavy": ("AM_Heavy", "Wolf_Attack_02_Lunge", "", ""),
    "Leap": ("AM_Leap", "Wolf_Attack_02_Lunge", "", ""),
    "Aggro": ("AM_Aggro", "", "", ""),
    "Hit_F": ("AM_Hit_F", "", "", ""), "Hit_B": ("AM_Hit_B", "", "", ""),
    "Hit_L": ("AM_Hit_L", "", "", ""), "Hit_R": ("AM_Hit_R", "", "", ""),
    "Death": ("AM_Death", "", "", ""),
}
CREATURE_SHAPES = {"Heavy": ("Front", 260, 70), "Leap": ("Front", 300, 60), "Attack1": ("Front", 200, 90), "Attack2": ("Front", 200, 90)}
# Tabelas de dano das criaturas: (arquivo, prefixo).
CREATURE_DAMAGE = [("javelindata_damagetable_boar", "Boar_"),
                   ("charactertables/wolf_datatables/javelindata_damagetable_wolf", "Wolf_")]
# Formato do acerto (padrão: lâmina). Alcance em cm, ângulo = abertura total do cone. Valores iniciais, a calibrar com nw.DebugHits.
HIT_SHAPES = {
    "Heavy": ("Front", 230, 50),
    "HeavyCharged": ("Front", 230, 50),
    "SprintAttack": ("Front", 260, 70),
    "ShieldBash": ("Front", 190, 100),
    "ShieldRush": ("Front", 220, 110),
    "SpinAttack": ("Around", 260, 360),
    "SwipeStab": ("Front", 270, 80),
    "LeapingStrike": ("Front", 260, 70),
}
# Habilidade -> AbilityID do jogo (recarga em javelindata_cooldowns_player).
ABILITY_IDS = {
    "ShieldBash": "Ability_Sword_Bash",
    "ShieldRush": "Ability_Sword_Rush",
    "SpinAttack": "Ability_Sword_WhirlingBlade",
    "SwipeStab": "Ability_Sword_ReverseStab",
    "Shout": "Ability_Sword_FinalStand",
    "FinalStand": "Ability_Sword_FinalStand",
    "LeapingStrike": "Ability_Sword_LeapingStrike",
}
# Efeitos das descrições do jogo (weaponabilities.loc.xml): atordoar o alvo (s); buff em si (s, redução de dano).
EFFECTS = {
    "ShieldBash": {"TargetStun": 2.0},
    "FinalStand": {"BuffSeconds": 8.0, "BuffDamageReduction": 0.3},
}
# Machadinha (mesmo esqueleto; montagens AM_Hx_*). Reações e morte reaproveitam as da espada.
HATCHET_ACTIONS = {
    "Light1": ("AM_Hx_LightCombo_1", "Hatchet_Damage_PrimaryAttack", "", "Light2"),
    "Light2": ("AM_Hx_LightCombo_2", "Hatchet_Damage_PrimaryAttack", "", "Light3"),
    "Light3": ("AM_Hx_LightCombo_3", "Hatchet_Damage_PrimaryAttack_Ender", "", ""),
    "HeavyCharge": ("AM_Hx_HeavyCharge", "", "", ""),
    "Heavy": ("AM_Hx_Heavy", "Hatchet_Damage_HeavyAttack", "", ""),
    "HeavyCharged": ("AM_Hx_HeavyCharged", "Hatchet_Damage_HeavyAttack_Charged", "", ""),
    "SprintAttack": ("AM_Hx_SprintAttack", "Hatchet_Damage_SprintAttack1", "", ""),
    "Dodge_F": ("AM_Hx_Dodge_F", "", "Dodge_Roll_Fast", ""),
    "Dodge_B": ("AM_Hx_Dodge_B", "", "Dodge_Roll_Fast", ""),
    "Dodge_L": ("AM_Hx_Dodge_L", "", "Dodge_Roll_Fast", ""),
    "Dodge_R": ("AM_Hx_Dodge_R", "", "Dodge_Roll_Fast", ""),
    "BlockStart": ("AM_Hx_BlockStart", "", "", ""),
    "BlockEnd": ("AM_Hx_BlockEnd", "", "", ""),
    "BlockIdle": ("AM_Hx_BlockIdle", "", "", ""),
    "BlockReact": ("AM_Hx_BlockReact", "", "", ""),
    "BlockBreak": ("AM_Hx_BlockBreak", "", "", ""),
    "Hit_F": ("AM_Hit_F", "", "", ""),
    "Hit_B": ("AM_Hit_B", "", "", ""),
    "Hit_L": ("AM_Hit_L", "", "", ""),
    "Hit_R": ("AM_Hit_R", "", "", ""),
    "Death": ("AM_Death", "", "", ""),
    "StunLoop": ("AM_StunLoop", "", "", ""),
    "Draw": ("AM_Hx_Draw", "", "", ""),
    "Sheath": ("AM_Hx_Sheath", "", "", ""),
    "RagingTorrent": ("AM_Hx_RagingTorrent", "Hatchet_Damage_RapidStrikes", "", ""),
    "RendingThrow": ("AM_Hx_RendingThrow", "Hatchet_Damage_RendingThrow", "", ""),
    "SocialDistancing": ("AM_Hx_SocialDistancing", "Hatchet_Damage_SocialDistancing", "", ""),
    "InfectedThrow": ("AM_Hx_Special4", "Hatchet_Damage_InfectedThrow", "", ""),
    "FeralRush": ("AM_Hx_Special3", "Hatchet_Damage_SprintAttack1", "", ""),
    "Berserk": ("AM_Hx_Special5", "", "", ""),
}
HATCHET_SHAPES = {
    "Heavy": ("Front", 230, 60), "HeavyCharged": ("Front", 230, 60), "SprintAttack": ("Front", 260, 70),
    "FeralRush": ("Front", 260, 80), "RagingTorrent": ("Front", 230, 90),
    "RendingThrow": ("Projectile", 2500, 0), "SocialDistancing": ("Projectile", 2500, 0), "InfectedThrow": ("Projectile", 2500, 0),
}
HATCHET_ABILITY_IDS = {a: f"Ability_Hatchet_{a}" for a in
                       ("Berserk", "FeralRush", "RagingTorrent", "RendingThrow", "SocialDistancing", "InfectedThrow")}
# Berserk: +20% de dano por 5 s (descrição do jogo).
HATCHET_EFFECTS = {"Berserk": {"BuffSeconds": 5.0, "BuffDamageBonus": 0.2}}
# Arco (montagens AM_Bw_*; docs/game/weapons/bow.md). Clique sem mira = Light1 (tiro rápido); com mira (botão direito):
# HeavyCharge puxa, soltar antes de 0,5 s = AimShot, depois = ChargedShot; recarga automática (NextAction).
BOW_ACTIONS = {
    "Light1": ("AM_Bw_HipFire", "BowAttack_Hipshot", "Attack_2H_Bow_Primary", ""),
    "HeavyCharge": ("AM_Bw_Charge", "", "Attack_2H_Bow_Aim_Charge", ""),
    "AimStart": ("AM_Bw_AimStart", "", "", ""),
    "AimIdle": ("AM_Bw_AimIdle", "", "", ""),
    "AimEnd": ("AM_Bw_AimEnd", "", "", ""),
    "AimEndCharged": ("AM_Bw_AimEndCharged", "", "", ""),
    "AimEndFromShot": ("AM_Bw_AimEndFromShot", "", "", ""),
    "AimEndFromShotCharged": ("AM_Bw_AimEndFromShotCharged", "", "", ""),
    "AimShot": ("AM_Bw_AimShot", "BowAttack1", "Attack_2H_Bow_Aim", ""),
    "ChargedShot": ("AM_Bw_ChargedShot", "BowAttack2", "Attack_2H_Bow_Primary_Charge", ""),
    "Reload": ("AM_Bw_Reload", "", "", ""),
    "ReloadCharged": ("AM_Bw_ReloadCharged", "", "", ""),
    **{f"Dodge_{d}": (f"AM_Bw_Dodge_{d}", "", "Dodge_Roll_Fast", "") for d in "FBLR"},
    **{f"Hit_{d}": (f"AM_Bw_Hit_{d}", "", "", "") for d in "FBLR"},
    "Death": ("AM_Death", "", "", ""),
    "StunLoop": ("AM_Bw_StunLoop", "", "", ""),
    "Draw": ("AM_Bw_Draw", "", "", ""),
    "Sheath": ("AM_Bw_Sheath", "", "", ""),
    "RapidShot": ("AM_Bw_RapidShot", "Bow_RapidAttack_Standard", "", ""),
    "EvadeShot": ("AM_Bw_EvadeShot", "BowAttackJumpBack", "", ""),
    "PenetratingShot": ("AM_Bw_PenetratingShot", "Bow_Penetrating_Attack", "", ""),
    "PoisonShot": ("AM_Bw_PoisonShot", "Bow_PoisonShot", "", ""),
    "ExplosiveArrow": ("AM_Bw_ExplosiveArrow", "Bow_ExplosionShot_Projectile", "", ""),
    "RainOfArrows": ("AM_Bw_RainShot", "Bow_RainOfArrows_Attack", "", ""),
}
# Flecha (radb Bow): 100 m/s, reta 5 m, raio 0,15 m (carregada 0,275), alcance 100 m.
ARROW = {"ProjectileSpeed": 10000, "ProjectileRadius": 15, "GravityDelay": 500}
BOW_SHAPES = {a: ("Projectile", 10000, 0) for a in ("Light1", "AimShot", "ChargedShot", "RapidShot", "EvadeShot", "PoisonShot", "ExplosiveArrow")}
BOW_SHAPES["PenetratingShot"] = ("Projectile", 5000, 0)
BOW_SHAPES["RainOfArrows"] = ("Around", 400, 360)
BOW_ABILITY_IDS = {"RapidShot": "Ability_Bow_RapidShot", "EvadeShot": "Ability_Bow_EvadeShot", "PenetratingShot": "Ability_Bow_PenetratingShot",
                   "PoisonShot": "Ability_Bow_PoisonShot", "ExplosiveArrow": "Ability_Bow_SplinterShot", "RainOfArrows": "Ability_Bow_RainofArrows"}
BOW_EFFECTS = {
    "Light1": {**ARROW, "FireTimes": "0.4"},
    "AimShot": {**ARROW, "FireTimes": "0.033", "NextAction": "Reload"},
    "ChargedShot": {**ARROW, "ProjectileRadius": 27.5, "FireTimes": "0.033", "NextAction": "ReloadCharged"},
    "RapidShot": {**ARROW, "FireTimes": "0.333;0.567;0.8",
                  "FireDamageIDs": "Bow_RapidAttack_Standard;Bow_RapidAttack_Standard;Bow_RapidAttack_KnockBack"},
    "EvadeShot": {**ARROW, "FireTimes": "0.733"},
    "PenetratingShot": {**ARROW, "FireTimes": "0.033", "bPierce": True},
    # Disparo Venenoso: veneno de 20% do dano de arma por segundo por 7 s no alvo (a nuvem de 3 m ainda não existe).
    # Disparo Venenoso: onde cai, nuvem de 3 m por 6 s; quem entra fica envenenado (20% do dano de arma por segundo, 7 s).
    "PoisonShot": {**ARROW, "FireTimes": "0.033", "CloudRadius": 300, "CloudSeconds": 6,
                   "StatusID": "Bow_Poison", "StatusSeconds": 7, "StatusTick": 1, "StatusDamage": 0.2},
    "ExplosiveArrow": {**ARROW, "ExplodeRadius": 250, "ExplodeDamageID": "Bow_ExplosionShot_Explosion"},
    # Chuva de Flechas: área de 4 m no ponto mirado (até 50 m), acerto em 0,7 s.
    "RainOfArrows": {"GroundRadius": 400, "GroundRange": 5000, "GroundDelay": 0.7},
}
# Rapieira (montagens AM_Rp_*; docs/game/weapons/rapier.md). Bloqueio e guardar reaproveitam os da machadinha.
RAPIER_ACTIONS = {
    "Light1": ("AM_Rp_LightCombo_1", "1H_Rapier_PrimaryAttack", "Attack_1H_Rapier_Primary", "Light2"),
    "Light2": ("AM_Rp_LightCombo_2", "1H_Rapier_PrimaryTwoThirdsAttack", "Attack_1H_Rapier_Primary", "Light3"),
    "Light3": ("AM_Rp_LightCombo_3", "1H_Rapier_PrimaryAttack_Ender", "Attack_1H_Rapier_Primary", "Light1"),
    "HeavyCharge": ("AM_Rp_HeavyCharge", "", "", ""),
    "HeavyCharged": ("AM_Rp_HeavyCharged", "1H_Rapier_HeavyAttack_Charged", "Attack_1H_Rapier_Heavy_Charged", ""),
    "SprintAttack": ("AM_Rp_SprintAttack", "1H_Rapier_SprintAttack", "Attack_1H_Rapier_Run", ""),
    **{f"Dodge_{d}": (f"AM_Rp_Dodge_{d}", "", "Dodge_Roll", "") for d in "FBLR"},
    "BlockStart": ("AM_Hx_BlockStart", "", "", ""), "BlockEnd": ("AM_Hx_BlockEnd", "", "", ""),
    "BlockIdle": ("AM_Hx_BlockIdle", "", "", ""), "BlockReact": ("AM_Hx_BlockReact", "", "", ""),
    "BlockBreak": ("AM_Hx_BlockBreak", "", "", ""),
    **{f"Hit_{d}": (f"AM_Hit_{d}", "", "", "") for d in "FBLR"},
    "Death": ("AM_Death", "", "", ""), "StunLoop": ("AM_StunLoop", "", "", ""),
    "Draw": ("AM_Rp_Draw", "", "", ""), "Sheath": ("AM_Hx_Sheath", "", "", ""),
    "Tondo": ("AM_Rp_Tondo", "1H_Rapier_Slash", "", ""),
    "Flurry": ("AM_Rp_Flurry", "1H_Rapier_Flurry_Strike_1", "", ""),
    "Flourish": ("AM_Rp_Flourish", "1H_Rapier_Flourish", "", "Finish"),
    "Finish": ("AM_Rp_Finish", "1H_Rapier_Finish", "", ""),
    "Riposte": ("AM_Rp_RiposteStart", "", "", ""),
    "RiposteLoop": ("AM_Rp_RiposteLoop", "", "", ""),
    "RiposteOut": ("AM_Rp_RiposteOut", "1H_Rapier_Riposte_Stun", "", ""),
    "RiposteCancel": ("AM_Rp_RiposteCancel", "", "", ""),
    "Fleche": ("AM_Rp_FlecheStart", "", "", ""),
    "FlecheLoop": ("AM_Rp_FlecheLoop", "1H_Rapier_Aerial_Lunge", "", ""),
    "FlecheEnd": ("AM_Rp_FlecheEnd", "", "", ""),
    **{f"Evade_{d}": (f"AM_Rp_Evade_{d}", "", "", "EvadeAttack") for d in "FBLR"},
    "EvadeAttack": ("AM_Rp_EvadeAttack", "1H_Rapier_PrimaryAttack", "", "Light2"),
}
RAPIER_SHAPES = {"Tondo": ("Front", 535, 60), "Flourish": ("Front", 350, 90), "RiposteOut": ("Front", 200, 90),
                 "FlecheLoop": ("Around", 110, 360)}
RAPIER_ABILITY_IDS = {"Tondo": "Ability_Rapier_Slash", "Flurry": "Ability_Rapier_Flurry", "Flourish": "Ability_Rapier_Flourish",
                      "Riposte": "Ability_Rapier_Riposte", "Fleche": "Ability_Rapier_Aerial_Lunge_Intro",
                      **{f"Evade_{d}": "Ability_Rapier_Evasion" for d in "FBLR"}}
BLEED = {"StatusID": "Rapier_Bleed_DOT", "StatusSeconds": 10, "StatusTick": 1.01, "StatusDamage": 0.1, "StatusMaxStacks": 3}
RAPIER_EFFECTS = {
    "Tondo": BLEED,
    "Flurry": {"FireDamageIDs": ";".join(f"1H_Rapier_Flurry_Strike_{i}" for i in range(1, 6))},
    "Finish": {"ConsumeStatusID": "Rapier_Bleed_DOT", "ConsumeDamage": 1.1},
    # Ripostar: 1 s de postura (sem dano e sem reação); golpe recebido → contra-ataque que atordoa 2 s (acaba no 1º dano).
    "Riposte": {"CounterSeconds": 1.0, "CounterAction": "RiposteOut", "NextAction": "RiposteLoop"},
    "RiposteLoop": {"CounterAction": "RiposteOut", "NextAction": "RiposteCancel"},
    "RiposteOut": {"TargetStun": 2.0, "bStunBreaksOnDamage": True},
    "Fleche": {"NextAction": "FlecheLoop"},
    "FlecheLoop": {"NextAction": "FlecheEnd"},
    **{f"Evade_{d}": {"CooldownGroup": "Evade", "bCancelsAny": True} for d in "FBLR"},
}
# Reações por PowerLevel (tabela de reação do jogo): Hit1 = R1 (leve), Hit = R2, Hit3 = R3 (forte), Down = R4 (cai e
# levanta: GetUp entra sozinho). Entram em todas as tabelas do jogador.
MELEE_REACTIONS = {**{f"Hit1_{d}": (f"AM_Hit1_{d}", "", "", "") for d in "FBLR"},
                   **{f"Hit3_{d}": (f"AM_Hit3_{d}", "", "", "") for d in "FBLR"},
                   **{f"Down_{d}": (f"AM_Down_{d}_1", "", "", "") for d in "FBLR"},
                   **{f"GetUp_{d}": (f"AM_Down_{d}_2", "", "", "") for d in "FBLR"}}
BOW_REACTIONS = {**MELEE_REACTIONS, **{f"Hit1_{d}": (f"AM_Bw_Hit1_{d}", "", "", "") for d in "FBLR"},
                 **{f"Hit3_{d}": (f"AM_Bw_Hit3_{d}", "", "", "") for d in "FBLR"}}
REACTION_NEXT = {f"Down_{d}": {"NextAction": f"GetUp_{d}"} for d in "FBLR"}
ACTIONS.update(MELEE_REACTIONS)
HATCHET_ACTIONS.update(MELEE_REACTIONS)
BOW_ACTIONS.update(BOW_REACTIONS)
RAPIER_ACTIONS.update(MELEE_REACTIONS)
ICEGAUNTLET_ACTIONS.update(MELEE_REACTIONS)
for fx in (EFFECTS, HATCHET_EFFECTS, BOW_EFFECTS, RAPIER_EFFECTS, ICEGAUNTLET_EFFECTS):
    fx.update(REACTION_NEXT)

# Janela do jogo que libera cada ação no meio de outra (nomes das CAGE-TransitionInstant/Buffered do .adb; as
# curvas "<nome>" e "Buffer_<nome>" saem do import). Ação sem entrada usa CanTransition.
TRANSITIONS = {
    "DT_NWActions": {"HeavyCharge": "CanAttack_Heavy", "ShieldBash": "CanAttack_ShieldBash", "ShieldRush": "CanAttack_ShieldRush",
                     "SpinAttack": "CanAttack_SpinAttack", "SwipeStab": "CanAttack_SwipeStabAttack", "FinalStand": "CanAttack_LastStand"},
    "DT_HatchetActions": {"HeavyCharge": "CanAttack_Heavy", "Berserk": "CanAttack_Berserk", "SocialDistancing": "CanAttack_DistancingThrow",
                          "SprintAttack": "CanAttack_Hatchet_Sprint"},
    "DT_BowActions": {"Light1": "CanAttack_Bow_HipFire", "HeavyCharge": "Charge_Bow", "AimShot": "Shoot_NoCharge_Bow",
                      "RapidShot": "CanAttack_RapidShot", "EvadeShot": "CanAttack_JumpBackShot", "PenetratingShot": "CanAttack_PenetratingShot",
                      "PoisonShot": "CanAttack_PoisonShot", "RainOfArrows": "CanAttack_RainOfArrows", "ExplosiveArrow": "CanAttack_SpreadShot"},
    "DT_RapierActions": {"HeavyCharge": "CanAttack_Heavy", "Tondo": "CanAttack_Slash", "Flurry": "CanAttack_Flurry",
                         "Flourish": "CanAttack_Flourish", "Finish": "CanAttack_Finish", "Riposte": "CanAttack_Riposte",
                         "Fleche": "CanAttack_Aerial_Lunge", **{f"Evade_{d}": "CanAttack_Evasion" for d in "FBLR"}},
    "DT_IceGauntletActions": ICEGAUNTLET_TRANSITIONS,
}
SETS = {"sword_shield": [("DT_NWActions", ACTIONS, HIT_SHAPES, ABILITY_IDS, None),
                         ("DT_HatchetActions", HATCHET_ACTIONS, HATCHET_SHAPES, HATCHET_ABILITY_IDS, HATCHET_EFFECTS),
                         ("DT_BowActions", BOW_ACTIONS, BOW_SHAPES, BOW_ABILITY_IDS, BOW_EFFECTS),
                         ("DT_RapierActions", RAPIER_ACTIONS, RAPIER_SHAPES, RAPIER_ABILITY_IDS, RAPIER_EFFECTS),
                         ("DT_IceGauntletActions", ICEGAUNTLET_ACTIONS, ICEGAUNTLET_SHAPES, ICEGAUNTLET_ABILITY_IDS, ICEGAUNTLET_EFFECTS)],
        "lost_sword": [("DT_LostActions", LOST_ACTIONS, HIT_SHAPES, {}, {})],
        "boar": [("DT_BoarActions", BOAR_ACTIONS, CREATURE_SHAPES, {}, {})],
        "wolf": [("DT_WolfActions", WOLF_ACTIONS, CREATURE_SHAPES, {}, {})]}


# Colunas da damagetable no DT_NWDamage (FNWDamageRow) e a coluna do jogo de onde cada uma vem, quando o nome muda.
# Colunas que o jogo lê e o port não: python3 tools/re_check.py.
DAMAGE_COLS = ["WeaponCategory", "DamageType", "DmgCoef", "HitStun", "BlockHitStun", "PowerLevel", "BlockPowerLevel", "ImpactDistance",
               "Unblockable", "AttackType", "ImpactDistanceX", "ImpactDecayRate", "HitStun_AI", "PowerLevel_AI", "ImpactDistance_AI",
               "DmgCoefCrit", "CritHitStun", "CritPowerLevel", "CritImpactDistance", "DmgCoefHead", "NoBackstab", "NoHeadshot",
               "StaggerDmgModifier", "StaggerResistModifier", "BlockStaminaDmgMod", "CameraShakeID", "TargetCameraShakeID",
               "BlockCameraShakeID", "BlockTargetCameraShakeID", "CritCameraShakeID", "CritTargetCameraShakeID"]
DAMAGE_SOURCE = {"ImpactDistance": "ImpactDistanceY", "ImpactDistance_AI": "ImpactDistanceY_AI", "CritImpactDistance": "CritImpactDistanceY"}
# Fora de propósito, com o motivo (texto).
DAMAGE_IGNORED = {"CanCrit": "dano.cancrit_vazio: o nwbt grava célula vazia como False, então a coluna não diz quem crita"}


def hitstun_start(manifest, montage, fallback):
    """Início do procclip HitStun da montagem (janelas do manifest); reação sem janela usa o mais comum do conjunto."""
    for sec in manifest.get("windows", {}).get(montage, []):
        for w in sec["windows"]:
            if w.get("type") == "HitStun":
                return w["start"]
    return fallback


def set_hitstun_start(name):
    """Início mais comum do procclip HitStun nos .adb do conjunto (window_adbs de tools/sets/<conjunto>.json)."""
    import collections
    import adb_windows
    import xml.etree.ElementTree as ET
    import setspec
    spec = setspec.load(os.path.join(ROOT, "tools", "sets", name + ".json"))
    adb_dir = os.path.join(ROOT, "extract", "models", "animations", "mannequin", "adb")
    starts = collections.Counter()
    for f in spec.get("window_adbs", ["player/player_sword.adb", "player/player_1h_melee.adb", "player/player.adb"]):
        for frag in ET.parse(os.path.join(adb_dir, f)).getroot().find("FragmentList"):
            for fr in frag.findall("Fragment"):
                starts.update(round(w["start"], 3) for w in adb_windows.windows(fr) if w.get("type") == "HitStun")
    return starts.most_common(1)[0][0] if starts else 0.2


def rows(name):
    return json.load(open(os.path.join(DT, name + ".datasheet.json")))["rows"]


def main():
    manifest = json.load(open(sys.argv[1]))
    out = os.path.join(os.path.dirname(sys.argv[1]), "data")
    os.makedirs(out, exist_ok=True)
    anim_path = f"/Game/NW/{manifest['name']}/Anims"

    montages = set()
    for name, secs in manifest["montages"].items():
        montages |= {name} if len(secs) == 1 else {f"{name}_{i + 1}" for i in range(len(secs))}

    damage = {r["DamageID"]: r for r in rows("javelindata_damagetable")}
    for table, prefix in CREATURE_DAMAGE:
        if os.path.exists(os.path.join(DT, table + ".datasheet.json")):
            damage.update({prefix + r["DamageID"]: r for r in rows(table)})
    stamina = {r["CostID"]: r["StaminaCost"] for r in rows("javelindata_staminacosts_player")}
    mana = {r["CostID"]: r["ManaCost"] for r in rows("javelindata_manacosts_player")}

    tables = SETS[manifest["name"]]
    # Tabela de dano única para todos os conjuntos: linhas das armas do jogador e das criaturas. Colunas e regras:
    # docs/game/combat.md (empurrão X/Y e decaimento, colunas _AI, crítico, cabeça, stagger, tremidas).
    used = sorted(d for d in damage if d.startswith(("1H_Sword", "OH_Shield", "Hatchet_", "1H_Rapier", "Bow", "IceMagic_", *(p for _, p in CREATURE_DAMAGE)))
                  or damage[d].get("WeaponCategory") in ("Bow", "Bow_Heavy", "Rapier"))
    cols, source = DAMAGE_COLS, DAMAGE_SOURCE

    def cell(r, c):
        if c in ICEGAUNTLET_DAMAGE_OVERRIDES.get(r.get("DamageID"), {}):
            return ICEGAUNTLET_DAMAGE_OVERRIDES[r["DamageID"]][c]
        v = r.get(source.get(c, c))
        if c in ("Unblockable", "NoBackstab", "NoHeadshot"):
            return str(bool(v))
        if v is None or v == "":
            return "" if c.endswith("ID") or c in ("AttackType", "WeaponCategory", "DamageType") else 0
        return str(v).strip() if isinstance(v, str) else v

    with open(os.path.join(out, "DT_NWDamage.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Name", *cols])
        for did in used:
            w.writerow([did, *(cell(damage[did], c) for c in cols)])
    # Tremidas de câmera (SShakeParams do CryEngine; ver FNWCameraShakeRow).
    with open(os.path.join(out, "DT_NWCameraShake.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Name", "ShiftX", "ShiftY", "ShiftZ", "AngleX", "AngleY", "AngleZ", "FadeIn", "Sustain", "FadeOut", "Frequency", "Randomness", "FlipVec"])
        for r in rows("javelindata_camerashake"):
            g = lambda k: r.get(k) or 0
            w.writerow([str(r["CameraShakeID"]).strip(), g("ShakeShiftX"), g("ShakeShiftY"), g("ShakeShiftZ"), g("ShakeAngleX"), g("ShakeAngleY"),
                        g("ShakeAngleZ"), g("FadeInDuration"), g("SustainDuration"), g("FadeOutDuration"), g("Frequency"), g("Randomness"),
                        str(bool(r.get("FlipVec")))])

    cooldown = {r["AbilityID"]: r["Time"] for r in rows("javelindata_cooldowns_player")}
    set_start = set_hitstun_start(manifest["name"])
    missing = []
    for table_name, actions, shapes, ability_ids, effects in tables:
        if effects is None:
            effects = EFFECTS
        with open(os.path.join(out, f"{table_name}.csv"), "w", newline="") as f:
            w = csv.writer(f)
            fx_cols = ["StatusID", "StatusSeconds", "StatusTick", "StatusDamage", "StatusMaxStacks", "StatusMoveSpeed",
                       "SelfMoveSpeed", "SelfMoveSeconds", "ConsumeStatusID", "ConsumeDamage",
                       "FireTimes", "FireDamageIDs", "ProjectileSpeed", "ProjectileRadius", "GravityDelay", "bPierce",
                       "ExplodeRadius", "ExplodeDamageID", "GroundRadius", "GroundRange", "GroundDelay", "NextAction",
                       "StartAt", "CooldownGroup", "bCancelsAny", "CounterSeconds", "CounterAction", "bStunBreaksOnDamage",
                       "TransitionCurve", "CloudRadius", "CloudSeconds", "HitStunStart", "ManaCost", "AttackAction", "ProjectileFx"]
            text_cols = {"StatusID", "ConsumeStatusID", "FireTimes", "FireDamageIDs", "ExplodeDamageID", "NextAction",
                         "CooldownGroup", "CounterAction", "TransitionCurve", "AttackAction", "ProjectileFx"}

            def default(c):
                return "" if c in text_cols else "False" if c.startswith("b") else 1 if c in ("StatusTick", "StatusMaxStacks") else 0
            w.writerow(["Name", "Montage", "DamageID", "StaminaCost", "ComboNext", "HitShape", "HitRange", "HitAngle", "Cooldown",
                        "TargetStun", "BuffSeconds", "BuffDamageReduction", "BuffDamageBonus", *fx_cols])
            for action, (montage, did, cost_id, nxt) in actions.items():
                if montage not in montages:
                    missing.append(montage)
                shape, rng, ang = shapes.get(action, ("Blade", 0, 0))
                cd = cooldown.get(ability_ids.get(action, ""), 0)
                fx = dict(effects.get(action, {}))
                # Montagem que começa no meio do clipe (montage_starts do conjunto).
                fx.setdefault("StartAt", manifest.get("montage_starts", {}).get(montage, 0))
                fx.setdefault("TransitionCurve", TRANSITIONS.get(table_name, {}).get(action, ""))
                fx.setdefault("ManaCost", mana.get(cost_id, 0))
                if action.startswith(("Hit", "Down_")):
                    fx.setdefault("HitStunStart", hitstun_start(manifest, montage, set_start))
                fx = {k: (str(v) if isinstance(v, bool) else v) for k, v in fx.items()}
                w.writerow([action, f"{anim_path}/{montage}.{montage}", did, stamina.get(cost_id, 0), nxt, shape, rng, ang, cd,
                            fx.get("TargetStun", 0), fx.get("BuffSeconds", 0), fx.get("BuffDamageReduction", 0),
                            fx.get("BuffDamageBonus", 0),
                            *(fx.get(c, default(c)) for c in fx_cols)])
        print(f"{len(actions)} ações ({table_name})")
    print(f"{len(used)} danos em {out}")
    if missing:
        print(f"montagens ausentes no manifest: {missing}", file=sys.stderr)


if __name__ == "__main__":
    main()
